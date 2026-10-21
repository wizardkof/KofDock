// SPDX-License-Identifier: MIT
#include "dockautostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QString readText(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(file.readAll());
}
}

class DockAutostartTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void freshInstallationIsOnByDefault() {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString entry = home.filePath("config/autostart/org.kof.KofDock.desktop");
        const QString app = home.filePath(".local/bin/kofdock");
        const QString preferences = home.filePath("settings/KofDock.ini");
        DockAutostart startup(entry, app, preferences);
        QVERIFY(startup.desiredEnabled());
        QVERIFY(!startup.enabled());
        QVERIFY(startup.sync());
        QVERIFY(startup.enabled());
        QVERIFY(readText(entry).contains(QStringLiteral("OnlyShowIn=KDE;")));
        QVERIFY(readText(entry).contains(QStringLiteral("Exec=\"") + app + QLatin1Char('"')));
        QVERIFY(!readText(entry).contains(QStringLiteral("X-KDE-Wayland-Interfaces=")));
        // Running install a second time doesn't add another entry.
        QVERIFY(startup.sync());
        QDir autostartDir(QFileInfo(entry).absolutePath());
        QCOMPARE(autostartDir.entryList(QDir::Files | QDir::NoDotAndDotDot).size(), 1);
    }

    void togglingOffRemovesOnlyAutostartAndSurvivesReinstall() {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString entry = home.filePath("config/autostart/org.kof.KofDock.desktop");
        const QString desktopPermission = home.filePath("applications/org.kof.KofDock.desktop");
        const QString preferences = home.filePath("settings/KofDock.ini");
        QVERIFY(QDir().mkpath(QFileInfo(desktopPermission).absolutePath()));
        QFile permission(desktopPermission);
        QVERIFY(permission.open(QIODevice::WriteOnly));
        QCOMPARE(permission.write("X-KDE-Wayland-Interfaces=org_kde_plasma_window_management\n"),
                 qint64(sizeof("X-KDE-Wayland-Interfaces=org_kde_plasma_window_management\n") - 1));
        permission.close();
        {
            DockAutostart startup(entry, home.filePath(".local/bin/kofdock"), preferences);
            QVERIFY(startup.sync());
            QVERIFY(startup.setEnabled(false));
            QVERIFY(!startup.enabled());
            QVERIFY(!QFile::exists(entry));
            QVERIFY(QFile::exists(desktopPermission));
        }
        {
            // Reinstalling without --autostart MUST NOT silently enable it.
            DockAutostart reinstalled(entry, home.filePath(".local/bin/kofdock"), preferences);
            QVERIFY(!reinstalled.desiredEnabled());
            QVERIFY(reinstalled.sync());
            QVERIFY(!reinstalled.enabled());
            QVERIFY(!QFile::exists(entry));
            QVERIFY(QFile::exists(desktopPermission));
            QVERIFY(reinstalled.setEnabled(true));
            QVERIFY(reinstalled.enabled());
        }
        QSettings saved(preferences, QSettings::IniFormat);
        QCOMPARE(saved.value("startOnLogin").toBool(), true);
    }

    void disabledPreferenceRemovesOldCopiedAutostart() {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString entry = home.filePath("config/autostart/org.kof.KofDock.desktop");
        const QString preferences = home.filePath("settings/KofDock.ini");
        DockAutostart startup(entry, home.filePath(".local/bin/kofdock"), preferences);
        QVERIFY(startup.setEnabled(false));
        QVERIFY(QDir().mkpath(QFileInfo(entry).absolutePath()));
        QFile previous(entry);
        QVERIFY(previous.open(QIODevice::WriteOnly));
        previous.write("[Desktop Entry]\nExec=/home/someone/kofdock\n");
        previous.close();
        QVERIFY(startup.sync());
        QVERIFY(!QFile::exists(entry));
    }

    void entryHasEscapedExecutableWithSpaces() {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString entry = home.filePath("config/autostart/org.kof.KofDock.desktop");
        const QString path = home.filePath("my apps/kofdock");
        DockAutostart startup(entry, path, home.filePath("settings/KofDock.ini"));
        QVERIFY(startup.setEnabled(true));
        QVERIFY(readText(entry).contains(QStringLiteral("Exec=\"") + path + QLatin1Char('"')));
        QCOMPARE(startup.lastError(), QString());
    }

    void fileFailureDoesNotPersistEnabled() {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString blocker = home.filePath("blocker");
        QFile file(blocker);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a directory");
        file.close();
        const QString entry = blocker + QStringLiteral("/autostart/org.kof.KofDock.desktop");
        const QString preferences = home.filePath("settings/KofDock.ini");
        DockAutostart startup(entry, home.filePath(".local/bin/kofdock"), preferences);
        QVERIFY(!startup.setEnabled(true));
        QVERIFY(!startup.enabled());
        QVERIFY(!startup.lastError().isEmpty());
        QVERIFY(startup.desiredEnabled()); // default, not a false success
        QSettings persisted(preferences, QSettings::IniFormat);
        QVERIFY(!persisted.contains("startOnLogin"));
    }
};

QTEST_GUILESS_MAIN(DockAutostartTests)
#include "test_dockautostart.moc"
