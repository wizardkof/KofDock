// SPDX-License-Identifier: MIT
#include "dockcontroller.h"
#include "docksettings.h"
#include "docksectionmodel.h"
#include "dockservices.h"
#include "dockautostart.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QFileInfo>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QUrl>
#include <QDebug>
#include <QTimer>
#include <QScreen>
#include <QVariantList>
#include <QVariantMap>
#include <QElapsedTimer>
#include <memory>
#include <vector>
#include <algorithm>

int main(int argc, char *argv[]) {
    // CLI mode requires no QtQuick compositor, display, or instance lock.
    // Installer calls this only after successful build and QML smoke-test.
    const QStringList startupFlags = [&]() {
        QStringList flags;
        for (int i = 1; i < argc; ++i) flags.append(QString::fromLocal8Bit(argv[i]));
        return flags;
    }();
    if (startupFlags.contains(QStringLiteral("--sync-autostart")) ||
        startupFlags.contains(QStringLiteral("--enable-autostart")) ||
        startupFlags.contains(QStringLiteral("--disable-autostart"))) {
        QCoreApplication cli(argc, argv);
        DockAutostart autostart;
        bool ok = false;
        if (startupFlags.contains(QStringLiteral("--enable-autostart")))
            ok = autostart.setEnabled(true);
        else if (startupFlags.contains(QStringLiteral("--disable-autostart")))
            ok = autostart.setEnabled(false);
        else
            ok = autostart.sync();
        return ok ? 0 : 6;
    }
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("KofDock"));
    app.setApplicationVersion(QStringLiteral("0.2.10-beta2"));
    app.setOrganizationName(QStringLiteral("KofDock"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.kof.KofDock"));
    app.setQuitOnLastWindowClosed(false);

    if (app.arguments().contains(QStringLiteral("--version"))) {
        qInfo().noquote() << "KofDock 0.2.10-beta2";
        return 0;
    }

    if (qEnvironmentVariable("XDG_SESSION_TYPE") != QStringLiteral("wayland")) {
        qCritical() << "KofDock requer uma sessão Wayland (KDE Plasma 6).";
        return 2;
    }

    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        qCritical() << "XDG_RUNTIME_DIR indisponível.";
        return 2;
    }
    // A non-visual QML smoke test may run while the installed dock is active.
    // It validates root component creation without taking the normal singleton lock.
    const bool checkQmlOnly = app.arguments().contains(QStringLiteral("--check-qml"));
    QLockFile singleInstance(runtime + QStringLiteral("/kofdock.lock"));
    singleInstance.setStaleLockTime(0);
    if (!checkQmlOnly && !singleInstance.tryLock(100)) {
        qWarning() << "Já existe uma instância do KofDock.";
        return 3;
    }


    qmlRegisterType<DockSectionModel>("KofDock", 1, 0, "DockSectionModel");
    DockSettings settings;
    DockAutostart autostart;
    DockController controller(&settings);
    // --screen primary ignores the saved HDMI-A-2 testing setting for this
    // invocation. --screen DP-3 selects an explicit connector, without writing
    // persistent settings or moving other applications' windows.
    const QStringList arguments = app.arguments();
    for (qsizetype i = 1; i < arguments.size(); ++i) {
        const QString arg = arguments.at(i);
        if (arg == QStringLiteral("--screen") && i + 1 < arguments.size())
            controller.setSessionScreenOverride(arguments.at(++i));
        else if (arg.startsWith(QStringLiteral("--screen=")))
            controller.setSessionScreenOverride(arg.mid(9));
    }
    DockServices dockServices;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("dockSettings"), &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("dockController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("dockServices"), &dockServices);
    engine.rootContext()->setContextProperty(QStringLiteral("dockAutostart"), &autostart);
    const QUrl mainUrl(QStringLiteral("qrc:/kofdock/qml/Main.qml"));
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Falha ao carregar interface. Verifique QtQuick, Kirigami e org.kde.taskmanager.";
        return 4;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) {
        qCritical() << "A interface principal não é uma QQuickWindow.";
        return 4;
    }
    // Root QML and its imported components have been instantiated successfully.
    // Keep the window unmapped; no user-facing dock or panel will be modified.
    if (checkQmlOnly) {
        // The empty fan used to pass the QML smoke check even if its repeated
        // leaves contained invalid runtime bindings. Populate a few synthetic
        // entries without mapping a window or launching any application.
        QObject *fan = window->findChild<QObject *>(QStringLiteral("kofdock-fan"));
        if (!fan) {
            qCritical() << "KofDock: componente FanStack ausente no teste QML";
            return 4;
        }
        QVariantList examples;
        for (int i = 0; i < 8; ++i) {
            QVariantMap entry;
            entry.insert(QStringLiteral("title"), QStringLiteral("Janela de teste %1").arg(i + 1));
            entry.insert(QStringLiteral("icon"), QStringLiteral("application-x-executable"));
            examples.append(entry);
        }
        if (!fan->setProperty("entries", examples) ||
            !fan->setProperty("open", true) ||
            !fan->setProperty("phase", 1.0)) {
            qCritical() << "KofDock: falha ao preparar FanStack no smoke test";
            return 4;
        }
        QCoreApplication::processEvents();
        if (fan->property("visibleRows").toInt() < 1) {
            qCritical() << "KofDock: o leque não criou itens durante o smoke test";
            return 4;
        }
        qInfo().noquote() << "KofDock: verificacao QML de inicializacao PASS (leque instanciado, Dock fechado).";
        return 0;
    }
    controller.attachWindow(window);
    if (controller.screenGeometry().isEmpty()) {
        qCritical() << "KofDock: falha na escolha do monitor; não mapeando superfície";
        return 5;
    }
    qInfo().noquote() << "KofDock: saídas=" << QGuiApplication::screens().size()
                      << "principal=" << (QGuiApplication::primaryScreen()
                           ? QGuiApplication::primaryScreen()->name() : QStringLiteral("indefinido"));
    if (app.arguments().contains(QStringLiteral("--diagnose-animation"))) {
        struct RenderSamples {
            QElapsedTimer clock;
            qint64 previousNs = -1;
            std::vector<double> elapsedMs;
            RenderSamples() { clock.start(); }
        };
        const auto metrics = std::make_shared<RenderSamples>();
        QObject::connect(window, &QQuickWindow::frameSwapped, window, [window, metrics]() {
            const qint64 now = metrics->clock.nsecsElapsed();
            if (metrics->previousNs >= 0) {
                const double frameMs = double(now - metrics->previousNs) / 1e6;
                // A long idle interval is not a dropped animation frame.
                if (frameMs >= 1.0 && frameMs <= 250.0)
                    metrics->elapsedMs.push_back(frameMs);
            }
            metrics->previousNs = now;
            if (metrics->elapsedMs.size() < 120) return;
            std::sort(metrics->elapsedMs.begin(), metrics->elapsedMs.end());
            const auto &samples = metrics->elapsedMs;
            const double screenHz = window->screen() ? window->screen()->refreshRate() : 60.0;
            const double budgetMs = 1000.0 / std::max(30.0, screenHz);
            const auto missed = std::count_if(samples.begin(), samples.end(),
                                              [budgetMs](double ms) { return ms > budgetMs * 1.6; });
            qInfo().noquote().nospace()
                << "KofDock animacao: tela=" << screenHz
                << "Hz, p50=" << samples[samples.size() / 2]
                << "ms, p95=" << samples[(samples.size() * 95) / 100]
                << "ms, intervalos_lentos=" << missed << "/" << samples.size()
                << " (medicao aproximada de frameSwapped)";
            metrics->elapsedMs.clear();
        });
        qInfo() << "KofDock diagnostico grafico ativo: mova o cursor sobre os icones por alguns segundos.";
    }
    qInfo().noquote() << "KofDock 0.2.10-beta2: sessão Wayland; superfície LayerShellQt preparada;"
                         "monitor=" << (window->screen() ? window->screen()->name() : QStringLiteral("indefinido"))
                      << "(os plugins QML podem ser carregados dinamicamente).";
    window->show();
    QTimer::singleShot(500, window, [window, &controller]() {
        qInfo().noquote() << "KofDock: monitor após mapeamento="
                          << (window->screen() ? window->screen()->name() : QStringLiteral("indefinido"))
                          << "área=" << controller.screenGeometry();
    });
    return app.exec();
}
