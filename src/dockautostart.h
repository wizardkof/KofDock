// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <memory>

// A user-session autostart entry. This never creates a system service and never
// changes the .desktop file that grants KWin's window-management interface.
class DockAutostart final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit DockAutostart(QObject *parent = nullptr);
    // Explicit paths make filesystem and persistence behaviour testable without
    // writing into the real user's ~/.config/autostart or settings directory.
    DockAutostart(QString entryPath, QString executablePath, QString settingsFile,
                  QObject *parent = nullptr);

    bool enabled() const;
    QString lastError() const;
    bool desiredEnabled() const;

    Q_INVOKABLE bool setEnabled(bool enabled);
    // Reconcile the actual .desktop file with persisted preference. A fresh
    // installation defaults to ON; an explicit OFF survives reinstallations.
    bool sync();

Q_SIGNALS:
    void enabledChanged();
    void lastErrorChanged();

private:
    bool applyFileState(bool desired);
    bool writeEntry();
    bool removeEntry();
    bool fail(const QString &message);
    void clearError();

    QString m_entryPath;
    QString m_executablePath;
    std::unique_ptr<QSettings> m_preferences;
    QString m_lastError;
};
