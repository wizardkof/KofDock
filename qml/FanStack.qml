// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami
import "DockLogic.js" as Logic

// Curved, Cairo-Dock-style fan drawn in the *existing* LayerShell surface.
// There is no 7-item grid fallback and no 32-item truncation. Large lists are
// windowed to the vertical space available, with wheel/touchpad navigation.
// Only the narrow icon/label path accepts input, never this transparent panel.
Item {
    id: fan
    objectName: "kofdock-fan"
    property bool open: false
    property var entries: []
    property string title: ""
    property real anchorX: width / 2
    property real anchorY: height - 46
    property real phase: 0
    property int revealCycle: 0
    property int scrollRows: 0
    property int hoveredLeaves: 0

    readonly property int iconExtent: 34
    readonly property int rowHeight: 41
    readonly property int visibleCapacity: Logic.fanVisibleCapacity(anchorY, 8)
    readonly property var scrollWindow: Logic.fanScrollWindow(entries.length, visibleCapacity, scrollRows)
    readonly property int visibleRows: scrollWindow.shown
    readonly property bool hasOverflow: entries.length > visibleCapacity
    readonly property bool labelsLeft: Logic.fanLabelsLeft(anchorX, width, 142, 86)
    readonly property int arcDirection: Logic.fanCurveDirection(anchorX, width, 86)
    readonly property real curveLimit: Math.min(86, Math.max(0,
        arcDirection < 0 ? anchorX - 38 : width - anchorX - 38))
    readonly property bool panelHovered: hoveredLeaves > 0 || bridge.containsMouse

    signal selected(var entry)
    signal inputChanged()

    function leafHover(delta) {
        hoveredLeaves = Math.max(0, hoveredLeaves + delta);
    }
    function scrollBy(delta) {
        if (!open || !hasOverflow || !delta) return;
        const next = Logic.fanScrollWindow(entries.length, visibleCapacity,
                                           scrollRows + delta).start;
        if (next !== scrollRows) scrollRows = next;
    }
    // Called when the *owner* changes. Preserve the whole list and reset its
    // viewport, rather than imposing an arbitrary maximum number of windows.
    onEntriesChanged: {
        scrollRows = 0;
        inputChanged();
    }
    onVisibleRowsChanged: inputChanged()
    // Follow the animated base icon, but throttle Wayland input-region updates.
    // Recomputing input masks on every GPU animation frame caused stutters.
    Timer {
        id: maskSchedule
        interval: 80
        repeat: false
        onTriggered: fan.inputChanged()
    }
    onAnchorXChanged: { if (open && !maskSchedule.running) maskSchedule.start(); }
    onAnchorYChanged: {
        const allowed = Logic.fanScrollWindow(entries.length, visibleCapacity,
                                               scrollRows).start;
        if (allowed !== scrollRows) scrollRows = allowed;
        inputChanged();
    }
    onScrollRowsChanged: inputChanged()

    // Stable Wayland input envelope. Rebuilt only when a different owner,
    // output or scroll page is selected, never on every frame of the animation.
    function inputRegions() {
        const result = [];
        if (!open || entries.length === 0) return result;
        result.push(Logic.fanBridgeGeometry(anchorX, anchorY));
        const minY = leafViewport.y;
        const maxY = leafViewport.y + leafViewport.height;
        for (let i = 0; i < visibleRows; ++i) {
            const leaf = leaves.itemAt(i);
            const labelWidth = leaf ? leaf.labelWidth : 142;
            const pos = Logic.fanLeafPosition(i, visibleRows, anchorX, anchorY,
                       arcDirection, curveLimit, labelWidth, labelsLeft);
            const label = Logic.fanLabelBounds(pos.iconCenterX, pos.iconCenterY,
                          labelsLeft, labelWidth,
                          Logic.fanLabelTilt(i, visibleRows), iconExtent, 3, 21);
            // The icon and the rotated label have small, separate input regions.
            // This avoids capturing the unrelated app icons below the fan.
            const regions = [
                {x: pos.iconCenterX - iconExtent / 2 - 7,
                 y: pos.iconCenterY - iconExtent / 2 - 7,
                 width: iconExtent + 14, height: iconExtent + 14},
                {x: label.x - 6, y: label.y - 6,
                 width: label.width + 12, height: label.height + 12}
            ];
            for (const r of regions) {
                const x1 = Math.max(0, r.x), x2 = Math.min(width, r.x + r.width);
                const y1 = Math.max(minY, r.y), y2 = Math.min(maxY, r.y + r.height);
                if (x2 > x1 && y2 > y1)
                    result.push({x: x1, y: y1, width: x2 - x1, height: y2 - y1});
            }
        }
        return result;
    }

    visible: (open || phase > 0.001) && entries.length > 0

    // Only the tiny gap between the base launcher and the first icon is a
    // pointer bridge. A rectangle covering the entire fan blocks other apps.
    MouseArea {
        id: bridge
        z: -1
        enabled: fan.open
        visible: enabled
        readonly property var area: Logic.fanBridgeGeometry(fan.anchorX, fan.anchorY)
        x: area.x
        y: area.y
        width: area.width
        height: area.height
        acceptedButtons: Qt.NoButton
        hoverEnabled: true
    }

    NumberAnimation {
        id: unfold
        target: fan
        property: "phase"
        to: 1
        duration: 345
        easing.type: Easing.OutCubic
    }
    NumberAnimation {
        id: fold
        target: fan
        property: "phase"
        to: 0
        duration: 135
        easing.type: Easing.OutCubic
    }
    onOpenChanged: {
        if (open) {
            fold.stop();
            unfold.from = phase;
            unfold.start();
        } else {
            unfold.stop();
            fold.from = phase;
            fold.start();
        }
    }
    onRevealCycleChanged: {
        if (open) {
            unfold.stop();
            fold.stop();
            phase = 0;
            unfold.from = 0;
            unfold.start();
        }
    }

    Item {
        id: leafViewport
        x: 0
        y: 6
        width: fan.width
        height: Math.max(0, fan.anchorY - y - 20)
        clip: true

        // Repeater instantiates AT MOST visibleCapacity leaves. All entries
        // beyond that stay reachable by scrolling the same tilted curved fan.
        Repeater {
            id: leaves
            model: fan.visibleRows
            delegate: Item {
                id: leaf
                required property int index
                readonly property int entryIndex: fan.scrollWindow.start + index
                readonly property var fanEntry: fan.entries[entryIndex] || ({})
                readonly property real progress: Logic.fanStaggerProgress(
                    fan.phase, index, fan.visibleRows)
                readonly property real labelWidth: Math.min(142,
                    Math.max(64, labelMetrics.width + 14))
                readonly property int iconExtent: fan.iconExtent
                readonly property int gap: 3
                // Parent hit bounds include the rotated flag tip even when it
                // rises above the unrotated 41px row; otherwise Wayland can
                // report hover loss at precisely the visible caption.
                readonly property int hoverTopSpace: 78
                readonly property int lateralBleed: 8
                readonly property var placement: Logic.fanLeafPosition(
                    index, fan.visibleRows, fan.anchorX, fan.anchorY,
                    fan.arcDirection, fan.curveLimit, labelWidth, fan.labelsLeft, progress)
                readonly property bool highlighted: iconHit.containsMouse || labelHit.containsMouse
                property bool hoverRegistered: false
                width: placement.width + 2 * lateralBleed
                height: placement.height + hoverTopSpace
                x: placement.leafX - lateralBleed
                y: placement.leafY - leafViewport.y - hoverTopSpace
                z: index + 1
                opacity: progress
                // Keep one origin: every leaf unfolds from the icon in the Dock.
                transform: Scale {
                    origin.x: leaf.lateralBleed + (fan.labelsLeft
                              ? leaf.placement.width - leaf.iconExtent / 2
                              : leaf.iconExtent / 2)
                    origin.y: leaf.hoverTopSpace + fan.rowHeight / 2
                    xScale: 0.42 + 0.58 * leaf.progress
                    yScale: 0.42 + 0.58 * leaf.progress
                }
                onHighlightedChanged: {
                    if (hoverRegistered !== highlighted) {
                        hoverRegistered = highlighted;
                        fan.leafHover(highlighted ? 1 : -1);
                    }
                }
                Component.onDestruction: {
                    if (hoverRegistered) fan.leafHover(-1);
                }
                function activate() {
                    if (progress >= 0.7) fan.selected(fanEntry);
                }
                function handleWheel(wheel) {
                    const amount = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y
                                                             : wheel.pixelDelta.y;
                    if (amount !== 0) {
                        fan.scrollBy(amount < 0 ? 1 : -1);
                        wheel.accepted = true;
                    }
                }
                TextMetrics {
                    id: labelMetrics
                    font: nameText.font
                    text: nameText.text
                }
                Kirigami.Icon {
                    id: appIcon
                    x: leaf.lateralBleed + leaf.placement.iconX
                    y: leaf.hoverTopSpace + (fan.rowHeight - leaf.iconExtent) / 2
                    width: leaf.iconExtent
                    height: width
                    source: leaf.fanEntry.icon || (leaf.fanEntry.url
                        ? dockServices.iconFor(String(leaf.fanEntry.url), "", "", "")
                        : "application-x-executable")
                    active: leaf.highlighted
                    rotation: (fan.labelsLeft ? 1 : -1) *
                        Logic.fanIconTilt(leaf.index, fan.visibleRows) * leaf.progress
                    transformOrigin: Item.Center
                    MouseArea {
                        id: iconHit
                        anchors.fill: parent
                        anchors.margins: -4
                        enabled: fan.open
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: leaf.activate()
                        onWheel: wheel => leaf.handleWheel(wheel)
                    }
                }
                // Classic Cairo-Dock flag: compact, dark, visually attached to
                // the icon. Rotate about the joining edge, *not* the centre;
                // centre rotation was separating the caption from its icon.
                Rectangle {
                    id: nameChip
                    x: leaf.lateralBleed + leaf.placement.labelX
                    y: leaf.hoverTopSpace + (fan.rowHeight - 21) / 2
                    width: leaf.labelWidth
                    height: 21
                    radius: 2
                    color: leaf.highlighted ? "#f01a1f29" : "#ed05060a"
                    border.width: 1
                    border.color: leaf.highlighted ? "#8e94a4b9" : "#515a6576"
                    rotation: (fan.labelsLeft ? 1 : -1) *
                        Logic.fanLabelTilt(leaf.index, fan.visibleRows) * leaf.progress
                    transformOrigin: fan.labelsLeft ? Item.Right : Item.Left
                    Behavior on color { ColorAnimation { duration: 95 } }
                    Text {
                        id: nameText
                        anchors.fill: parent
                        anchors.leftMargin: 7
                        anchors.rightMargin: 7
                        text: String(leaf.fanEntry.title || (leaf.fanEntry.url
                            ? dockServices.displayName(String(leaf.fanEntry.url), "", "", "Aplicativo")
                            : "Aplicativo")).replace(/\s+/g, " ").trim()
                        color: "#ffffffff"
                        font.family: Qt.application.font.family
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        renderType: Text.QtRendering
                        horizontalAlignment: fan.labelsLeft ? Text.AlignRight : Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                    MouseArea {
                        id: labelHit
                        anchors.fill: parent
                        enabled: fan.open
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: leaf.activate()
                        onWheel: wheel => leaf.handleWheel(wheel)
                    }
                }
            }
        }
    }

    // This hint is only relevant when more rows exist than fit on the screen.
    // It does not introduce a second layout or capture mouse input.
    Rectangle {
        visible: fan.open && fan.hasOverflow
        anchors.top: parent.top
        anchors.topMargin: 3
        anchors.horizontalCenter: parent.horizontalCenter
        width: scrollHint.implicitWidth + 24
        height: 23
        radius: 11
        color: "#b4000000"
        opacity: Math.min(1, fan.phase * 2)
        Text {
            id: scrollHint
            anchors.centerIn: parent
            text: qsTr("%1–%2 / %3 · rolar").arg(fan.scrollWindow.start + 1)
                  .arg(fan.scrollWindow.start + fan.visibleRows).arg(fan.entries.length)
            color: "#f4f4f6"
            font.pixelSize: 11
            font.weight: Font.Medium
        }
    }
}
