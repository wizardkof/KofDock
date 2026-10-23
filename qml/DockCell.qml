// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQml.Models
import org.kde.taskmanager as TM
import org.kde.kirigami as Kirigami
import "DockLogic.js" as Logic

Item {
    id: cell
    required property int index             // row in the visible section
    required property var model             // task model roles, through the proxy
    required property var hostWindow
    required property var settings
    required property var taskManagerModel  // original TasksModel
    required property int sourceRow         // original TasksModel row
    required property int dockIndex         // icon's visible index across BOTH sections
    required property bool pinnedSection
    required property real rowHeight

    readonly property string launcherUrl: String(model.LauncherUrlWithoutIcon || model.LauncherUrl || "")
    readonly property var displayIcon: dockServices.iconFor(launcherUrl, String(model.AppId || ""),
                                       String(model.StorageId || ""), model.decoration)
    readonly property int countWindows: Math.max(0, Number(model.ChildCount || 0))
    // The CPU keeps Kirigami.Icon at one texture size; only the GPU transform
    // changes. Avoid regenerating icons while the mouse moves.
    readonly property real zoomAmount: hostWindow.draggingUrl.length > 0
        ? 1 : (hostWindow.zoomLayout.scales[dockIndex] ?? 1)
    readonly property real iconShift: hostWindow.draggingUrl.length > 0
        ? 0 : (hostWindow.zoomLayout.shifts[dockIndex] ?? 0)
    // Actual animated translation, not the target value that may be ahead.
    readonly property real renderedShift: slideTransform.x
    readonly property real launchOffset: hostWindow.bouncingKey ===
            hostWindow.launchKey(launcherUrl, String(model.AppId || ""))
        ? Logic.launchBounceOffset(hostWindow.bounceProgress, settings.iconSize) : 0
    property bool dragging: false
    property real dragOffset: 0
    readonly property int unreadCount: {
        dockServices.badgeRevision; // dependency to refresh on Unity D-Bus updates
        return settings.showBadges ? dockServices.badgeCount(launcherUrl,
                    String(model.AppId || ""), String(model.StorageId || "")) : 0;
    }
    readonly property bool pointerHover: pointer.containsMouse
    readonly property string stackTitle: String(model.AppName || model.display || "Janelas")
    // A window group is represented as a stack of actual window *icons*.
    // The fan is displayed ONLY on pointer hover, not by left or right click.
    function windowEntries() {
        const out = [];
        if (!model.IsGroupParent || countWindows < 2) return out;
        for (let child = 0; child < countWindows; child++) {
            const idx = taskManagerModel.makeModelIndex(sourceRow, child);
            out.push({ kind: "window", sourceRow: sourceRow, childRow: child,
                title: Logic.compactTitle(String(taskManagerModel.data(idx, Qt.DisplayRole) ||
                        "Janela " + (child + 1)), 31),
                icon: dockServices.iconFor(launcherUrl,
                      String(model.AppId || ""), String(model.StorageId || ""),
                      taskManagerModel.data(idx, Qt.DecorationRole)) });
        }
        return out;
    }

    // Keep slots FIXED SIZE to avoid the mouse-geometry feedback loop.
    visible: true
    width: settings.iconSize + 10
    height: rowHeight
    z: dragging ? 1000 : zoomAmount

    Kirigami.Icon {
        id: applicationIcon
        // Render once at MAX zoom resolution to keep icons sharp, then
        // scale the existing scene-graph node down at rest.
        width: cell.settings.iconSize * cell.settings.zoomScale * 0.94
        height: width
        x: (cell.width - width) / 2
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        source: cell.displayIcon
        active: cell.model.IsStartup
        transform: [
            Scale {
                id: zoomTransform
                origin.x: applicationIcon.width / 2
                origin.y: applicationIcon.height
                xScale: cell.zoomAmount / Math.max(1, cell.settings.zoomScale)
                yScale: cell.zoomAmount / Math.max(1, cell.settings.zoomScale)
                Behavior on xScale {
                    enabled: cell.settings.animationMs > 0
                    SmoothedAnimation { duration: cell.settings.animationMs; velocity: -1 }
                }
                Behavior on yScale {
                    enabled: cell.settings.animationMs > 0
                    SmoothedAnimation { duration: cell.settings.animationMs; velocity: -1 }
                }
            },
            Translate {
                id: slideTransform
                x: cell.iconShift
                Behavior on x {
                    enabled: cell.settings.animationMs > 0
                    SmoothedAnimation { duration: cell.settings.animationMs; velocity: -1 }
                }
            },
            Translate { x: cell.dragging ? cell.dragOffset : 0;
                        y: (cell.dragging ? -14 : 0) + cell.launchOffset }
        ]
        // KDE/Unity LauncherEntry unread count: only applications that publish it.
        Rectangle {
            id: notificationBadge
            z: 140
            visible: cell.unreadCount > 0
            width: Math.max(19 * cell.settings.zoomScale,
                            badgeText.implicitWidth + 10 * cell.settings.zoomScale)
            height: 20 * cell.settings.zoomScale
            radius: height / 2
            color: "#df354c"
            // Local to the icon: inherits exactly the same hover scale and slide.
            x: applicationIcon.width - width / 2
            y: 1
            Text {
                id: badgeText
                anchors.centerIn: parent
                text: cell.unreadCount > 99 ? "99+" : String(cell.unreadCount)
                color: "white"
                font.pixelSize: 11 * cell.settings.zoomScale
                font.bold: true
            }
        }
        // The running marker lives INSIDE the same transformed scene node as
        // the icon and its unread badge. It follows every zoom/slide frame.
        Rectangle {
            id: runningIndicator
            z: 130
            visible: cell.settings.showIndicators &&
                     (cell.model.IsWindow || cell.model.IsGroupParent || cell.model.IsStartup)
            width: (cell.model.IsActive ? 11 : 5) * cell.settings.zoomScale
            height: 4 * cell.settings.zoomScale
            x: (applicationIcon.width - width) / 2
            y: applicationIcon.height + 6 * cell.settings.zoomScale
            radius: height / 2
            color: cell.model.IsDemandingAttention ? "#ffb86c" : cell.settings.indicatorColor
            opacity: cell.model.IsActive ? 1 : 0.85
            Behavior on width { NumberAnimation { duration: 130 } }
        }

    }

    function publishGeometry() {
        if (!settings.publishMinimizeGeometry || sourceRow < 0 ||
            (!model.IsWindow && !model.IsGroupParent)) return;
        const p = applicationIcon.mapToGlobal(0, 0);
        if (!Number.isFinite(p.x) || !Number.isFinite(p.y)) return;
        const q = applicationIcon.mapToGlobal(applicationIcon.width, applicationIcon.height);
        const rect = Qt.rect(Math.round(Math.min(p.x, q.x)), Math.round(Math.min(p.y, q.y)),
                             Math.max(1, Math.round(Math.abs(q.x - p.x))),
                             Math.max(1, Math.round(Math.abs(q.y - p.y))));
        taskManagerModel.requestPublishDelegateGeometry(taskManagerModel.makeModelIndex(sourceRow), rect, cell);
    }
    Timer {
        interval: 450
        repeat: true
        running: cell.settings.publishMinimizeGeometry &&
                 (cell.model.IsWindow || cell.model.IsGroupParent)
        onTriggered: cell.publishGeometry()
    }

    MouseArea {
        id: pointer
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        hoverEnabled: true
        preventStealing: true
        property real pressX: 0
        property real pressY: 0

        onEntered: {
            cell.hostWindow.followCursorFromCell(cell, pointer.mouseX, pointer.mouseY);
            const eligible = cell.settings.fanEnabled &&
                             cell.model.IsGroupParent && cell.countWindows > 1;
            const wasOpen = cell.hostWindow.fanOpen;
            cell.hostWindow.switchOpenFan(cell, eligible ? cell.windowEntries() : []);
            if (!wasOpen && eligible) fanTimer.restart();
            else fanTimer.stop();
        }
        onExited: {
            fanTimer.stop();
            cell.hostWindow.requestFanClose(cell);
        }
        onPositionChanged: mouse => {
            // Two *stationary* seconds, not two seconds of continuous movement.
            // Hover timer does not restart on pointer jitter: the stack is
            // stable once the cursor enters the icon.
            if ((pressedButtons & Qt.LeftButton) &&
                (cell.launcherUrl.length > 0 || (!cell.pinnedSection && cell.sourceRow >= 0))) {
                const dx = mouse.x - pressX;
                const dy = mouse.y - pressY;
                if (!cell.dragging && Math.hypot(dx, dy) > 9) {
                    cell.dragging = true;
                    cell.hostWindow.draggingUrl = cell.launcherUrl.length > 0
                        ? cell.launcherUrl : "running-row:" + cell.sourceRow;
                    cell.hostWindow.draggingWasPinned = cell.pinnedSection;
                    cell.hostWindow.hoverPosition = -1;
                    fanTimer.stop();
                    cell.hostWindow.closeFan();
                }
                if (cell.dragging) {
                    cell.dragOffset = dx;
                    const p = cell.mapToItem(cell.parent, mouse.x, mouse.y);
                    cell.hostWindow.updateDropGuide(p.x);
                    return;
                }
            }
            cell.hostWindow.followCursorFromCell(cell, mouse.x, mouse.y);
        }
        onPressed: mouse => {
            pressX = mouse.x;
            pressY = mouse.y;
        }
        onReleased: mouse => {
            if (cell.dragging) {
                const url = cell.launcherUrl;
                const p = cell.mapToItem(cell.parent, mouse.x, mouse.y);
                cell.dragging = false;
                cell.dragOffset = 0;
                cell.hostWindow.dropIcon(url, cell.sourceRow, cell.pinnedSection, p.x);
                return;
            }
            // Newly pinned apps can be absent from the KDE task list until
            // the next launch. They still retain a working context menu.
            if (mouse.button === Qt.RightButton) {
                fanTimer.stop();
                cell.hostWindow.closeFan();
                appMenu.popup();
            } else if (mouse.button === Qt.MiddleButton) {
                cell.hostWindow.closeFan();
                cell.hostWindow.playLaunchBounce(cell);
                if (cell.sourceRow >= 0)
                    cell.taskManagerModel.requestNewInstance(cell.taskManagerModel.makeModelIndex(cell.sourceRow));
                else dockServices.launchLauncher(cell.launcherUrl);
            } else if (mouse.button === Qt.LeftButton) {
                cell.hostWindow.closeFan();
                // The real macOS Dock bounces for launch, not when merely
                // switching to an already running application.
                if (cell.model.IsLauncher || cell.model.IsStartup)
                    cell.hostWindow.playLaunchBounce(cell);
                cell.hostWindow.activate(cell.sourceRow, cell.model);
            }
        }
        onCanceled: {
            cell.dragging = false;
            cell.dragOffset = 0;
            cell.hostWindow.draggingUrl = "";
            cell.hostWindow.dropGuideX = -1;
            fanTimer.stop();
            cell.hostWindow.closeFan();
        }
    }
    ToolTip.visible: pointer.containsMouse && !appMenu.visible && !cell.hostWindow.fanOpen && !cell.dragging
    ToolTip.delay: 600
    ToolTip.text: dockServices.displayName(cell.launcherUrl,
                     String(cell.model.AppId || ""), String(cell.model.StorageId || ""),
                     String(cell.model.AppName || cell.model.display || ""))


    // No window thumbnails / no KWin screencast permission needed.
    Timer {
        id: fanTimer
        interval: cell.settings.fanDelay
        repeat: false
        onTriggered: {
            if (pointer.containsMouse && !cell.dragging && !appMenu.opened &&
                cell.settings.fanEnabled && cell.model.IsGroupParent && cell.countWindows > 1)
                cell.hostWindow.showStackForCell(cell, cell.windowEntries());
        }
    }

    // Native-looking compact context menu. Clicking a group icon activates its
    // last-used window; the full window list is only one submenu away.
    Menu {
        id: appMenu
        // Keep rendering in the persistent dock surface. A running task can
        // disappear while its menu is closing; it must not own a native window.
        popupType: Popup.Item
        parent: cell.hostWindow.contentItem
        onOpened: {
            cell.hostWindow.openMenus += 1;
            cell.hostWindow.activeAppMenu = appMenu;
            cell.hostWindow.deferMask();
        }
        onClosed: {
            cell.hostWindow.openMenus = Math.max(0, cell.hostWindow.openMenus - 1);
            if (cell.hostWindow.activeAppMenu === appMenu)
                cell.hostWindow.activeAppMenu = null;
            cell.hostWindow.deferMask();
        }
        onXChanged: cell.hostWindow.deferMask()
        onYChanged: cell.hostWindow.deferMask()

        Instantiator {
            model: cell.model.IsGroupParent ? 1 : 0
            delegate: Menu {
                id: windowSubmenu
                title: "Janelas abertas (" + cell.countWindows + ")"
                Instantiator {
                    model: Math.min(30, cell.countWindows)
                    delegate: MenuItem {
                        required property int index
                        readonly property var windowIndex: cell.taskManagerModel.makeModelIndex(cell.sourceRow, index)
                        text: Logic.compactTitle(String(cell.taskManagerModel.data(windowIndex, 0) ||
                                                        "Janela " + (index + 1)), 48)
                        checkable: true
                        checked: Boolean(cell.taskManagerModel.data(windowIndex, TM.AbstractTasksModel.IsActive))
                        onTriggered: cell.hostWindow.activateGroupWindow(cell.sourceRow, index)
                    }
                    onObjectAdded: (index, object) => windowSubmenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => windowSubmenu.removeItem(object)
                }
            }
            onObjectAdded: (index, object) => appMenu.insertMenu(0, object)
            onObjectRemoved: (index, object) => appMenu.removeMenu(object)
        }

        MenuItem {
            text: "Abrir nova janela"
            onTriggered: {
                const row = cell.sourceRow;
                cell.hostWindow.playLaunchBounce(cell);
                if (row >= 0) cell.taskManagerModel.requestNewInstance(cell.taskManagerModel.makeModelIndex(row));
                else dockServices.launchLauncher(cell.launcherUrl);
            }
        }
        MenuItem {
            text: "Adicionar à pilha de aplicativos"
            enabled: cell.launcherUrl.length > 0 &&
                !cell.settings.stackLaunchers.some(url =>
                    cell.hostWindow.normalizedUrl(url) === cell.hostWindow.normalizedUrl(cell.launcherUrl))
            onTriggered: Qt.callLater(() => cell.hostWindow.moveToStack(cell.launcherUrl))
        }
        MenuItem {
            text: cell.pinnedSection ? "Desafixar do Dock" : "Fixar no Dock"
            enabled: cell.launcherUrl.length > 0
            onTriggered: {
                // Capture values BEFORE row invalidation; never access a deleted
                // delegate in the queued callback.
                const host = cell.hostWindow;
                const url = cell.launcherUrl;
                Qt.callLater(function () { host.pinOrUnpin(url); });
            }
        }
        MenuItem {
            text: cell.model.IsMinimized ? "Restaurar janela" : "Minimizar janela"
            visible: cell.model.IsWindow && !cell.model.IsGroupParent
            enabled: visible
            onTriggered: if (cell.sourceRow >= 0)
                cell.taskManagerModel.requestToggleMinimized(cell.taskManagerModel.makeModelIndex(cell.sourceRow))
        }
        MenuItem {
            text: "Fechar janela"
            visible: cell.model.IsWindow && !cell.model.IsGroupParent && cell.model.IsClosable
            enabled: visible
            onTriggered: if (cell.sourceRow >= 0)
                cell.hostWindow.closeTaskWindow(cell.sourceRow)
        }
        MenuSeparator {}
        MenuItem {
            text: "Personalizar KofDock…"
            onTriggered: cell.hostWindow.openSettings()
        }
    }
}
