// SPDX-License-Identifier: MIT
#pragma once

#include <QSortFilterProxyModel>
#include <QStringList>
#include <QHash>
#include <QMetaObject>
#include <QList>

// Stable two-section presentation of the *same* KWin task model.
// Task close dispatch is queued with a persistent source index.
class DockSectionModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(bool pinnedSection READ pinnedSection WRITE setPinnedSection NOTIFY pinnedSectionChanged)
    Q_PROPERTY(bool showRunning READ showRunning WRITE setShowRunning NOTIFY showRunningChanged)
    Q_PROPERTY(QStringList launchers READ launchers WRITE setLaunchers NOTIFY launchersChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int mappingRevision READ mappingRevision NOTIFY mappingChanged)
    Q_PROPERTY(QStringList stackLaunchers READ stackLaunchers WRITE setStackLaunchers NOTIFY stackLaunchersChanged)
public:
    explicit DockSectionModel(QObject *parent = nullptr);
    void setSourceModel(QAbstractItemModel *model) override;
    bool pinnedSection() const { return m_pinnedSection; }
    bool showRunning() const { return m_showRunning; }
    QStringList launchers() const { return m_launchers; }
    QStringList stackLaunchers() const { return m_stackLaunchers; }
    int count() const { return rowCount(); }
    int mappingRevision() const { return m_mappingRevision; }
    void setPinnedSection(bool value);
    void setShowRunning(bool value);
    void setLaunchers(const QStringList &urls);
    void setStackLaunchers(const QStringList &urls);

    // Maps delegate row to the original TasksModel index; -1 means unavailable.
    Q_INVOKABLE int sourceRow(int row) const;
    // Resolve saved applications:/ and preferred:// pins against the live
    // KDE model without touching its launcherList or its row ordering.
    Q_INVOKABLE int sourceRowForLauncher(const QString &launcherUrl) const;
    // Visual-only reorder. Do not modify KDE's task list when a user drags
    // an unpinned running icon; that list is independent from our dock order.
    Q_INVOKABLE bool moveRunning(int sourceFromRow, int sourceToRow);
    Q_INVOKABLE void requestCloseLater(int sourceRow);

Q_SIGNALS:
    void pinnedSectionChanged();
    void showRunningChanged();
    void launchersChanged();
    void stackLaunchersChanged();
    void countChanged();
    void mappingChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    int roleNumber(const char *roleName) const;
    static QString normalized(const QString &url);
    QString stableIdentity(const QModelIndex &sourceIndex) const;
    int pinnedRank(const QModelIndex &sourceIndex) const;
    int launcherPositionForUrl(const QString &url) const;
    bool matchesLauncher(const QModelIndex &idx, const QString &launcherUrl) const;
    void captureRunningOrder();
    void scheduleRunningOrderCapture();
    QList<QMetaObject::Connection> m_sourceConnections;
    int m_mappingRevision = 0;
    bool m_runningCaptureScheduled = false;
    bool m_pinnedSection = true;
    bool m_showRunning = true;
    QStringList m_launchers;
    QStringList m_stackLaunchers;
    QStringList m_runningOrder;
};
