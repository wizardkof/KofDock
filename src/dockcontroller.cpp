// SPDX-License-Identifier: MIT
#include "dockcontroller.h"
#include "docksettings.h"
#include "screenrouting.h"

#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QRegion>
#include <QScreen>
#include <QRectF>
#include <QDebug>
#include <QTimer>

DockController::DockController(DockSettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
    connect(settings, &DockSettings::screenNameChanged, this, [this]() {
        // A choice made in Settings takes precedence over the command-line
        // selection used to start this session.
        if (!m_sessionScreenOverride.isEmpty()) {
            qInfo().noquote() << "KofDock: escolha nas preferências substituiu --screen"
                              << m_sessionScreenOverride;
            m_sessionScreenOverride.clear();
        }
        chooseScreen();
    });
    auto *guiApp = static_cast<QGuiApplication *>(QCoreApplication::instance());
    connect(guiApp, &QGuiApplication::screenAdded, this, [this](QScreen *) {
        emit screenNamesChanged(); chooseScreen();
    });
    connect(guiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
        emit screenNamesChanged(); chooseScreen();
    });
    connect(guiApp, &QGuiApplication::primaryScreenChanged, this, [this](QScreen *) {
        if (m_sessionScreenOverride.isEmpty() && m_settings->screenName().isEmpty()) chooseScreen();
    });
}

QRect DockController::screenGeometry() const {
    // Use the selected output, not QWindow::screen(), which can temporarily
    // report the previous output while a Wayland layer surface is remapped.
    return m_selectedScreen ? m_selectedScreen->geometry() : QRect();
}

QString DockController::targetScreenName() const {
    return m_selectedScreen ? m_selectedScreen->name() : QStringLiteral("indefinido");
}

void DockController::selectScreen(const QString &name) {
    // Explicit GUI selection always wins, even if the stored value is unchanged.
    const QString selected = name.trimmed();
    if (!selected.isEmpty() && selected != QStringLiteral("primary")) {
        bool found = false;
        for (const auto *output : QGuiApplication::screens()) {
            if (output->name() == selected) { found = true; break; }
        }
        if (!found) {
            qWarning().noquote() << "KofDock: a escolha não está mais conectada:" << selected;
            return;
        }
    }
    m_sessionScreenOverride.clear();
    if (m_settings->screenName() != selected) {
        m_settings->setScreenName(selected);
    } else {
        chooseScreen();
    }
}

void DockController::watchScreenGeometry(QScreen *screen) {
    if (m_trackedScreen == screen) return;
    if (m_screenGeometryConnection) QObject::disconnect(m_screenGeometryConnection);
    m_trackedScreen = screen;
    if (screen) {
        m_screenGeometryConnection = connect(screen, &QScreen::geometryChanged, this,
                                             [this](const QRect &) { emit screenGeometryChanged(); });
    }
    emit screenGeometryChanged();
}

QStringList DockController::screenNames() const {
    QStringList out{QStringLiteral("Principal (automático)")};
    for (const auto *screen : QGuiApplication::screens()) out << screen->name();
    return out;
}

void DockController::attachWindow(QQuickWindow *window) {
    m_window = window;
    m_maskInitialized = false;
    window->setColor(Qt::transparent);
    // Screen MUST be selected before LayerShellQt::Window::get() creates
    // a shell integration. The wl_output is fixed by get_layer_surface().
    chooseScreen();
    if (!m_selectedScreen) {
        qCritical() << "KofDock: nenhum monitor válido para a superfície";
        return;
    }
    // Defensive: if QML/Qt already created a native surface, recreate it
    // with the chosen output BEFORE attaching LayerShellQt.
    if (window->handle()) {
        qWarning() << "KofDock: descartando superfície nativa prematura antes do LayerShell";
        window->destroy();
        window->setScreen(m_selectedScreen);
    }
    configureLayer();
    LayerShellQt::Window::get(window)->setScreen(m_selectedScreen);
    connect(window, &QWindow::screenChanged, this, [this](QScreen *reported) {
        // A Qt screenChanged notification is not proof that wl_output has moved.
        if (reported && m_selectedScreen && reported != m_selectedScreen && !m_remapping) {
            qWarning().noquote() << "KofDock: QWindow reporta" << reported->name()
                << "mas a superfície solicitou" << m_selectedScreen->name();
        }
    });
}

void DockController::configureLayer() {
    if (!m_window) return;
    auto *surface = LayerShellQt::Window::get(m_window);
    // The surface is not given a panel shadow by Plasma; only our QML draws one.
    surface->setScope(QStringLiteral("kofdock"));
    LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorBottom;
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;
    surface->setAnchors(anchors);
    surface->setLayer(LayerShellQt::Window::LayerTop);
    surface->setExclusiveZone(0);
    surface->setMargins(QMargins(0, 0, 0, 0));
    surface->setActivateOnShow(false);
    surface->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    surface->setCloseOnDismissed(false);
}

void DockController::setSessionScreenOverride(const QString &name) {
    m_sessionScreenOverride = name;
    if (m_window) chooseScreen();
}

void DockController::chooseScreen() {
    if (!m_window) return;
    QStringList names;
    for (const auto *output : QGuiApplication::screens()) names << output->name();
    const auto *primary = QGuiApplication::primaryScreen();
    // The user's preferred primary is DP-3 even when Qt reports HDMI-A-2.
    const QString targetName = ScreenRouting::choose(
        names, primary ? primary->name() : QString(),
        m_settings->screenName(), m_sessionScreenOverride, QStringLiteral("DP-3"));
    if (targetName.isEmpty()) {
        qWarning().noquote() << "KofDock: monitor solicitado não encontrado:"
                              << (m_sessionScreenOverride.isEmpty()
                                  ? m_settings->screenName() : m_sessionScreenOverride)
                              << "; nenhuma troca foi realizada. Saídas disponíveis:"
                              << names.join(QStringLiteral(", "));
        return;
    }
    QScreen *screen = nullptr;
    for (auto *output : QGuiApplication::screens()) {
        if (output->name() == targetName) { screen = output; break; }
    }
    if (!screen) return;
    if (!m_window->isVisible()) {
        // On initial startup this runs BEFORE configureLayer() and BEFORE
        // the Wayland layer-surface is mapped.
        const bool changed = m_selectedScreen != screen;
        m_selectedScreen = screen;
        m_window->setScreen(screen);
        watchScreenGeometry(screen);
        if (changed) emit targetScreenChanged();
        qInfo().noquote() << "KofDock: saída escolhida antes de criar o LayerShell="
                          << screen->name() << "geometria=" << screen->geometry()
                          << "preferência=" << m_settings->screenName()
                          << "override=" << m_sessionScreenOverride;
        return;
    }
    if (screen == m_selectedScreen) return;
    scheduleRemap(screen);
}

void DockController::scheduleRemap(QScreen *target) {
    m_pendingScreen = target;
    if (m_remapScheduled || m_remapping || !m_window) return;
    m_remapScheduled = true;
    // Never destroy a QQuickWindow's native surface from the ComboBox's
    // onActivated/mouse event handler. Defer until the event is finished.
    QTimer::singleShot(0, this, [this]() {
        m_remapScheduled = false;
        remapToPendingScreen();
    });
}

void DockController::remapToPendingScreen() {
    if (!m_window || m_remapping) return;
    QScreen *target = m_pendingScreen;
    m_pendingScreen.clear();
    if (!target || target == m_selectedScreen) return;

    m_remapping = true;
    // LayerShellQt passes wl_output only at get_layer_surface() creation;
    // changing Window::setScreen on an existing role cannot move it.
    // Re-create only the KofDock window's native wl_surface. The QML object,
    // favorites, settings and KWin panel remain intact.
    qInfo().noquote() << "KofDock: recriando superfície LayerShellQt:"
                      << targetScreenName() << "->" << target->name();
    m_window->hide();
    m_window->destroy();
    m_window->setScreen(target);
    auto *surface = LayerShellQt::Window::get(m_window);
    surface->setScreen(target);
    m_selectedScreen = target;
    watchScreenGeometry(target);
    m_maskInitialized = false;
    m_previousMask = QRegion();
    emit targetScreenChanged();
    emit screenGeometryChanged();
    m_window->show();
    m_remapping = false;
    qInfo().noquote() << "KofDock: superfície recriada para saída="
                      << target->name() << "geometria=" << target->geometry();
    QTimer::singleShot(450, this, [this, requested = QPointer<QScreen>(target)]() {
        if (!m_window || !requested) return;
        qInfo().noquote() << "KofDock: verificação pós-mapeamento: solicitada="
                          << requested->name() << "Qt="
                          << (m_window->screen() ? m_window->screen()->name() : QStringLiteral("indefinido"));
    });
    if (m_pendingScreen && m_pendingScreen != m_selectedScreen)
        scheduleRemap(m_pendingScreen);
}

void DockController::applyMaskIfChanged(const QRegion &mask) {
    if (!m_window) return;
    if (m_maskInitialized && m_previousMask == mask) return;
    m_previousMask = mask;
    m_maskInitialized = true;
    m_window->setMask(mask);
}

void DockController::setInputArea(int x, int y, int width, int height) {
    if (!m_window) return;
    const QRect windowBounds(QPoint(0, 0), m_window->size());
    const QRect area = QRect(x, y, qMax(0, width), qMax(0, height)).intersected(windowBounds);
    if (area.isValid()) applyMaskIfChanged(QRegion(area));
}

void DockController::setInputRegions(const QVariantList &rectangles) {
    if (!m_window) return;
    const QRect windowBounds(QPoint(0, 0), m_window->size());
    QRegion regions;
    for (const QVariant &entry : rectangles) {
        // Qt.rect from QML arrives as QRectF, but QRect is supported too.
        const QRect rect = entry.toRectF().toAlignedRect().intersected(windowBounds);
        if (rect.isValid() && !rect.isEmpty()) regions += QRegion(rect);
    }
    // Empty masks remove the restriction in Qt. Keep a one-pixel fallback.
    if (regions.isEmpty()) regions = QRegion(QRect(0, m_window->height() - 1, 1, 1));
    applyMaskIfChanged(regions);
}

void DockController::resetInputArea() {
    if (m_window) applyMaskIfChanged(QRegion(QRect(QPoint(0,0), m_window->size())));
}
