// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QVariant>
#include <QHash>
#include <QMap>
#include <QStringList>
#include <QIcon>

// Passive KDE Unity LauncherEntry observer: does not claim the D-Bus service
// already owned by Plasma. Never fabricates notification counts.
class DockServices : public QObject {
    Q_OBJECT
    Q_PROPERTY(int badgeRevision READ badgeRevision NOTIFY badgesChanged)
public:
    explicit DockServices(QObject *parent = nullptr);
    int badgeRevision() const { return m_revision; }
    Q_INVOKABLE int badgeCount(const QString &launcherUrl, const QString &appId, const QString &storageId) const;
    Q_INVOKABLE QStringList plasmaPinnedLaunchers() const;
    Q_INVOKABLE bool launchSystemSettings() const;
    Q_INVOKABLE bool launchLauncher(const QString &url) const;
    Q_INVOKABLE QString displayName(const QString &launcherUrl, const QString &appId,
                                    const QString &storageId, const QString &fallback) const;
    Q_INVOKABLE QVariant iconFor(const QString &launcherUrl, const QString &appId,
                                  const QString &storageId, const QVariant &providedIcon) const;
Q_SIGNALS:
    void badgesChanged();
private Q_SLOTS:
    void onUnityUpdate(const QString &applicationUri, const QMap<QString, QVariant> &properties);
private:
    struct Entry { int count = 0; bool visible = false; };
    static QString normalId(const QString &id);
    static QString desktopIcon(const QString &desktopId);
    QHash<QString, Entry> m_badges;
    int m_revision = 0;
};
