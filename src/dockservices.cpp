// SPDX-License-Identifier: MIT
#include "dockservices.h"
#include <QProcess>
#include <QDBusConnection>
#include <QDebug>
#include <QFileInfo>
#include <QDirIterator>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QStringList>
#include <QSet>
#include <algorithm>
#include <limits>

DockServices::DockServices(QObject *parent) : QObject(parent) {
    // This is the same protocol consumed by KDE's SmartLauncher backend.
    // We listen only; Plasma owns com.canonical.Unity itself.
    if (!QDBusConnection::sessionBus().connect(
            QString(), QString(), QStringLiteral("com.canonical.Unity.LauncherEntry"),
            QStringLiteral("Update"), this,
            SLOT(onUnityUpdate(QString,QMap<QString,QVariant>)))) {
        qWarning() << "KofDock: não foi possível observar os contadores via D-Bus";
    }
}

QString DockServices::normalId(const QString &input) {
    QString out = input.section(QLatin1Char('?'), 0, 0).trimmed();
    if (out.startsWith(QLatin1String("application://"), Qt::CaseInsensitive)) {
        out = out.mid(QStringLiteral("application://").size());
    } else if (out.startsWith(QLatin1String("applications:"), Qt::CaseInsensitive)) {
        out = out.mid(QStringLiteral("applications:").size());
    } else if (out.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
        out = QUrl(out).toLocalFile();
    }
    out = QFileInfo(out).fileName().toLower();
    if (!out.isEmpty() && !out.endsWith(QLatin1String(".desktop"))) out += QStringLiteral(".desktop");
    return out;
}

void DockServices::onUnityUpdate(const QString &applicationUri, const QMap<QString, QVariant> &properties) {
    const QString key = normalId(applicationUri);
    if (key.isEmpty()) return;
    Entry state = m_badges.value(key);
    if (properties.contains(QStringLiteral("count")))
        state.count = int(std::clamp(properties.value(QStringLiteral("count")).toLongLong(),
                                     qint64(0), qint64(999999)));
    if (properties.contains(QStringLiteral("count-visible")))
        state.visible = properties.value(QStringLiteral("count-visible")).toBool();
    const Entry before = m_badges.value(key);
    if (before.count == state.count && before.visible == state.visible) return;
    m_badges.insert(key, state);
    ++m_revision;
    emit badgesChanged();
}

int DockServices::badgeCount(const QString &launcherUrl, const QString &appId, const QString &storageId) const {
    // KDE can map an application .desktop ID to a different Unity Launcher ID.
    // Use the same user-owned rules file instead of assuming they are identical.
    const QString configFile = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                               + QStringLiteral("/taskmanagerrulesrc");
    QSettings mapping(configFile, QSettings::IniFormat);
    for (const auto &raw : {storageId, launcherUrl, appId}) {
        const QString key = normalId(raw);
        if (key.isEmpty()) continue;
        const QString alias = mapping.value(QStringLiteral("Unity Launcher Mapping/") + key).toString();
        for (const QString &candidate : {key, normalId(alias)}) {
            const auto entry = m_badges.constFind(candidate);
            if (entry != m_badges.cend()) return entry->visible ? entry->count : 0;
        }
    }
    return 0;
}

namespace {
QStringList desktopCandidates(const QString &desktopId) {
    if (desktopId.isEmpty()) return {};
    QStringList candidates{desktopId};
    // Some KDE tasks advertise org.kde.systemsettings.desktop although the
    // distro ships systemsettings.desktop. Resolve the plain .desktop variant
    // before showing a missing-document icon.
    const QString base = desktopId.endsWith(QLatin1String(".desktop"))
        ? desktopId.left(desktopId.size() - 8) : desktopId;
    if (base.contains(QLatin1Char('.'))) {
        const QString shortened = base.section(QLatin1Char('.'), -1) + QStringLiteral(".desktop");
        if (!candidates.contains(shortened)) candidates.append(shortened);
    }
    return candidates;
}

QString findDesktopFile(const QString &desktopId) {
    static QHash<QString, QString> cache;
    if (cache.contains(desktopId)) return cache.value(desktopId);
    const QStringList candidates = desktopCandidates(desktopId);
    const QStringList roots = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &root : roots) {
        for (const QString &candidate : candidates) {
            const QString direct = QDir(root).filePath(candidate);
            if (QFileInfo(direct).isFile()) {
                cache.insert(desktopId, direct);
                return direct;
            }
        }
    }
    // Index nested .desktop files only once. Doing a recursive scan for every
    // initially missing app icon would block the animation thread repeatedly.
    static bool indexReady = false;
    static QHash<QString, QString> nestedIndex;
    if (!indexReady) {
        int scanned = 0;
        for (const QString &root : roots) {
            QDirIterator files(root, {QStringLiteral("*.desktop")},
                               QDir::Files, QDirIterator::Subdirectories);
            while (files.hasNext() && scanned < 20000) {
                const QString path = files.next();
                ++scanned;
                const QString key = QFileInfo(path).fileName().toLower();
                if (!nestedIndex.contains(key)) nestedIndex.insert(key, path);
            }
            if (scanned >= 20000) break;
        }
        indexReady = true;
    }
    for (const QString &candidate : candidates) {
        const QString key = candidate.toLower();
        if (nestedIndex.contains(key)) {
            const QString path = nestedIndex.value(key);
            cache.insert(desktopId, path);
            return path;
        }
    }
    cache.insert(desktopId, QString());
    return {};
}
}

QString DockServices::desktopIcon(const QString &desktopId) {
    const QString file = findDesktopFile(desktopId);
    if (file.isEmpty()) return {};
    QSettings desktopFile(file, QSettings::IniFormat);
    return desktopFile.value(QStringLiteral("Desktop Entry/Icon")).toString().trimmed();
}

QStringList DockServices::plasmaPinnedLaunchers() const {
    // Read-only import of pinned launchers from Plasma's task-manager config.
    // This is an explicit user action; never edit or restart plasmashell.
    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/plasma-org.kde.plasma.desktop-appletsrc");
    QFile file(config);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QStringList collected;
    bool inGeneral = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inGeneral = line.contains(QStringLiteral("[Applets]")) &&
                        line.endsWith(QStringLiteral("[Configuration][General]"));
            continue;
        }
        if (!inGeneral || !line.startsWith(QStringLiteral("launchers="))) continue;
        const QStringList entries = line.mid(QStringLiteral("launchers=").size())
                                        .split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString entry : entries) {
            entry = entry.trimmed();
            if (!entry.startsWith(QStringLiteral("applications:")) &&
                !entry.startsWith(QStringLiteral("preferred://")) &&
                !entry.startsWith(QStringLiteral("file://"))) continue;
            if (!collected.contains(entry)) collected.append(entry);
        }
    }
    return collected;
}

QString DockServices::displayName(const QString &launcherUrl, const QString &appId,
                                   const QString &storageId, const QString &fallback) const {
    if (!fallback.isEmpty() && !fallback.endsWith(QLatin1String(".desktop"))) return fallback;
    const auto locale = QLocale::system().name();
    for (const QString &raw : {storageId, launcherUrl, appId}) {
        const QString fullName = findDesktopFile(normalId(raw));
        if (fullName.isEmpty()) continue;
        QSettings desktop(fullName, QSettings::IniFormat);
        for (const QString &key : {
                QStringLiteral("Desktop Entry/Name[") + locale + QLatin1Char(']'),
                QStringLiteral("Desktop Entry/Name[") + locale.section(QLatin1Char('_'), 0, 0) + QLatin1Char(']'),
                QStringLiteral("Desktop Entry/Name")}) {
            const QString readable = desktop.value(key).toString();
            if (!readable.isEmpty()) return readable;
        }
    }
    return fallback.isEmpty() ? QStringLiteral("Aplicativo") : fallback;
}

QVariant DockServices::iconFor(const QString &launcherUrl, const QString &appId,
                               const QString &storageId, const QVariant &providedIcon) const {
    for (const QString &raw : {storageId, launcherUrl, appId}) {
        const QString iconName = desktopIcon(normalId(raw));
        if (iconName.isEmpty()) continue;
        if (QFileInfo::exists(iconName)) return QVariant::fromValue(QIcon(iconName));
        if (QIcon::hasThemeIcon(iconName)) return QVariant::fromValue(QIcon::fromTheme(iconName));
    }
    // KWin's actual decoration can be an absolute-path or dynamically generated
    // QIcon. Preserve it when the .desktop lookup cannot find an icon.
    if (providedIcon.isValid() && !providedIcon.isNull()) {
        if (providedIcon.canConvert<QIcon>() && !providedIcon.value<QIcon>().isNull())
            return providedIcon;
        if (providedIcon.canConvert<QString>()) {
            const QString iconName = providedIcon.toString();
            if (QFileInfo::exists(iconName)) return QVariant::fromValue(QIcon(iconName));
            if (QIcon::hasThemeIcon(iconName)) return QVariant::fromValue(QIcon::fromTheme(iconName));
        }
    }
    for (const QString &raw : {appId, storageId, launcherUrl}) {
        for (const QString &desktopId : desktopCandidates(normalId(raw))) {
            const QString themeName = desktopId.left(desktopId.size() - 8);
            if (QIcon::hasThemeIcon(themeName))
                return QVariant::fromValue(QIcon::fromTheme(themeName));
        }
    }
    // Warn once per unrecognized ID so an empty icon is diagnosable without
    // flooding the console while the task model changes.
    static QSet<QString> warned;
    const QString id = normalId(!storageId.isEmpty() ? storageId :
                                (!launcherUrl.isEmpty() ? launcherUrl : appId));
    if (!id.isEmpty() && !warned.contains(id)) {
        warned.insert(id);
        qWarning().noquote() << "KofDock: não encontrei ícone para" << id
                             << "(verifique o arquivo .desktop e o tema de ícones)";
    }
    return QVariant::fromValue(QIcon::fromTheme(QStringLiteral("application-x-executable")));
}

// The .desktop alias differs among distributions. Launch the actual KDE
// System Settings executable when a stale pinned alias fails to resolve.
bool DockServices::launchSystemSettings() const {
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("systemsettings"));
    if (executable.isEmpty()) {
        qWarning() << "KofDock: executable 'systemsettings' não encontrado no PATH";
        return false;
    }
    const bool started = QProcess::startDetached(executable, QStringList{});
    if (!started) qWarning() << "KofDock: não foi possível iniciar" << executable;
    return started;
}

// Pinned icons are now owned by the Dock, not by a mutable KDE TasksModel.
// Launch an installed desktop entry through KDE's application URL handler.
// No shell interpolation or arbitrary Exec-line parsing is performed here.
bool DockServices::launchLauncher(const QString &url) const {
    const QUrl launcher(url);
    const QString scheme = launcher.scheme().toLower();
    if (scheme != QStringLiteral("applications") &&
        scheme != QStringLiteral("preferred") &&
        scheme != QStringLiteral("file")) {
        qWarning().noquote() << "KofDock: esquema de lançador não permitido:" << url;
        return false;
    }
    const QString kioclient = QStandardPaths::findExecutable(QStringLiteral("kioclient6"));
    if (!kioclient.isEmpty() && QProcess::startDetached(kioclient, {QStringLiteral("exec"), url}))
        return true;
    if (scheme == QStringLiteral("applications")) {
        const QString gtk = QStandardPaths::findExecutable(QStringLiteral("gtk-launch"));
        QString desktop = launcher.path();
        while (desktop.startsWith(QLatin1Char('/'))) desktop.remove(0, 1);
        if (!gtk.isEmpty() && !desktop.isEmpty() && QProcess::startDetached(gtk, {desktop}))
            return true;
    }
    qWarning().noquote() << "KofDock: lançamento não disponível para" << url
                         << "(instale kioclient6 ou corrija o arquivo .desktop)";
    return false;
}
