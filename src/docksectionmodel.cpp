// SPDX-License-Identifier: MIT
#include "docksectionmodel.h"

#include <QAbstractItemModel>
#include <QByteArray>
#include <QUrl>
#include <QMetaObject>
#include <QTimer>
#include <QPersistentModelIndex>
#include <QPointer>
#include <limits>

DockSectionModel::DockSectionModel(QObject *parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
    // Sort by stable dock presentation order, never by KDE source row.
    sort(0);
    // QML's Repeater uses rowCount to build sections; expose change notifications.
    connect(this, &QAbstractItemModel::rowsInserted, this, [this] { emit countChanged(); });
    connect(this, &QAbstractItemModel::rowsRemoved, this, [this] { emit countChanged(); });
    connect(this, &QAbstractItemModel::modelReset, this, [this] { emit countChanged(); });
    connect(this, &QAbstractItemModel::layoutChanged, this, [this] { emit countChanged(); });
}

void DockSectionModel::setSourceModel(QAbstractItemModel *model) {
    // Disconnect only our subscriptions; disconnect(source, nullptr, this, nullptr)
    // also removes QSortFilterProxyModel's internal source-model connections.
    for (const auto &connection : m_sourceConnections) QObject::disconnect(connection);
    m_sourceConnections.clear();
    m_runningCaptureScheduled = false;
    QSortFilterProxyModel::setSourceModel(model);
    m_runningOrder.clear();
    if (model) {
        auto schedule = [this]() { scheduleRunningOrderCapture(); };
        m_sourceConnections << connect(model, &QAbstractItemModel::rowsInserted, this, schedule);
        m_sourceConnections << connect(model, &QAbstractItemModel::rowsRemoved, this, schedule);
        m_sourceConnections << connect(model, &QAbstractItemModel::rowsMoved, this, schedule);
        m_sourceConnections << connect(model, &QAbstractItemModel::dataChanged, this, schedule);
        m_sourceConnections << connect(model, &QAbstractItemModel::modelReset, this, schedule);
        m_sourceConnections << connect(model, &QAbstractItemModel::layoutChanged, this, schedule);
    }
    // The initial source model is fully constructed; later changes are coalesced
    // to the next event-loop turn so sorting cannot reenter a KDE model signal.
    captureRunningOrder();
    sort(0);
}

void DockSectionModel::scheduleRunningOrderCapture() {
    if (m_runningCaptureScheduled) return;
    m_runningCaptureScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_runningCaptureScheduled = false;
        if (sourceModel()) captureRunningOrder();
        ++m_mappingRevision;
        emit mappingChanged();
    });
}

QString DockSectionModel::normalized(const QString &url) {
    QString result = url.section(QLatin1Char('?'), 0, 0).trimmed();
    if (result.startsWith(QLatin1String("applications:/"))) {
        QString suffix = result.mid(QStringLiteral("applications:").size());
        while (suffix.startsWith(QLatin1Char('/'))) suffix.remove(0, 1);
        result = QStringLiteral("applications:") + suffix;
    }
    // KDE's desktop file and Wayland app ID use these two names for the
    // same System Settings application.
    if (result == QStringLiteral("applications:org.kde.systemsettings.desktop"))
        result = QStringLiteral("applications:systemsettings.desktop");
    return result;
}

int DockSectionModel::roleNumber(const char *roleName) const {
    if (!sourceModel()) return -1;
    const auto names = sourceModel()->roleNames();
    return names.key(QByteArray(roleName), -1);
}

QString DockSectionModel::stableIdentity(const QModelIndex &idx) const {
    if (!idx.isValid() || !sourceModel()) return {};
    const auto named = [this, &idx](const char *role) {
        const int r = roleNumber(role);
        return r >= 0 ? sourceModel()->data(idx, r).toString().trimmed() : QString();
    };
    // AppId and StorageId are stable across a launcher -> window transition.
    const QString appId = named("AppId");
    if (!appId.isEmpty()) return QStringLiteral("app:") + appId;
    const QString storage = named("StorageId");
    if (!storage.isEmpty()) return QStringLiteral("storage:") + storage;
    QString url = named("LauncherUrlWithoutIcon");
    if (url.isEmpty()) url = named("LauncherUrl");
    if (!url.isEmpty()) return QStringLiteral("url:") + normalized(url);
    const QString winList = named("WinIdList");
    if (!winList.isEmpty()) return QStringLiteral("win:") + winList;
    // Best effort when KDE cannot identify a Wine/Proton window.
    return QStringLiteral("row:") + QString::number(idx.row());
}

void DockSectionModel::captureRunningOrder() {
    if (!sourceModel()) return;
    bool newIdentity = false;
    for (int row = 0; row < sourceModel()->rowCount(); ++row) {
        const QModelIndex idx = sourceModel()->index(row, 0);
        const int launcherRole = roleNumber("IsLauncher");
        if (launcherRole >= 0 && sourceModel()->data(idx, launcherRole).toBool()) continue;
        const int windowRole = roleNumber("IsWindow");
        const int groupRole = roleNumber("IsGroupParent");
        const int startupRole = roleNumber("IsStartup");
        if (!(windowRole >= 0 && sourceModel()->data(idx, windowRole).toBool()) &&
            !(groupRole >= 0 && sourceModel()->data(idx, groupRole).toBool()) &&
            !(startupRole >= 0 && sourceModel()->data(idx, startupRole).toBool())) continue;
        const QString key = stableIdentity(idx);
        if (!key.isEmpty() && !m_runningOrder.contains(key)) {
            m_runningOrder.append(key);
            newIdentity = true;
        }
    }
    if (newIdentity) invalidate();
}

int DockSectionModel::launcherPositionForUrl(const QString &url) const {
    // Plasma's launcherPosition resolves preferred://browser against KDE's
    // launcher list. We deliberately DO NOT infer ownership from HasLauncher:
    // after a KofDock unpin that role can remain stale until the next start.
    if (!sourceModel() || url.isEmpty() ||
        sourceModel()->metaObject()->indexOfMethod("launcherPosition(QUrl)") < 0)
        return -1;
    int position = -1;
    const bool called = QMetaObject::invokeMethod(sourceModel(), "launcherPosition",
        Qt::DirectConnection, Q_RETURN_ARG(int, position),
        Q_ARG(QUrl, QUrl(url)));
    return called ? position : -1;
}

bool DockSectionModel::matchesLauncher(const QModelIndex &idx, const QString &rawSaved) const {
    if (!idx.isValid() || !sourceModel()) return false;
    const QString saved = normalized(rawSaved);
    if (saved.isEmpty()) return false;
    const int primaryRole = roleNumber("LauncherUrlWithoutIcon");
    const int alternateRole = roleNumber("LauncherUrl");
    const QString primary = primaryRole >= 0
        ? normalized(sourceModel()->data(idx, primaryRole).toString()) : QString();
    const QString alternate = alternateRole >= 0
        ? normalized(sourceModel()->data(idx, alternateRole).toString()) : QString();
    if (saved.startsWith(QStringLiteral("applications:"))) {
        for (const char *role : {"AppId", "StorageId"}) {
            const int number = roleNumber(role);
            if (number < 0) continue;
            QString id = sourceModel()->data(idx, number).toString().trimmed();
            if (id.isEmpty()) continue;
            if (!id.endsWith(QStringLiteral(".desktop"))) id += QStringLiteral(".desktop");
            if (normalized(QStringLiteral("applications:") + id) == saved) return true;
        }
    }
    if ((!primary.isEmpty() && primary == saved) ||
        (!alternate.isEmpty() && alternate == saved)) return true;

    if (!saved.startsWith(QStringLiteral("preferred://"))) return false;
    // KDE's launcherPosition(QUrl) compares the *candidate* to each
    // resolved launcher. Passing preferred://browser itself returns -1;
    // do not try to resolve the alias by querying that method with the alias.
    // Instead use the original list seeded into KDE's TasksModel at startup.
    // Unlike KofDock's current list, that source list intentionally remains
    // unchanged while pins are added, removed or reordered during this run.
    const QStringList sourceLaunchers =
        sourceModel()->property("launcherList").toStringList();
    int aliasPosition = -1;
    for (int i = 0; i < sourceLaunchers.size(); ++i) {
        if (normalized(sourceLaunchers.at(i)) == saved) {
            aliasPosition = i;
            break;
        }
    }
    if (aliasPosition < 0) return false;
    // Same *slot* in KDE's resolved launcher list; not just any app with
    // launcherPosition >= 0, which could also be another pinned application.
    return ((!primary.isEmpty() && launcherPositionForUrl(primary) == aliasPosition) ||
            (!alternate.isEmpty() && launcherPositionForUrl(alternate) == aliasPosition));
}

int DockSectionModel::pinnedRank(const QModelIndex &idx) const {
    if (!idx.isValid() || !sourceModel()) return std::numeric_limits<int>::max();
    for (int i = 0; i < m_launchers.size(); ++i) {
        if (matchesLauncher(idx, m_launchers.at(i))) return i;
    }
    return std::numeric_limits<int>::max() / 2;
}

int DockSectionModel::sourceRowForLauncher(const QString &launcherUrl) const {
    if (!sourceModel()) return -1;
    const QString wanted = normalized(launcherUrl);
    if (wanted.isEmpty()) return -1;
    const bool isApplication = wanted.startsWith(QStringLiteral("applications:"));
    const QString expectedApp = isApplication
        ? wanted.mid(QStringLiteral("applications:").size())
                  .remove(QStringLiteral(".desktop"), Qt::CaseInsensitive).toLower()
        : QString();
    const int appIdRole = roleNumber("AppId");
    const int windowRole = roleNumber("IsWindow");
    const int groupRole = roleNumber("IsGroupParent");
    const int startupRole = roleNumber("IsStartup");
    int launcherFallback = -1;
    for (int row = 0; row < sourceModel()->rowCount(); ++row) {
        const QModelIndex idx = sourceModel()->index(row, 0);
        bool matches = matchesLauncher(idx, wanted);
        if (!matches && isApplication && appIdRole >= 0) {
            QString appId = sourceModel()->data(idx, appIdRole).toString().toLower();
            if (appId.endsWith(QStringLiteral(".desktop"))) appId.chop(8);
            matches = !expectedApp.isEmpty() && expectedApp == appId;
        }
        if (!matches) continue;
        // KDE can briefly expose both a launcher and a window. Prefer the
        // live task so clicks and running markers remain on the same icon.
        if ((windowRole >= 0 && sourceModel()->data(idx, windowRole).toBool()) ||
            (groupRole >= 0 && sourceModel()->data(idx, groupRole).toBool()) ||
            (startupRole >= 0 && sourceModel()->data(idx, startupRole).toBool()))
            return row;
        launcherFallback = row;
    }
    return launcherFallback;
}

bool DockSectionModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    if (m_pinnedSection) {
        const int a = pinnedRank(left);
        const int b = pinnedRank(right);
        return a == b ? left.row() < right.row() : a < b;
    }
    const int a = m_runningOrder.indexOf(stableIdentity(left));
    const int b = m_runningOrder.indexOf(stableIdentity(right));
    const int ar = a < 0 ? std::numeric_limits<int>::max() / 2 : a;
    const int br = b < 0 ? std::numeric_limits<int>::max() / 2 : b;
    return ar == br ? left.row() < right.row() : ar < br;
}

bool DockSectionModel::moveRunning(int sourceFromRow, int sourceToRow) {
    if (m_pinnedSection || !sourceModel() ||
        sourceFromRow < 0 || sourceToRow < 0 ||
        sourceFromRow >= sourceModel()->rowCount() ||
        sourceToRow >= sourceModel()->rowCount()) return false;
    const QString from = stableIdentity(sourceModel()->index(sourceFromRow, 0));
    const QString to = stableIdentity(sourceModel()->index(sourceToRow, 0));
    const int first = m_runningOrder.indexOf(from);
    const int target = m_runningOrder.indexOf(to);
    if (first < 0 || target < 0 || first == target) return false;
    m_runningOrder.move(first, target);
    // Resort only after an explicit drag, never on application launch.
    invalidate();
    return true;
}

bool DockSectionModel::filterAcceptsRow(int sourceRow, const QModelIndex &parent) const {
    if (!sourceModel()) return false;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, parent);
    if (!index.isValid()) return false;
    const int launcherRole = roleNumber("LauncherUrlWithoutIcon");
    const int fallbackRole = roleNumber("LauncherUrl");
    QString url = launcherRole >= 0 ? sourceModel()->data(index, launcherRole).toString() : QString();
    if (url.isEmpty() && fallbackRole >= 0) url = sourceModel()->data(index, fallbackRole).toString();
    url = normalized(url);

    // KDE's IsLauncher/HasLauncher can remain stale after an unpin. Only
    // current KofDock pins count, including explicitly resolved preferred://
    // aliases (matched to their precise KDE launcher-list slot).
    const bool pinned = pinnedRank(index) < std::numeric_limits<int>::max() / 2;

    // A stacked favorite remains in the KDE task source but is represented
    // visually by a single stack cell in the pinned section.
    if (m_pinnedSection && m_stackLaunchers.contains(url)) return false;
    if (m_pinnedSection) {
        for (const QString &stacked : m_stackLaunchers)
            if (matchesLauncher(index, stacked)) return false;
    }
    if (m_pinnedSection) return pinned;
    if (!m_showRunning || pinned) return false;

    const int isWindowRole = roleNumber("IsWindow");
    const int isGroupRole = roleNumber("IsGroupParent");
    const int startupRole = roleNumber("IsStartup");
    return (isWindowRole >= 0 && sourceModel()->data(index, isWindowRole).toBool()) ||
           (isGroupRole >= 0 && sourceModel()->data(index, isGroupRole).toBool()) ||
           (startupRole >= 0 && sourceModel()->data(index, startupRole).toBool());
}

void DockSectionModel::setPinnedSection(bool pinned) {
    if (m_pinnedSection == pinned) return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
#endif
    m_pinnedSection = pinned;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
    emit pinnedSectionChanged();
}

void DockSectionModel::setShowRunning(bool running) {
    if (m_showRunning == running) return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
#endif
    m_showRunning = running;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
    emit showRunningChanged();
}

void DockSectionModel::setLaunchers(const QStringList &launchers) {
    QStringList cleaned;
    for (const auto &entry : launchers) {
        const QString value = normalized(entry);
        if (!value.isEmpty() && !cleaned.contains(value)) cleaned << value;
    }
    if (m_launchers == cleaned) return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
#endif
    m_launchers = cleaned;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
    // Re-sort after a deliberate pin/unpin/reorder; source row changes alone
    // must not shift these cells out of their user-defined positions.
    invalidate();
    emit launchersChanged();
}

int DockSectionModel::sourceRow(int row) const {
    const QModelIndex mapped = mapToSource(index(row, 0));
    return mapped.isValid() ? mapped.row() : -1;
}

void DockSectionModel::setStackLaunchers(const QStringList &urls) {
    QStringList cleaned;
    for (const auto &entry : urls) {
        const QString value = normalized(entry);
        if (!value.isEmpty() && !cleaned.contains(value)) cleaned << value;
    }
    if (m_stackLaunchers == cleaned) return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
#endif
    m_stackLaunchers = cleaned;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
    emit stackLaunchersChanged();
}

void DockSectionModel::requestCloseLater(int sourceRow) {
    if (!sourceModel() || sourceRow < 0 || sourceRow >= sourceModel()->rowCount()) return;
    const QPersistentModelIndex target(sourceModel()->index(sourceRow, 0));
    const QPointer<QAbstractItemModel> model(sourceModel());
    // Leave MenuItem.triggered before KDE can remove the delegate and its menu.
    QTimer::singleShot(0, this, [model, target]() {
        if (!model || !target.isValid()) return;
        const QModelIndex index(target);
        QMetaObject::invokeMethod(model, "requestClose", Qt::DirectConnection,
                                  Q_ARG(QModelIndex, index));
    });
}
