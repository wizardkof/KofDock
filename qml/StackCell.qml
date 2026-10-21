// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQml.Models
import org.kde.kirigami as Kirigami

// User-defined persistent stack: launchers are kept in KDE's TasksModel and
// moved into a single visual slot, rather than creating fake tasks.
Item {
    id: cell
    required property var hostWindow
    required property var settings
    required property int dockIndex
    required property var launchers
    readonly property string stackTitle: String(settings.stackName || "Aplicativos")
    readonly property bool pointerHover: pointer.containsMouse
    readonly property real zoomAmount: hostWindow.zoomLayout.scales[dockIndex] ?? 1
    readonly property real iconShift: hostWindow.zoomLayout.shifts[dockIndex] ?? 0
    readonly property real renderedShift: stackSlide.x
    z: zoomAmount

    function entries() {
        const result = [];
        for (let i = 0; i < launchers.length; i++) {
            const url = String(launchers[i]);
            // Resolve icon and display name only for the visible fan rows.
            // Building a stack with hundreds of entries must not synchronously
            // request hundreds of icons each time the pointer enters.
            result.push({kind: "launcher", url: url});
        }
        return result;
    }

    readonly property int unreadTotal: {
        dockServices.badgeRevision;
        if (!settings.showBadges) return 0;
        let total = 0;
        for (const url of launchers)
            total += Math.max(0, dockServices.badgeCount(String(url), "", ""));
        return total;
    }
    readonly property bool hasRunning: {
        hostWindow.taskRefreshRevision;
        return hostWindow.stackHasRunning(launchers);
    }

    // Same visual sizing/transform chain as DockCell. The stack badge and
    // running marker are children of the tile and therefore cannot lag behind.
    Item {
        id: tile
        readonly property real maxZoom: Math.max(1, settings.zoomScale)
        width: settings.iconSize * maxZoom * 0.94
        height: width
        x: (cell.width - width) / 2
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        transform: [
            Scale {
                origin.x: tile.width / 2
                origin.y: tile.height
                xScale: cell.zoomAmount / tile.maxZoom
                yScale: cell.zoomAmount / tile.maxZoom
                Behavior on xScale { SmoothedAnimation { duration: settings.animationMs; velocity: -1 } }
                Behavior on yScale { SmoothedAnimation { duration: settings.animationMs; velocity: -1 } }
            },
            Translate {
                id: stackSlide
                x: cell.iconShift
                Behavior on x { SmoothedAnimation { duration: settings.animationMs; velocity: -1 } }
            }
        ]
        Rectangle {
            anchors.fill: parent
            radius: 14 * tile.maxZoom
            readonly property bool dropHover: cell.hostWindow.draggingUrl.length > 0 &&
                cell.hostWindow.dropGuideX >= cell.x &&
                cell.hostWindow.dropGuideX <= cell.x + cell.width
            color: dropHover ? "#b02e4862" : "#6b293441"
            border.width: dropHover ? 2 : 1
            border.color: dropHover ? "#a3c7ff" : "#53677889"
            Behavior on color { ColorAnimation { duration: 130 } }
        }
        Repeater {
            model: Math.min(4, cell.launchers.length)
            delegate: Kirigami.Icon {
                required property int index
                width: (tile.width / 2) - 7 * tile.maxZoom
                height: width
                x: 4 * tile.maxZoom + (index % 2) * (tile.width / 2)
                y: 4 * tile.maxZoom + Math.floor(index / 2) * (tile.height / 2)
                source: dockServices.iconFor(String(cell.launchers[index]), "", "", "")
            }
        }
        Rectangle {
            id: stackBadge
            z: 140
            visible: cell.unreadTotal > 0
            width: Math.max(20 * tile.maxZoom,
                            unreadText.implicitWidth + 10 * tile.maxZoom)
            height: 20 * tile.maxZoom
            x: tile.width - width / 2
            y: 0
            radius: height / 2
            color: "#df354c"
            Text {
                id: unreadText
                anchors.centerIn: parent
                text: cell.unreadTotal > 99 ? "99+" : String(cell.unreadTotal)
                color: "white"
                font.pixelSize: 11 * tile.maxZoom
                font.bold: true
            }
        }
        Rectangle {
            id: stackRunningIndicator
            z: 130
            visible: cell.hasRunning && settings.showIndicators
            width: 6 * tile.maxZoom
            height: 4 * tile.maxZoom
            x: (tile.width - width) / 2
            y: tile.height + 6 * tile.maxZoom
            radius: height / 2
            color: settings.indicatorColor
            opacity: 0.85
        }
    }
    Timer {
        id: fanTimer
        interval: settings.fanDelay
        repeat: false
        onTriggered: {
            if (pointer.containsMouse && !hostWindow.draggingUrl && cell.launchers.length > 0)
                hostWindow.showStackForCell(cell, cell.entries());
        }
    }
    MouseArea {
        id: pointer
        anchors.fill: parent
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        hoverEnabled: true
        onEntered: {
            const eligible = settings.fanEnabled && cell.launchers.length > 0;
            const wasOpen = hostWindow.fanOpen;
            hostWindow.switchOpenFan(cell, eligible ? cell.entries() : []);
            if (!wasOpen && eligible) fanTimer.restart();
            else fanTimer.stop();
        }
        onExited: {
            fanTimer.stop();
            hostWindow.requestFanClose(cell);
        }
        onClicked: mouse => {
            if (mouse.button === Qt.RightButton) {
                fanTimer.stop();
                hostWindow.closeFan();
                stackMenu.popup();
            }
        }
    }
    ToolTip.visible: pointer.containsMouse && !hostWindow.fanOpen && !stackMenu.opened
    ToolTip.delay: 750
    ToolTip.text: cell.stackTitle + " · " + cell.launchers.length + " aplicativos"

    Menu {
        id: stackMenu
        popupType: Popup.Window
        onOpened: cell.hostWindow.openMenus += 1
        onClosed: cell.hostWindow.openMenus = Math.max(0, cell.hostWindow.openMenus - 1)
        MenuItem { text: "Configurar pilha…"; onTriggered: cell.hostWindow.openSettings() }
        Menu {
            id: removeMenu
            title: "Retirar da pilha"
            Instantiator {
                model: cell.launchers.length
                delegate: MenuItem {
                    required property int index
                    readonly property string launchUrl: String(cell.launchers[index])
                    text: dockServices.displayName(launchUrl, "", "", launchUrl)
                    onTriggered: Qt.callLater(() => cell.hostWindow.removeFromStack(launchUrl))
                }
                onObjectAdded: (index, object) => removeMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => removeMenu.removeItem(object)
            }
        }
        MenuItem {
            text: "Desfazer pilha (preservar aplicativos)"
            onTriggered: Qt.callLater(() => { cell.settings.stackLaunchers = []; })
        }
    }
}
