// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import org.kde.taskmanager as TM
import org.kde.kirigami as Kirigami
import KofDock 1.0
import "DockLogic.js" as Logic

Window {
    id: dock
    visible: false // Layer-shell attached in C++ before mapping.
    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"
    title: "KofDock"

    // CRITICAL: a fixed full-screen-width layer surface. Zoom must not resize
    // or re-center the Wayland window; only the drawn icons/plate may animate.
    width: Math.max(1, dockController.screenGeometry.width > 0 ? dockController.screenGeometry.width : Screen.width)
    readonly property real itemHeight: dockSettings.iconSize * dockSettings.zoomScale + 24
    readonly property real plateHeight: dockSettings.iconSize + 22
    // Fan lives in this SAME layer surface, above the icon that owns it.
    // Never open a separate Popup.Window that Wayland can place elsewhere.
    readonly property int fanHeadroom: Math.max(460, Math.min(900,
        (dockController.screenGeometry.height > 0 ? dockController.screenGeometry.height : Screen.height)
        - itemHeight - dockSettings.bottomGap - 8))
    height: Math.min(Math.max(1, dockController.screenGeometry.height > 0 ? dockController.screenGeometry.height : Screen.height), Math.ceil(itemHeight + dockSettings.bottomGap + fanHeadroom))

    property real hoverPosition: -1
    property int hoveredIndex: -1
    property int openMenus: 0
    property string draggingUrl: ""
    property bool draggingWasPinned: false
    property real dropGuideX: -1
    property bool reordering: false
    property var queuedLaunchers: null
    property bool launcherUpdateScheduled: false
    property string bouncingKey: ""
    property real bounceProgress: 0
    NumberAnimation {
        id: bounceAnimation
        target: dock
        property: "bounceProgress"
        from: 0
        to: 1
        duration: 960
        easing.type: Easing.Linear
        onFinished: dock.bouncingKey = ""
    }
    function launchKey(url, appId) {
        const app = String(appId || "").replace(/\.desktop$/, "").toLowerCase();
        if (app.length > 0) return app;
        return normalizedUrl(url).replace(/^applications:\/*/, "")
               .replace(/\.desktop$/, "").toLowerCase();
    }
    property bool modelReady: false
    property int taskRefreshRevision: 0
    property bool revealed: true
    property bool windowOverlapsDock: false
    readonly property int sidePadding: 14
    readonly property int peekHeight: 3
    readonly property int edgeTriggerHeight: 12
    readonly property var visiblePinnedLaunchers: dockSettings.launchers.filter(url =>
        !dockSettings.stackLaunchers.some(stacked => normalizedUrl(stacked) === normalizedUrl(url)))
    readonly property int stackCount: dockSettings.stackLaunchers.length
    readonly property int applicationCount: visiblePinnedLaunchers.length + runningModel.count
    readonly property int totalCells: applicationCount + (stackCount > 0 ? 1 : 0)
    // Without stacks the divider separates favorites and unpinned windows;
    // with a stack it separates ALL apps from folders, as on macOS.
    readonly property bool showDivider: stackCount === 0 && visiblePinnedLaunchers.length > 0 && runningModel.count > 0
    readonly property bool showStackDivider: stackCount > 0 && applicationCount > 0
    readonly property bool anyMenuOpen: settingsWin.visible || openMenus > 0 || fanOpen
    property bool fanOpen: false
    property var fanOwner: null
    property var fanEntries: []
    property real fanAnchorX: -1
    property int fanRevealCycle: 0
    // Track the *transformed* icon, not the centre of its fixed task slot.
    readonly property real liveFanAnchorX: fanOwner
        ? iconRow.x + fanOwner.x + fanOwner.width / 2 + fanOwner.renderedShift
        : fanAnchorX
    readonly property real fanBaseY: iconRow.y + iconRow.height - 12 - dockSettings.iconSize / 2
    readonly property bool hideRequested: dockSettings.autoHide || (dockSettings.smartHide && windowOverlapsDock)
    readonly property color plateTint: dockSettings.backgroundColor
    // Compute the wave once per cursor position. Icons only animate scene-graph
    // transforms; their QtQuick/Kirigami pixel size remains constant.
    readonly property var zoomLayout: Logic.zoomLayout(totalCells, hoverPosition,
                                         dockSettings.zoomScale, dockSettings.zoomSpread,
                                         dockSettings.iconSize)
    readonly property real extraVisualWidth: zoomLayout.extra
    readonly property real dividerOffset: Logic.dividerShift(zoomLayout.scales, visiblePinnedLaunchers.length, dockSettings.iconSize)
    readonly property real stackDividerOffset: Logic.dividerShift(zoomLayout.scales, applicationCount, dockSettings.iconSize)
    readonly property real maxVisualExtra: Logic.maximumZoomExtra(totalCells,
                                       dockSettings.zoomScale, dockSettings.zoomSpread,
                                       dockSettings.iconSize)

    TM.TasksModel {
        id: taskModel
        sortMode: TM.TasksModel.SortManual
        groupMode: TM.TasksModel.GroupApplications
        groupInline: false
        groupingWindowTasksThreshold: -1
        separateLaunchers: false
        hideActivatedLaunchers: true
        launchInPlace: true
        filterByScreen: false
        filterByCurrentVirtualDesktop: false
        filterByActivity: false
        // Saved launchers are owned by DockSettings. KDE may refresh its list
        // in response to window changes; that is NOT a user pin/unpin action.
        // Never mutate DockSettings from onLauncherListChanged (reentrancy risk).
        Component.onCompleted: {
            launcherList = dockSettings.launchers;
            dock.modelReady = true;
        }
    }

    // Separate model restricted to the current desktop and monitor for intelligent hide.
    // Does not affect which launchers or windows appear in the Dock.
    TM.TasksModel {
        id: visibilityModel
        groupMode: TM.TasksModel.GroupDisabled
        filterByCurrentVirtualDesktop: true
        filterByScreen: true
        screenGeometry: dockController.screenGeometry
        hideActivatedLaunchers: true
    }

    // A stack indicator is visible only when one of its apps is running.
    function stackHasRunning(urls) {
        for (const url of urls) {
            const row = stackEntryAt(String(url));
            if (row < 0) continue;
            const idx = taskModel.makeModelIndex(row);
            if (taskModel.data(idx, TM.AbstractTasksModel.IsWindow) ||
                taskModel.data(idx, TM.AbstractTasksModel.IsGroupParent) ||
                taskModel.data(idx, TM.AbstractTasksModel.IsStartup)) return true;
        }
        return false;
    }
    Connections {
        target: taskModel
        function onCountChanged() { dock.taskRefreshRevision += 1; }
        function onDataChanged() { dock.taskRefreshRevision += 1; }
        function onModelReset() { dock.taskRefreshRevision += 1; }
        function onLayoutChanged() { dock.taskRefreshRevision += 1; }
    }

    // Pinned delegates follow the USER list, not mutable task-proxy rows.
    // Resolve preferred://browser through the *same* KDE launcherPosition
    // contract used by the running-section filter. No inconsistent duplicates.
    function sourceRowForLauncher(launcherUrl) {
        taskRefreshRevision; // refresh on KDE window lifecycle notifications
        return runningModel.sourceRowForLauncher(String(launcherUrl));
    }
    function snapshotForLauncher(url) {
        taskRefreshRevision;
        const row = sourceRowForLauncher(url);
        const idx = row >= 0 ? taskModel.makeModelIndex(row) : null;
        const role = (roleId, fallback) => row >= 0
                 ? taskModel.data(idx, roleId) : fallback;
        const key = String(url).replace(/^applications:\/*/, "").replace(/\.desktop$/, "");
        return {
            LauncherUrlWithoutIcon: url, LauncherUrl: url,
            AppId: String(role(TM.AbstractTasksModel.AppId, key) || key),
            StorageId: String(role(TM.AbstractTasksModel.StorageId, key + ".desktop") || ""),
            AppName: String(role(TM.AbstractTasksModel.AppName,
                         dockServices.displayName(url, "", "", key)) || key),
            display: String(role(Qt.DisplayRole, key) || key),
            decoration: role(Qt.DecorationRole, dockServices.iconFor(url, "", "", "")),
            IsLauncher: row < 0 || Boolean(role(TM.AbstractTasksModel.IsLauncher, false)),
            IsStartup: Boolean(role(TM.AbstractTasksModel.IsStartup, false)),
            IsWindow: Boolean(role(TM.AbstractTasksModel.IsWindow, false)),
            IsGroupParent: Boolean(role(TM.AbstractTasksModel.IsGroupParent, false)),
            IsActive: Boolean(role(TM.AbstractTasksModel.IsActive, false)),
            IsMinimized: Boolean(role(TM.AbstractTasksModel.IsMinimized, false)),
            IsDemandingAttention: Boolean(role(TM.AbstractTasksModel.IsDemandingAttention, false)),
            IsClosable: Boolean(role(TM.AbstractTasksModel.IsClosable, false)),
            ChildCount: Number(role(TM.AbstractTasksModel.ChildCount, 0) || 0)
        };
    }
    DockSectionModel {
        id: runningModel
        sourceModel: taskModel
        pinnedSection: false
        showRunning: dockSettings.showRunningApps
        launchers: dockSettings.launchers
    }

    // Keep a bottom-edge footprint only while a real window occupies it.
    // A maximized window is always an occluder on the current screen.
    function computeWindowOverlap() {
        if (!dockSettings.smartHide) return false;
        const screenRect = dockController.screenGeometry;
        if (!screenRect || screenRect.width <= 0 || screenRect.height <= 0) return false;
        const zoneWidth = Math.min(dock.width, iconRow.implicitWidth +
                                   dock.sidePadding * 2 + dock.maxVisualExtra);
        // Proximity is intentional: hiding starts *before* a window touches the plate.
        const zone = Logic.dockProximityZone(screenRect, zoneWidth,
                     dock.plateHeight, dockSettings.bottomGap, 36);
        for (let i = 0; i < visibilityModel.count; ++i) {
            const idx = visibilityModel.makeModelIndex(i);
            if (!visibilityModel.data(idx, TM.AbstractTasksModel.IsWindow)) continue;
            if (visibilityModel.data(idx, TM.AbstractTasksModel.IsMinimized)) continue;
            if (visibilityModel.data(idx, TM.AbstractTasksModel.IsMaximized)) return true;
            const rect = visibilityModel.data(idx, TM.AbstractTasksModel.Geometry);
            if (Logic.rectanglesOverlap(zone, rect)) return true;
        }
        return false;
    }
    function refreshVisibility() {
        const newOverlap = computeWindowOverlap();
        if (newOverlap !== windowOverlapsDock)
            console.info("KofDock: ocultação inteligente -> janela próxima=" + newOverlap +
                         ", tela=" + JSON.stringify(dockController.screenGeometry));
        windowOverlapsDock = newOverlap;
        if (hideRequested && !dockHover.hovered && !anyMenuOpen && !draggingUrl) {
            if (revealed && !hideTimer.running) hideTimer.restart();
        } else {
            hideTimer.stop();
            if (!revealed) revealed = true;
        }
    }
    Timer {
        interval: 350
        repeat: true
        running: dockSettings.smartHide
        onTriggered: dock.refreshVisibility()
    }

    function normalizedUrl(url) { return Logic.cleanUrl(url); }
    function isPinned(url) {
        const wanted = normalizedUrl(url);
        return wanted.length > 0 && dockSettings.launchers.some(item => normalizedUrl(item) === wanted);
    }
    // The live TaskManager launcherList MUST NOT be rewritten here.
    // The earlier revisions crashed after precisely that update. The saved
    // launcher set is rendered independently and the Qt task list is seeded
    // from persistent settings on the NEXT process start.
    Timer {
        id: launcherCommitTimer
        interval: 130
        repeat: false
        onTriggered: {
            if (dock.openMenus > 0 || dock.draggingUrl || dock.fanOpen) {
                restart();
                return;
            }
            const pending = dock.queuedLaunchers;
            dock.queuedLaunchers = null;
            dock.launcherUpdateScheduled = false;
            if (!pending) return;
            dock.closeFan();
            dockSettings.launchers = pending;
            console.info("KofDock: favoritos gravados sem mutar TasksModel: " + pending.length);
        }
    }
    function queueLauncherCommit(next) {
        const candidate = Array.from(next || []);
        if (JSON.stringify(candidate) === JSON.stringify(dockSettings.launchers) &&
            !launcherUpdateScheduled) return false;
        queuedLaunchers = candidate;
        launcherUpdateScheduled = true;
        launcherCommitTimer.restart();
        return true;
    }
    function pinOrUnpin(url) {
        const wanted = normalizedUrl(url);
        if (!wanted) return false;
        return queueLauncherCommit(Logic.togglePinnedLauncher(dockSettings.launchers, url));
    }
    function importKdeLaunchers() {
        const fromPlasma = dockServices.plasmaPinnedLaunchers();
        const merged = Array.from(dockSettings.launchers);
        let added = 0;
        for (const entry of fromPlasma) {
            if (merged.some(existing => normalizedUrl(existing) === normalizedUrl(entry))) continue;
            merged.push(entry);
            ++added;
        }
        if (added > 0) queueLauncherCommit(merged);
        return added;
    }
    // A real, persistent application stack. Items remain in the underlying
    // TasksModel launcher list so they keep KWin activation/running state.
    function moveToStack(url) {
        const id = normalizedUrl(url);
        if (!id) return false;
        const current = Array.from(dockSettings.stackLaunchers);
        if (current.some(entry => normalizedUrl(entry) === id)) return false;
        if (!isPinned(url)) queueLauncherCommit(Logic.addPinnedLauncher(dockSettings.launchers, url));
        current.push(url);
        dockSettings.stackLaunchers = current;
        console.info("KofDock: aplicativo adicionado à pilha: " + id);
        return true;
    }
    function removeFromStack(url) {
        const id = normalizedUrl(url);
        dockSettings.stackLaunchers = Array.from(dockSettings.stackLaunchers)
                                    .filter(entry => normalizedUrl(entry) !== id);
    }
    function moveStackItem(from, delta) {
        const entries = Array.from(dockSettings.stackLaunchers);
        const target = from + delta;
        if (from < 0 || target < 0 || from >= entries.length || target >= entries.length) return;
        const selected = entries.splice(from, 1)[0];
        entries.splice(target, 0, selected);
        dockSettings.stackLaunchers = entries;
    }
    function stackEntryAt(url) {
        const id = normalizedUrl(url);
        for (let i = 0; i < taskModel.count; ++i) {
            const index = taskModel.makeModelIndex(i);
            const candidate = taskModel.data(index, TM.AbstractTasksModel.LauncherUrlWithoutIcon);
            const other = taskModel.data(index, TM.AbstractTasksModel.LauncherUrl);
            if (normalizedUrl(candidate) === id || normalizedUrl(other) === id)
                return i;
        }
        return -1;
    }
    function showStackForCell(owner, entries) {
        if (!dockSettings.fanEnabled || !revealed || draggingUrl || !entries || !entries.length)
            return;
        fanDismiss.stop();
        fanClear.stop();
        const wasDifferent = fanOwner !== owner;
        fanOwner = owner;
        fanEntries = entries.slice(); // No 32-entry cap: the fan virtualizes by screen height.
        const point = owner.mapToItem(entireArea, owner.width / 2, 0);
        fanAnchorX = point.x + (owner.renderedShift || 0);
        if (wasDifferent && fanOpen) fanRevealCycle += 1;
        fanOpen = true;
        hideTimer.stop();
        deferMask();
    }
    // When an already-open fan is crossed on the way to another dock app,
    // ownership is handed off *immediately*. A simple app closes the fan;
    // a multi-window app replaces the old fan instead of displaying stale
    // window names until the old 650-ms timer elapses.
    function switchOpenFan(owner, entries) {
        const action = Logic.fanHoverAction(fanOpen, fanOwner === owner,
                                            entries ? entries.length : 0);
        if (action === "switch") showStackForCell(owner, entries);
        else if (action === "dismiss") closeFan();
        else if (fanOpen && fanOwner === owner) fanDismiss.stop();
        return action !== "none";
    }
    function requestFanClose(owner) {
        if (fanOpen && fanOwner === owner) fanDismiss.restart();
    }
    // Safety net for compositors that keep routing hover to the opening fan
    // while the pointer crosses over a second Dock icon. Detect the *drawn*
    // icon slot independently, so ownership cannot get stuck on app A.
    function reconcileFanAtPointer(point) {
        if (!fanOpen || !point || point.y < fanBaseY - 35) return;
        const candidates = [];
        for (const repeater of [pinnedRepeater, runningRepeater]) {
            for (let i = 0; i < repeater.count; ++i) {
                const c = repeater.itemAt(i);
                if (c && c.visible) candidates.push(c);
            }
        }
        if (stackCount > 0) candidates.push(stackCell);
        for (const c of candidates) {
            const center = iconRow.x + c.x + c.width / 2 + (c.renderedShift || 0);
            if (Math.abs(point.x - center) > c.width / 2 + 4) continue;
            if (c === fanOwner) return;
            let entries = [];
            if (c === stackCell) entries = stackCell.entries();
            else if (c.model.IsGroupParent && c.countWindows > 1)
                entries = c.windowEntries();
            switchOpenFan(c, entries);
            return;
        }
    }
    function pointerInFanCorridor() {
        if (!fanOpen || !dockHover.hovered) return false;
        const p = dockHover.point.position;
        const b = Logic.fanBridgeGeometry(liveFanAnchorX, fanBaseY);
        return p.x >= b.x && p.x <= b.x + b.width &&
               p.y >= b.y && p.y <= b.y + b.height;
    }
    function closeFan() {
        fanDismiss.stop();
        fanOpen = false;
        // Keep entries alive while their exit animation plays; previously
        // clearing here made the fan disappear in a single static frame.
        fanClear.restart();
        deferMask();
        Qt.callLater(refreshVisibility);
    }
    Timer {
        id: fanClear
        interval: 165
        repeat: false
        onTriggered: {
            if (!dock.fanOpen) {
                dock.fanOwner = null;
                dock.fanEntries = [];
                dock.deferMask();
            }
        }
    }
    function activateFanEntry(entry) {
        if (entry.kind === "window") {
            activateGroupWindow(entry.sourceRow, entry.childRow);
        } else if (entry.kind === "launcher") {
            const source = stackEntryAt(entry.url);
            if (source >= 0) {
                const idx = taskModel.makeModelIndex(source);
                if (taskModel.data(idx, TM.AbstractTasksModel.IsGroupParent)) {
                    const childCount = taskModel.data(idx, TM.AbstractTasksModel.ChildCount);
                    if (childCount > 0) activateGroupWindow(source, 0);
                } else {
                    if (taskModel.data(idx, TM.AbstractTasksModel.IsMinimized))
                        taskModel.requestToggleMinimized(idx);
                    taskModel.requestActivate(idx);
                }
            } else {
                console.warn("KofDock: lançador da pilha indisponível: " + entry.url);
            }
        }
        closeFan();
    }
    Timer {
        id: fanDismiss
        interval: 115
        repeat: false
        onTriggered: {
            if (dock.pointerInFanCorridor() || fanPanel.panelHovered) return;
            if (!dock.fanOwner || !dock.fanOwner.pointerHover) dock.closeFan();
        }
    }
    function openSettings() {
        settingsWin.show();
        settingsWin.raise();
        settingsWin.requestActivate();
    }
    function activateGroupWindow(sourceRow, childRow) {
        const idx = taskModel.makeModelIndex(sourceRow, childRow);
        if (taskModel.data(idx, TM.AbstractTasksModel.IsMinimized))
            taskModel.requestToggleMinimized(idx);
        taskModel.requestActivate(idx);
    }
    function closeGroupWindow(sourceRow, childRow) {
        taskModel.requestClose(taskModel.makeModelIndex(sourceRow, childRow));
    }
    // Animate the ACTUAL delegate, never create a duplicate icon sprite.
    // The phase lives on the root Window so a KDE launcher->window transition
    // can replace its delegate without restarting the bounce clock.
    function playLaunchBounce(cell) {
        if (!cell || !dock.revealed) return;
        const key = launchKey(cell.launcherUrl, cell.model.AppId);
        if (!key) return;
        bouncingKey = key;
        bounceProgress = 0;
        bounceAnimation.restart();
        console.info("KofDock: salto no próprio ícone: " + key);
    }
    function activate(sourceRow, taskData) {
        if (!taskData.IsWindow && !taskData.IsGroupParent &&
            Logic.isSystemSettingsLauncher(taskData.LauncherUrlWithoutIcon,
                 taskData.AppId, taskData.StorageId)) {
            if (dockServices.launchSystemSettings()) return;
            console.warn("KofDock: tentando lançamento KDE padrão após falha do executável de configurações");
        }
        if (sourceRow < 0) {
            dockServices.launchLauncher(String(taskData.LauncherUrlWithoutIcon || taskData.LauncherUrl || ""));
            return;
        }
        const idx = taskModel.makeModelIndex(sourceRow);
        if (taskData.IsGroupParent) {
            // Group parent activation is a no-op in KWin's task model.
            // Pick the last-used child instead, like clicking a macOS Dock app.
            const windows = [];
            const count = Math.max(0, Math.min(200, Number(taskData.ChildCount || 0)));
            for (let child = 0; child < count; ++child) {
                const ci = taskModel.makeModelIndex(sourceRow, child);
                const stamp = taskModel.data(ci, TM.AbstractTasksModel.LastActivated);
                let last = 0;
                if (stamp && typeof stamp.getTime === "function") last = stamp.getTime();
                else if (typeof stamp === "number") last = stamp;
                windows.push({
                    active: Boolean(taskModel.data(ci, TM.AbstractTasksModel.IsActive)),
                    minimized: Boolean(taskModel.data(ci, TM.AbstractTasksModel.IsMinimized)),
                    lastUsed: Number.isFinite(last) ? last : 0
                });
            }
            const chosen = Logic.lastWindowIndex(windows);
            if (chosen >= 0) activateGroupWindow(sourceRow, chosen);
            return;
        }
        if (taskData.IsWindow && taskData.IsActive && dockSettings.minimizeOnClick) {
            taskModel.requestToggleMinimized(idx);
        } else {
            if (taskData.IsWindow && taskData.IsMinimized)
                taskModel.requestToggleMinimized(idx);
            taskModel.requestActivate(idx);
        }
    }

    function pinnedCells() {
        const result = [];
        for (let i = 0; i < pinnedRepeater.count; ++i) {
            const cell = pinnedRepeater.itemAt(i);
            if (cell && cell.visible && cell.width > 0)
                result.push({url: cell.launcherUrl, left: cell.x, right: cell.x + cell.width});
        }
        return result;
    }
    function updateDropGuide(pointerX) {
        if (stackCount > 0 && draggingUrl &&
            pointerX >= stackCell.x - 8 && pointerX <= stackCell.x + stackCell.width + 8) {
            dropGuideX = stackCell.x + stackCell.width / 2;
            return null;
        }
        const pinnedEdge = pinnedDropBoundary();
        if (!draggingWasPinned && pointerX > pinnedEdge && runningRepeater.count > 0) {
            let nearest = null;
            let nearestDistance = Infinity;
            for (let i = 0; i < runningRepeater.count; ++i) {
                const c = runningRepeater.itemAt(i);
                if (!c || c.sourceRow < 0) continue;
                const center = c.x + c.width / 2;
                const d = Math.abs(pointerX - center);
                if (d < nearestDistance) {
                    nearestDistance = d;
                    nearest = pointerX > center ? c.x + c.width : c.x;
                }
            }
            dropGuideX = nearest === null ? -1 : nearest;
            return null;
        }
        const destination = Logic.chooseDropTarget(pinnedCells(), pointerX);
        dropGuideX = destination ? destination.guideX : (visiblePinnedLaunchers.length === 0 ? 0 : -1);
        return destination;
    }
    function dropPinnedAt(url, pointerX) {
        const destination = Logic.chooseDropTarget(pinnedCells(), pointerX);
        draggingUrl = "";
        dropGuideX = -1;
        if (!url) return;
        // An empty pinned section must accept the first app dragged into it.
        if (!destination && visiblePinnedLaunchers.length === 0) {
            queueLauncherCommit(Logic.addPinnedLauncher(dockSettings.launchers, url));
            return;
        }
        if (!destination) return;
        const original = Array.from(dockSettings.launchers);
        const reordered = Logic.reorderOrPinLauncher(original, url, destination.url, destination.after);
        if (JSON.stringify(original) !== JSON.stringify(reordered)) queueLauncherCommit(reordered);
    }

    function reorderRunningAt(sourceRow, pointerX) {
        let targetRow = -1;
        let targetDistance = Infinity;
        for (let i = 0; i < runningRepeater.count; ++i) {
            const c = runningRepeater.itemAt(i);
            if (!c || c.sourceRow < 0) continue;
            const delta = Math.abs(pointerX - (c.x + c.width / 2));
            if (delta < targetDistance) {
                targetDistance = delta;
                targetRow = c.sourceRow;
            }
        }
        draggingUrl = "";
        dropGuideX = -1;
        if (sourceRow >= 0 && targetRow >= 0 && sourceRow !== targetRow)
            runningModel.moveRunning(sourceRow, targetRow);
    }
    function pinnedDropBoundary() {
        // Drop boundary ALWAYS concerns pinned apps; the visible separator
        // moves after running apps when the user creates a Mac-style stack.
        if (stackCount === 0 && separatorCell.visible)
            return separatorCell.x + separatorCell.width / 2;
        const last = pinnedRepeater.count > 0
            ? pinnedRepeater.itemAt(pinnedRepeater.count - 1) : null;
        return last ? last.x + last.width : 0;
    }
    function dropIcon(url, sourceRow, wasPinned, pointerX) {
        if (stackCount > 0 && url &&
            pointerX >= stackCell.x - 8 && pointerX <= stackCell.x + stackCell.width + 8) {
            moveToStack(url);
            draggingUrl = "";
            dropGuideX = -1;
            return;
        }
        const action = Logic.routeDrop(wasPinned, pointerX, pinnedDropBoundary(),
                                       visiblePinnedLaunchers.length, runningModel.count);
        if (action === "pin" && url) dropPinnedAt(url, pointerX);
        else if (action === "reorder-running") reorderRunningAt(sourceRow, pointerX);
        else if (action === "pin" && !url) {
            draggingUrl = "";
            dropGuideX = -1;
            console.warn("KofDock: aplicativo sem .desktop associado: não é possível fixar");
        }
        else { draggingUrl = ""; dropGuideX = -1; }
        console.info("KofDock: arrastar concluído: " + action);
    }
    function updateInputMask() {
        // Static envelope: changing the Wayland input region every frame was
        // stalling the UI, especially on high-refresh-rate displays.
        if (dock.hideRequested && !revealed) {
            const peekWidth = Math.min(dock.width - 8, Math.max(90,
                   iconRow.implicitWidth + 2 * sidePadding + maxVisualExtra));
            dockController.setInputArea(Math.round(plate.x + (plate.width - peekWidth) / 2),
                                        Math.max(0, height - edgeTriggerHeight),
                                        Math.ceil(peekWidth), edgeTriggerHeight);
            return;
        }
        const regions = [];
        // A CONTINUOUS hit region keeps hover alive while crossing the divider
        // and transparent gaps. It is limited to the Dock, never the whole screen.
        const maxPlateWidth = Math.min(dock.width - 8, Math.max(90,
              iconRow.implicitWidth + 2 * sidePadding + maxVisualExtra));
        const hoverTop = Math.max(0, dock.height - dockSettings.bottomGap - dock.itemHeight - 12);
        // Anchor the input envelope to maximum possible zoom, not plate.x
        // (which animates). Otherwise the icon can move outside a stale mask.
        const stableLeft = iconRow.x - dock.sidePadding - dock.maxVisualExtra / 2;
        regions.push(Qt.rect(stableLeft - 12, hoverTop,
                             maxPlateWidth + 24, dock.itemHeight + 20));
        const overhang = Math.ceil(maxVisualExtra / 2 +
                   dockSettings.iconSize * (dockSettings.zoomScale - 1) / 2 + 8);
        for (const repeater of [pinnedRepeater, runningRepeater]) {
            for (let i = 0; i < repeater.count; i++) {
                const cell = repeater.itemAt(i);
                if (!cell || !cell.visible) continue;
                const p = cell.mapToItem(entireArea, 0, 0);
                regions.push(Qt.rect(p.x - overhang, p.y - 3,
                          cell.width + 2 * overhang, cell.height + 7));
            }
        }
        if (stackCount > 0) {
            const p = stackCell.mapToItem(entireArea, 0, 0);
            regions.push(Qt.rect(p.x - overhang, p.y - 3,
                                 stackCell.width + 2 * overhang, stackCell.height + 7));
        }
        if (fanOpen) {
            // Never mask the 660px-wide empty fan rectangle. It used to steal
            // pointer input from other dock icons and keep the wrong fan open.
            for (const rect of fanPanel.inputRegions()) {
                const pt = fanPanel.mapToItem(entireArea, rect.x, rect.y);
                regions.push(Qt.rect(pt.x, pt.y, rect.width, rect.height));
            }
        }
        dockController.setInputRegions(regions);
    }
    function deferMask() { maskRefresh.restart(); }
    Timer {
        id: maskRefresh
        interval: 0
        repeat: false
        onTriggered: dock.updateInputMask()
    }
    function followCursorFromCell(cell, x, y) {
        const p = cell.mapToItem(entireArea, x, y);
        followCursor(p.x);
    }
    function followCursor(globalX) {
        if (draggingUrl || !revealed) return;
        const px = globalX - iconRow.x;
        const centers = [];
        for (let i = 0; i < pinnedRepeater.count; ++i) {
            const cell = pinnedRepeater.itemAt(i);
            if (cell && cell.visible) centers.push(cell.x + cell.width / 2);
        }
        for (let i = 0; i < runningRepeater.count; ++i) {
            const cell = runningRepeater.itemAt(i);
            if (cell && cell.visible) centers.push(cell.x + cell.width / 2);
        }
        if (stackCount > 0) centers.push(stackCell.x + stackCell.width / 2);
        const newHover = Logic.hoverBetweenCenters(centers, px, dockSettings.iconSize + 10);
        // No model update when the cursor sits still / the event repeats.
        if (newHover !== hoverPosition) hoverPosition = newHover;
        hoveredIndex = newHover < 0 ? -1 : Math.round(newHover);
    }
    onWidthChanged: deferMask()
    onHeightChanged: deferMask()
    onRevealedChanged: {
        if (!revealed && fanOpen) closeFan();
        deferMask();
    }
    onFanOpenChanged: deferMask()
    onFanOwnerChanged: deferMask()
    onFanEntriesChanged: deferMask()
    onMaxVisualExtraChanged: deferMask()
    onTotalCellsChanged: { deferMask(); Qt.callLater(refreshVisibility); }

    onDraggingUrlChanged: {
        if (!draggingUrl && hideRequested && !dockHover.hovered && !anyMenuOpen)
            hideTimer.restart();
    }
    onOpenMenusChanged: {
        if (!anyMenuOpen && hideRequested && !dockHover.hovered)
            hideTimer.restart();
    }
    Component.onCompleted: { deferMask(); Qt.callLater(refreshVisibility); }

    Timer {
        interval: 1700
        repeat: false
        running: true
        onTriggered: console.info("KofDock: tarefas=" + taskModel.count +
                                 ", fixados=" + visiblePinnedLaunchers.length +
                                 ", não-fixados=" + runningModel.count +
                                 ", zoom=" + dockSettings.zoomScale)
    }
    Timer {
        id: hideTimer
        repeat: false
        interval: dockSettings.hideDelay
        onTriggered: {
            if (dock.hideRequested && !dockHover.hovered && !dock.anyMenuOpen && !dock.draggingUrl)
                dock.revealed = false;
        }
    }
    Connections {
        target: dockController
        function onScreenGeometryChanged() { dock.refreshVisibility(); dock.deferMask(); dock.closeFan(); }
    }
    Connections {
        target: dockSettings
        function onAutoHideChanged() { dock.refreshVisibility(); }
        function onSmartHideChanged() { dock.refreshVisibility(); }
        function onZoomScaleChanged() { dock.deferMask(); }
        function onZoomSpreadChanged() { dock.deferMask(); }
        function onSpacingChanged() { dock.deferMask(); }
        function onIconSizeChanged() { dock.deferMask(); }
        function onBottomGapChanged() { dock.deferMask(); dock.refreshVisibility(); }
        function onBackgroundOpacityChanged() { dock.deferMask(); }
        function onShadowEnabledChanged() { dock.deferMask(); }
        function onBorderEnabledChanged() { dock.deferMask(); }
        function onStackLaunchersChanged() { dock.closeFan(); dock.deferMask(); }
    }

    Item {
        id: entireArea
        anchors.fill: parent
        HoverHandler {
            id: dockHover
            // Observes motion throughout the Dock without taking clicks away
            // from the per-icon MouseAreas or the context menus.
            onPointChanged: {
                if (!hovered) return;
                dock.reconcileFanAtPointer(point.position);
                if (!dock.fanOpen ||
                    point.position.y >= iconRow.y + dock.itemHeight * 0.55)
                    dock.followCursor(point.position.x);
            }
            onHoveredChanged: {
                if (hovered) {
                    hideTimer.stop();
                    dock.revealed = true;
                    dock.followCursor(point.position.x);
                } else {
                    dock.hoverPosition = -1;
                    dock.hoveredIndex = -1;
                    if (dock.fanOpen) dock.requestFanClose(dock.fanOwner);
                    if (dock.hideRequested && !dock.draggingUrl && !dock.fanOpen) hideTimer.restart();
                }
            }
        }
        Rectangle {
            visible: dockSettings.shadowEnabled && dock.revealed
            x: plate.x + 2
            y: plate.y + 5
            width: plate.width
            height: plate.height
            radius: plate.radius
            color: "#29000000"
        }
        Rectangle {
            id: plate
            width: Math.min(Math.max(1, dock.width - 8),
                            Math.max(90, iconRow.implicitWidth + 2 * dock.sidePadding + dock.extraVisualWidth))
            height: dock.plateHeight
            x: Math.max(4, Math.min(dock.width - width - 4,
                  iconRow.x - dock.sidePadding - dock.extraVisualWidth / 2))
            y: dock.revealed ? dock.height - dockSettings.bottomGap - height : dock.height - dock.peekHeight
            color: Qt.rgba(dock.plateTint.r, dock.plateTint.g, dock.plateTint.b,
                           dockSettings.backgroundOpacity)
            radius: dockSettings.cornerRadius
            border.width: dockSettings.borderEnabled && dockSettings.backgroundOpacity > 0 ? 1 : 0
            border.color: Qt.rgba(0.92, 0.94, 1, 0.22)
            Behavior on y { NumberAnimation { duration: 210; easing.type: Easing.OutCubic } }
            Behavior on width {
                SmoothedAnimation { duration: dockSettings.animationMs; velocity: -1 }
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: backgroundMenu.popup()
            }
            Menu {
                id: backgroundMenu
                popupType: Popup.Window
                onOpened: dock.openMenus += 1
                onClosed: dock.openMenus = Math.max(0, dock.openMenus - 1)
                MenuItem { text: "Personalizar KofDock…"; onTriggered: dock.openSettings() }
            }
        }

        Row {
            id: iconRow
            // Keep pinned applications at stable screen coordinates while
            // new unpinned windows appear on the right. Do not re-center the
            // whole Row each time the KDE source model inserts a window.
            x: Logic.stableRowLeft(dock.width, visiblePinnedLaunchers.length,
                   dockSettings.iconSize + 10, dockSettings.spacing, implicitWidth)
            y: plate.y + plate.height - height - 3
            height: dock.itemHeight
            spacing: dockSettings.spacing

            Repeater {
                id: pinnedRepeater
                model: dock.visiblePinnedLaunchers
                onItemAdded: dock.deferMask()
                onItemRemoved: (index, item) => {
                    if (dock.fanOwner === item) {
                        dock.fanAnchorX = dock.liveFanAnchorX;
                        dock.closeFan();
                        // Item is about to be destroyed. No stale QML pointer
                        // may remain in a fan binding or dismissal timer.
                        dock.fanOwner = null;
                    }
                    dock.deferMask();
                }
                delegate: DockCell {
                    required property string modelData
                    hostWindow: dock
                    settings: dockSettings
                    taskManagerModel: taskModel
                    rowHeight: dock.itemHeight
                    model: dock.snapshotForLauncher(modelData)
                    sourceRow: dock.sourceRowForLauncher(modelData)
                    dockIndex: index
                    pinnedSection: true
                }
            }
            Item {
                id: separatorCell
                visible: dock.showDivider
                width: visible ? 21 : 0
                height: dock.itemHeight
                transform: Translate {
                    x: dock.dividerOffset
                    Behavior on x {
                        SmoothedAnimation { duration: dockSettings.animationMs; velocity: -1 }
                    }
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 14
                    width: 1
                    height: Math.round(dockSettings.iconSize * 0.65)
                    radius: 1
                    color: dockSettings.indicatorColor
                    opacity: 0.35
                }
                // Pointer tracking belongs to the outer HoverHandler: resetting
                // the wave here causes a visible snap when crossing the separator.
            }
            Repeater {
                id: runningRepeater
                model: runningModel
                onItemAdded: dock.deferMask()
                onItemRemoved: (index, item) => {
                    if (dock.fanOwner === item) {
                        dock.fanAnchorX = dock.liveFanAnchorX;
                        dock.closeFan();
                        // Item is about to be destroyed. No stale QML pointer
                        // may remain in a fan binding or dismissal timer.
                        dock.fanOwner = null;
                    }
                    dock.deferMask();
                }
                delegate: DockCell {
                    hostWindow: dock
                    settings: dockSettings
                    taskManagerModel: taskModel
                    rowHeight: dock.itemHeight
                    sourceRow: runningModel.sourceRow(index)
                    dockIndex: visiblePinnedLaunchers.length + index
                    pinnedSection: false
                }
            }
            Item {
                id: stackSeparatorCell
                visible: dock.showStackDivider
                width: visible ? 21 : 0
                height: dock.itemHeight
                transform: Translate {
                    x: dock.stackDividerOffset
                    Behavior on x { SmoothedAnimation { duration: dockSettings.animationMs; velocity: -1 } }
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 14
                    width: 1
                    height: Math.round(dockSettings.iconSize * 0.65)
                    radius: 1
                    color: dockSettings.indicatorColor
                    opacity: 0.35
                }
            }
            StackCell {
                id: stackCell
                visible: dock.stackCount > 0
                width: visible ? dockSettings.iconSize + 10 : 0
                height: dock.itemHeight
                hostWindow: dock
                settings: dockSettings
                dockIndex: dock.applicationCount
                launchers: dockSettings.stackLaunchers
            }
        }
        FanStack {
            id: fanPanel
            z: 2200
            width: Math.min(dock.width - 8, 690)
            height: Math.max(150, Math.min(dock.height - 8, dock.fanHeadroom + 65))
            // The stack's ICONS share the exact X axis of the animated base.
            x: Logic.fanPlacement(dock.liveFanAnchorX, dock.width, width).x
            y: Math.max(4, dock.fanBaseY - height + 38)
            anchorX: dock.liveFanAnchorX - x
            anchorY: dock.fanBaseY - y
            open: dock.fanOpen && dock.revealed
            entries: dock.fanEntries
            title: dock.fanOwner && dock.fanOwner.stackTitle ? dock.fanOwner.stackTitle : ""
            revealCycle: dock.fanRevealCycle
            onPanelHoveredChanged: {
                if (panelHovered) fanDismiss.stop();
                else dock.requestFanClose(dock.fanOwner);
            }
            onSelected: entry => dock.activateFanEntry(entry)
            onInputChanged: dock.deferMask()
        }
        Rectangle {
            visible: dock.draggingUrl.length > 0 && dock.dropGuideX >= 0
            x: iconRow.x + dock.dropGuideX - 1.5
            y: iconRow.y + 13
            width: 3
            height: dockSettings.iconSize
            radius: 2
            color: dockSettings.indicatorColor
            z: 2000
        }
    }

    SettingsWindow {
        id: settingsWin
        settings: dockSettings
        controller: dockController
        autostart: dockAutostart
        hostDock: dock
        visible: false
        onVisibleChanged: {
            if (!visible) dock.refreshVisibility();
        }
    }
}
