// SPDX-License-Identifier: MIT
#include "dockautostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDebug>
#include <utility>

namespace {
QString defaultEntryPath() {
    QString configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configHome.isEmpty()) configHome = QDir::homePath() + QStringLiteral("/.config");
    return QDir(configHome).filePath(QStringLiteral("autostart/org.kof.KofDock.desktop"));
}
QString defaultExecutablePath() {
    return QDir::home().filePath(QStringLiteral(".local/bin/kofdock"));
}
QString desktopExec(QString path) {
    // Desktop Entry specification: escape percent field codes and quote the
    // executable path so HOME paths containing whitespace are not truncated.
    path.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    path.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    path.replace(QLatin1Char('%'), QStringLiteral("%%"));
    return QStringLiteral("\"") + path + QStringLiteral("\"");
}
} // namespace

DockAutostart::DockAutostart(QObject *parent)
    : QObject(parent)
    , m_entryPath(defaultEntryPath())
    , m_executablePath(defaultExecutablePath())
    , m_preferences(std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
                                                QStringLiteral("KofDock"), QStringLiteral("KofDock"))) {}

DockAutostart::DockAutostart(QString entryPath, QString executablePath, QString settingsFile,
                             QObject *parent)
    : QObject(parent)
    , m_entryPath(std::move(entryPath))
    , m_executablePath(std::move(executablePath))
    , m_preferences(std::make_unique<QSettings>(settingsFile, QSettings::IniFormat)) {}

bool DockAutostart::enabled() const {
    // Report the state that the desktop session will actually see, rather than
    // claiming ON if the file was deleted outside the application.
    return QFileInfo::exists(m_entryPath);
}

bool DockAutostart::desiredEnabled() const {
    return m_preferences->value(QStringLiteral("startOnLogin"), true).toBool();
}

QString DockAutostart::lastError() const { return m_lastError; }

bool DockAutostart::fail(const QString &message) {
    if (m_lastError != message) {
        m_lastError = message;
        Q_EMIT lastErrorChanged();
    }
    qWarning().noquote() << "KofDock: inicialização automática:" << message;
    Q_EMIT enabledChanged(); // revert a user-toggled Switch on failure
    return false;
}

void DockAutostart::clearError() {
    if (m_lastError.isEmpty()) return;
    m_lastError.clear();
    Q_EMIT lastErrorChanged();
}

bool DockAutostart::writeEntry() {
    const QString parent = QFileInfo(m_entryPath).absolutePath();
    if (!QDir().mkpath(parent)) {
        return fail(QStringLiteral("não foi possível criar a pasta: ") + parent);
    }
    const QByteArray content = (QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=KofDock\n"
        "Comment=Iniciar KofDock ao entrar na sessão KDE Plasma\n"
        "Exec=%1\n"
        "Icon=preferences-desktop-icons\n"
        "Terminal=false\n"
        "OnlyShowIn=KDE;\n"
        "StartupNotify=false\n")
        .arg(desktopExec(m_executablePath))).toUtf8();

    QFile current(m_entryPath);
    if (current.open(QIODevice::ReadOnly)) {
        if (current.readAll() == content) return true; // no unnecessary writes
        current.close();
    }

    QSaveFile out(m_entryPath);
    if (!out.open(QIODevice::WriteOnly)) {
        return fail(QStringLiteral("não foi possível gravar ") + m_entryPath +
                    QStringLiteral(": ") + out.errorString());
    }
    if (out.write(content) != content.size()) {
        return fail(QStringLiteral("erro ao escrever a entrada de inicialização: ") + out.errorString());
    }
    if (!out.commit()) {
        return fail(QStringLiteral("erro ao salvar a entrada de inicialização: ") + out.errorString());
    }
    return true;
}

bool DockAutostart::removeEntry() {
    if (!QFile::exists(m_entryPath) && !QFileInfo(m_entryPath).isSymLink()) return true;
    if (!QFile::remove(m_entryPath)) {
        return fail(QStringLiteral("não foi possível remover a entrada: ") + m_entryPath);
    }
    return true;
}

bool DockAutostart::applyFileState(bool desired) {
    return desired ? writeEntry() : removeEntry();
}

bool DockAutostart::setEnabled(bool desired) {
    const bool oldDesired = desiredEnabled();
    const bool oldActual = enabled();
    if (!applyFileState(desired)) return false;

    const QString preferencesDirectory = QFileInfo(m_preferences->fileName()).absolutePath();
    if (!QDir().mkpath(preferencesDirectory)) {
        applyFileState(oldActual);
        return fail(QStringLiteral("não foi possível criar a pasta de preferências: ") +
                    preferencesDirectory);
    }
    m_preferences->setValue(QStringLiteral("startOnLogin"), desired);
    m_preferences->sync();
    if (m_preferences->status() != QSettings::NoError) {
        // Prefer leaving file + preferences consistent with the old choice.
        m_preferences->setValue(QStringLiteral("startOnLogin"), oldDesired);
        m_preferences->sync();
        applyFileState(oldActual);
        return fail(QStringLiteral("não foi possível salvar a preferência de inicialização."));
    }
    clearError();
    if (oldActual != enabled() || oldDesired != desired) Q_EMIT enabledChanged();
    qInfo().noquote() << "KofDock: inicialização automática"
                      << (desired ? "ativada" : "desativada") << "arquivo=" << m_entryPath;
    return true;
}

bool DockAutostart::sync() {
    // Installer and session use the same authority. A saved OFF must never be
    // silently reversed by an update or a manual program launch.
    const bool before = enabled();
    if (!applyFileState(desiredEnabled())) return false;
    clearError();
    if (before != enabled()) Q_EMIT enabledChanged();
    qInfo().noquote() << "KofDock: inicialização automática"
                      << (enabled() ? "ativada" : "desativada")
                      << "arquivo=" << m_entryPath;
    return true;
}
