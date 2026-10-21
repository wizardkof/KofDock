// SPDX-License-Identifier: MIT
#include "docksectionmodel.h"

#include <QAbstractListModel>
#include <QCoreApplication>
#include <QTest>
#include <QUrl>
#include <utility>

class FakeTasks final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QStringList launcherList READ launcherList)
public:
    struct Entry {
        QString url;
        bool launcher;
        bool window;
        bool group;
        bool startup;
        bool hasLauncher = false;
        QString appId;
    };
    QList<Entry> entries;
    QString preferredBrowser = QStringLiteral("applications:org.mozilla.firefox.desktop");
    QStringList seededLaunchers;
    QStringList launcherList() const { return seededLaunchers; }
    enum Roles { UrlRole = Qt::UserRole + 1, IsLauncherRole, IsWindowRole, IsGroupRole, IsStartupRole, HasLauncherRole, AppIdRole };

    explicit FakeTasks(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : int(entries.size());
    }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= entries.size()) return {};
        const auto &item = entries.at(index.row());
        switch (role) {
        case UrlRole: return item.url;
        case IsLauncherRole: return item.launcher;
        case IsWindowRole: return item.window;
        case IsGroupRole: return item.group;
        case IsStartupRole: return item.startup;
        case HasLauncherRole: return item.hasLauncher;
        case AppIdRole: return item.appId;
        default: return {};
        }
    }
    QHash<int, QByteArray> roleNames() const override {
        return {{UrlRole, "LauncherUrlWithoutIcon"}, {IsLauncherRole, "IsLauncher"},
                {IsWindowRole, "IsWindow"}, {IsGroupRole, "IsGroupParent"},
                {IsStartupRole, "IsStartup"}, {HasLauncherRole, "HasLauncher"},
                {AppIdRole, "AppId"}};
    }
    void append(Entry entry) {
        const int pos = entries.size();
        beginInsertRows(QModelIndex(), pos, pos);
        entries.append(std::move(entry));
        endInsertRows();
    }
    void replace(const QList<Entry> &updated) {
        beginResetModel();
        entries = updated;
        endResetModel();
    }
    // Plasma's LauncherTasksModel::launcherPosition compares an application
    // candidate to the RESOLVED URL for each launcher. Querying the preferred
    // alias directly returns -1 (the alias itself is not an applications: URL).
    Q_INVOKABLE int launcherPosition(const QUrl &url) const {
        const QString candidate = url.toString();
        for (int i = 0; i < seededLaunchers.size(); ++i) {
            const QString saved = seededLaunchers.at(i);
            if (saved.startsWith(QStringLiteral("applications:")) && candidate == saved)
                return i;
            if (saved == QStringLiteral("preferred://browser") &&
                candidate == preferredBrowser) return i;
        }
        return -1;
    }
};

class DockSectionTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void sectionsAreDisjoint() {
        FakeTasks source;
        source.append({"applications:a.desktop", true, false, false, false});
        source.append({"applications:b.desktop?icon=foo", false, true, false, false});
        source.append({"applications:c.desktop", false, true, false, false});
        source.append({"", false, true, false, false});
        DockSectionModel pinned, running;
        pinned.setSourceModel(&source);
        running.setSourceModel(&source);
        pinned.setLaunchers({"applications:a.desktop", "applications:b.desktop"});
        running.setLaunchers({"applications:a.desktop", "applications:b.desktop"});
        running.setPinnedSection(false);
        QCOMPARE(pinned.count(), 2);
        QCOMPARE(running.count(), 2);
        QCOMPARE(pinned.sourceRow(0), 0);
        QCOMPARE(pinned.sourceRow(1), 1);
        QCOMPARE(running.sourceRow(0), 2);
        QCOMPARE(running.sourceRow(1), 3);
        QCOMPARE(pinned.sourceRow(10), -1);
    }
    void dynamicHideAndAdd() {
        FakeTasks source;
        source.append({"applications:a.desktop", false, true, false, false});
        DockSectionModel running;
        running.setPinnedSection(false);
        running.setSourceModel(&source);
        QCOMPARE(running.count(), 1);
        running.setShowRunning(false);
        QCOMPARE(running.count(), 0);
        running.setShowRunning(true);
        QCOMPARE(running.count(), 1);
        running.setLaunchers({"applications:a.desktop"});
        QCOMPARE(running.count(), 0);
        source.append({"applications:new.desktop", false, false, true, false});
        QCOMPARE(running.count(), 1);
    }
    void activeBrowserResolvesPreferredLauncherWithoutDuplicates() {
        FakeTasks source;
        source.seededLaunchers = {QStringLiteral("preferred://browser")};
        source.append({"applications:org.mozilla.firefox.desktop", false, true, false, false, true});
        DockSectionModel pinned, running;
        pinned.setSourceModel(&source);
        running.setSourceModel(&source);
        pinned.setLaunchers({"preferred://browser"});
        running.setLaunchers({"preferred://browser"});
        running.setPinnedSection(false);
        QCOMPARE(pinned.count(), 1);
        QCOMPARE(running.count(), 0);
        QCOMPARE(running.sourceRowForLauncher(QStringLiteral("preferred://browser")), 0);
        // The browser remains visible when unpinned, despite KDE's stale
        // HasLauncher and unchanged internal launcherPosition result.
        pinned.setLaunchers({});
        running.setLaunchers({});
        QCOMPARE(pinned.count(), 0);
        QCOMPARE(running.count(), 1);
    }
    void preferredBrowserNeverClaimsOtherKdeLauncherSlots() {
        FakeTasks source;
        source.seededLaunchers = {QStringLiteral("preferred://browser"),
                                  QStringLiteral("applications:other.desktop")};
        source.append({"applications:org.mozilla.firefox.desktop", false, true, false, false, true});
        source.append({"applications:zen.desktop", false, true, false, false, true});
        source.append({"applications:other.desktop", false, true, false, false, true});
        DockSectionModel pinned, running;
        pinned.setSourceModel(&source);
        running.setSourceModel(&source);
        pinned.setLaunchers({"preferred://browser", "applications:other.desktop"});
        running.setLaunchers({"preferred://browser", "applications:other.desktop"});
        running.setPinnedSection(false);
        QCOMPARE(pinned.count(), 2);
        QCOMPARE(pinned.sourceRow(0), 0); // Firefox is the preferred browser
        QCOMPARE(pinned.sourceRow(1), 2); // other app is an explicit pin
        QCOMPARE(running.count(), 1);
        QCOMPARE(running.sourceRow(0), 1); // Zen is running but not the default
        QCOMPARE(pinned.sourceRowForLauncher(QStringLiteral("preferred://browser")), 0);
    }
    void preferredPinReorderUsesOriginalKdeSlot() {
        FakeTasks source;
        source.seededLaunchers = {QStringLiteral("preferred://browser"),
                                  QStringLiteral("applications:other.desktop")};
        source.append({"applications:other.desktop", false, true, false, false, true});
        source.append({"applications:org.mozilla.firefox.desktop", false, true, false, false, true});
        DockSectionModel pinned;
        pinned.setSourceModel(&source);
        pinned.setLaunchers({"applications:other.desktop", "preferred://browser"});
        QCOMPARE(pinned.count(), 2);
        QCOMPARE(pinned.sourceRow(0), 0); // reordered visual list: other first
        QCOMPARE(pinned.sourceRow(1), 1); // preferred browser second
        QCOMPARE(pinned.sourceRowForLauncher(QStringLiteral("preferred://browser")), 1);
    }
    void optionalSlashAndQueryMatchPinnedLauncher() {
        FakeTasks source;
        source.append({"applications:/org.kde.konsole.desktop?icon=demo", false, true, false, false});
        DockSectionModel pinned;
        pinned.setSourceModel(&source);
        pinned.setLaunchers({"applications:org.kde.konsole.desktop"});
        QCOMPARE(pinned.count(), 1);
    }
    void stackHidesOnlyItsVisualPinnedCellsAndPreservesTasks() {
        FakeTasks source;
        source.append({"applications:writer.desktop", true, false, false, false});
        source.append({"applications:calc.desktop", true, false, false, false});
        source.append({"applications:browser.desktop", false, true, false, false});
        DockSectionModel pinned, running;
        pinned.setSourceModel(&source);
        running.setSourceModel(&source);
        pinned.setLaunchers({"applications:writer.desktop", "applications:calc.desktop"});
        running.setLaunchers({"applications:writer.desktop", "applications:calc.desktop"});
        running.setPinnedSection(false);
        QCOMPARE(pinned.count(), 2);
        QCOMPARE(running.count(), 1);
        pinned.setStackLaunchers({"applications:writer.desktop"});
        QCOMPARE(pinned.count(), 1);
        QCOMPARE(pinned.sourceRow(0), 1);
        QCOMPARE(running.count(), 1);
        pinned.setStackLaunchers({});
        QCOMPARE(pinned.count(), 2);
        QCOMPARE(pinned.sourceRow(0), 0);
    }
    void stackNormalizesSlashAndQuery() {
        FakeTasks source;
        source.append({"applications:/org.kde.dolphin.desktop?icon=x", true, false, false, false});
        DockSectionModel pinned;
        pinned.setSourceModel(&source);
        pinned.setLaunchers({"applications:org.kde.dolphin.desktop"});
        QCOMPARE(pinned.count(), 1);
        pinned.setStackLaunchers({"applications:org.kde.dolphin.desktop"});
        QCOMPARE(pinned.count(), 0);
    }
    void pinnedOrderDoesNotFollowSourceReorderAtLaunch() {
        FakeTasks source;
        source.append({"applications:b.desktop", true, false, false, false});
        source.append({"applications:a.desktop", true, false, false, false});
        DockSectionModel pinned;
        pinned.setSourceModel(&source);
        pinned.setLaunchers({"applications:a.desktop", "applications:b.desktop"});
        QCOMPARE(pinned.count(), 2);
        QCOMPARE(pinned.sourceRow(0), 1); // A first, despite KDE source order B/A
        QCOMPARE(pinned.sourceRow(1), 0);

        // KDE replaces launcher A with a real window and reverses its rows.
        source.replace({{"applications:a.desktop", false, true, false, false, true},
                        {"applications:b.desktop", true, false, false, false}});
        QCOMPARE(pinned.sourceRow(0), 0); // A stays visually first
        QCOMPARE(pinned.sourceRow(1), 1);
        QCOMPARE(pinned.data(pinned.index(0, 0), FakeTasks::UrlRole).toString(),
                 QStringLiteral("applications:a.desktop"));
    }
    void pinnedOrderHonorsPreferredBrowserAlias() {
        FakeTasks source;
        source.preferredBrowser = QStringLiteral("applications:zen.desktop");
        source.seededLaunchers = {QStringLiteral("preferred://browser"),
                                  QStringLiteral("applications:b.desktop")};
        source.append({"applications:b.desktop", true, false, false, false});
        source.append({"applications:zen.desktop", false, true, false, false, true});
        DockSectionModel pinned;
        pinned.setSourceModel(&source);
        pinned.setLaunchers({"preferred://browser", "applications:b.desktop"});
        QCOMPARE(pinned.count(), 2);
        QCOMPARE(pinned.sourceRow(0), 1);
        QCOMPARE(pinned.sourceRow(1), 0);
    }
    void runningOrderStableAcrossLaunchResetAndUserDrag() {
        FakeTasks source;
        source.append({"applications:c.desktop", false, true, false, false});
        source.append({"applications:d.desktop", false, true, false, false});
        DockSectionModel running;
        running.setPinnedSection(false);
        running.setSourceModel(&source);
        QCOMPARE(running.count(), 2);
        // A third app opens; source reorder should not shuffle the first two.
        source.replace({{"applications:d.desktop", false, true, false, false},
                        {"applications:e.desktop", false, true, false, false},
                        {"applications:c.desktop", false, true, false, false}});
        QCOMPARE(running.count(), 3);
        QCOMPARE(running.data(running.index(0, 0), FakeTasks::UrlRole).toString(),
                 QStringLiteral("applications:c.desktop"));
        QCOMPARE(running.data(running.index(1, 0), FakeTasks::UrlRole).toString(),
                 QStringLiteral("applications:d.desktop"));
        QCOMPARE(running.data(running.index(2, 0), FakeTasks::UrlRole).toString(),
                 QStringLiteral("applications:e.desktop"));
        // An explicit user drag can change presentation order without altering KDE.
        QVERIFY(running.moveRunning(2, 0));
        QCOMPARE(running.data(running.index(0, 0), FakeTasks::UrlRole).toString(),
                 QStringLiteral("applications:d.desktop"));
        QCOMPARE(running.data(running.index(1, 0), FakeTasks::UrlRole).toString(),
                 QStringLiteral("applications:c.desktop"));
        source.replace({{"applications:e.desktop", false, true, false, false},
                        {"applications:c.desktop", false, true, false, false},
                        {"applications:d.desktop", false, true, false, false}});
        QCOMPARE(running.data(running.index(0, 0), FakeTasks::UrlRole).toString(),
                 QStringLiteral("applications:d.desktop"));
    }
    void sourceReplacementKeepsProxyConnected() {
        FakeTasks first, second;
        first.append({"applications:a.desktop", true, false, false, false});
        second.append({"applications:b.desktop", false, true, false, false});
        DockSectionModel proxy;
        proxy.setPinnedSection(false);
        proxy.setSourceModel(&first);
        QCOMPARE(proxy.count(), 0);
        proxy.setSourceModel(&second);
        QCOMPARE(proxy.count(), 1);
        first.append({"applications:c.desktop", false, true, false, false});
        QCoreApplication::processEvents();
        QCOMPARE(proxy.count(), 1); // old source cannot mutate this proxy
        second.append({"applications:d.desktop", false, true, false, false});
        QCoreApplication::processEvents();
        QCOMPARE(proxy.count(), 2);
    }
    void rapidPinUnpinAndWindowTransitionsDoNotReenterSorting() {
        FakeTasks source;
        source.append({"applications:a.desktop", true, false, false, false});
        source.append({"applications:b.desktop", false, true, false, false});
        DockSectionModel pinned, running;
        pinned.setSourceModel(&source);
        running.setSourceModel(&source);
        running.setPinnedSection(false);
        for (int i = 0; i < 100; ++i) {
            const QStringList pins = i % 2 == 0
                ? QStringList{QStringLiteral("applications:a.desktop"), QStringLiteral("applications:b.desktop")}
                : QStringList{QStringLiteral("applications:a.desktop")};
            pinned.setLaunchers(pins);
            running.setLaunchers(pins);
            source.replace(i % 2 == 0
                ? QList<FakeTasks::Entry>{{"applications:b.desktop", false, true, false, false, true},
                                          {"applications:a.desktop", true, false, false, false}}
                : QList<FakeTasks::Entry>{{"applications:a.desktop", false, true, false, false, true},
                                          {"applications:b.desktop", false, true, false, false}});
            QCoreApplication::processEvents();
            QCOMPARE(pinned.count() + running.count(), 2);
            for (int j = 0; j < pinned.count(); ++j)
                QVERIFY(pinned.sourceRow(j) >= 0 && pinned.sourceRow(j) < source.rowCount());
            for (int j = 0; j < running.count(); ++j)
                QVERIFY(running.sourceRow(j) >= 0 && running.sourceRow(j) < source.rowCount());
        }
    }
    void unpinningWithStaleKdeHasLauncherKeepsRunningTaskVisible() {
        FakeTasks source;
        source.append({"applications:app.desktop", false, true, false, false, true});
        DockSectionModel running;
        running.setSourceModel(&source);
        running.setPinnedSection(false);
        running.setLaunchers({QStringLiteral("applications:app.desktop")});
        QCOMPARE(running.count(), 0);
        // KDE may still say HasLauncher=true until this Dock restarts.
        // The user's own saved list governs the visible section.
        running.setLaunchers({});
        QCOMPARE(running.count(), 1);
        QCOMPARE(running.sourceRow(0), 0);
    }
    void launchersRemainPinnedDuringStart() {
        FakeTasks source;
        source.append({"applications:app.desktop", false, false, false, true});
        DockSectionModel pinned;
        pinned.setSourceModel(&source);
        pinned.setLaunchers({"applications:app.desktop"});
        QCOMPARE(pinned.count(), 1);
        QCOMPARE(pinned.sourceRow(0), 0);
    }
};

QTEST_GUILESS_MAIN(DockSectionTests)
#include "test_docksectionmodel.moc"
