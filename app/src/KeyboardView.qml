// SPDX-License-Identifier: GPL-3.0-or-later
// The keyboard drawn from backend.layout (quarter key widths), scaled to the item's width.
// The page decides what each key says and which keys are selected. A click reports the
// keyboard modifiers held (Ctrl/Shift add to the selection). Dragging draws a box and reports
// every key it touches, with the modifiers held at the end of the drag.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Item {
    id: view

    property var keys: backend.layout
    property var labelFor: function (id) { return "" }  // text on a key
    property var markedFor: function (id) { return false }  // small dot: differs from factory
    property var colorFor: function (id) { return "" }  // fills the key with a colour when not ""
    property var selectedKeys: []
    signal keyClicked(int id, int modifiers)
    signal boxSelected(var ids, int modifiers)

    // While a box is drawn, the keys shown as selected: the box, plus the selection when
    // Ctrl or Shift will add to it.
    property var boxKeys: []
    property bool boxAdds: false
    readonly property bool boxing: mouse.boxing
    readonly property var shownKeys: boxing ? (boxAdds ? selectedKeys.concat(boxKeys) : boxKeys) : selectedKeys
    property int hoveredKey: -1

    readonly property int columns: 66  // 16.5 keys
    readonly property int rows: 26     // 6 rows and the gaps
    readonly property real unit: width / columns
    readonly property real gap: Math.max(2, unit * 0.35)

    // Keys grow with the window up to this size (one key width, in pixels), then the drawing
    // stays put: on a wide window it would push everything else off screen.
    property real maxKeySize: 56
    implicitWidth: columns * maxKeySize / 4
    implicitHeight: rows * unit

    Repeater {
        model: view.keys
        delegate: Rectangle {
            id: cap
            required property var modelData
            readonly property int keyId: modelData.id
            readonly property bool selected: view.shownKeys.indexOf(keyId) >= 0
            readonly property string fill: view.colorFor(keyId)
            // Light colours get dark text, dark ones light text.
            readonly property bool lightFill: fill !== "" && (0.299 * Qt.color(fill).r + 0.587 * Qt.color(fill).g + 0.114 * Qt.color(fill).b) > 0.55

            objectName: "key_" + keyId
            x: modelData.x * view.unit + view.gap / 2
            y: modelData.y * view.unit + view.gap / 2
            width: modelData.w * view.unit - view.gap
            height: 4 * view.unit - view.gap
            radius: Math.min(theme.radius, 6)
            // A filled key keeps its colour when selected; a thick accent ring marks it instead.
            color: fill !== "" ? fill : selected ? theme.accent : (view.hoveredKey === keyId ? "#2a2a31" : "#202026")
            border.width: fill !== "" && selected ? 3 : 1
            border.color: selected ? theme.accent : fill !== "" && view.hoveredKey === keyId ? "#ffffff" : "#2e2e36"


            Label {
                anchors.fill: parent
                anchors.margins: view.unit < 10 ? 1 : 3
                text: view.labelFor(cap.keyId)
                color: cap.fill !== "" ? (cap.lightFill ? "#0e0e10" : "#f0f0f0") : cap.selected ? "#0e0e10" : Material.foreground
                // Sized from the key, so every label starts at the same size; only long names
                // shrink on their own, before they are cut.
                font.pixelSize: Math.max(8, Math.min(13, Math.round(view.unit * 1.5)))
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: 7
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Rectangle {
                visible: view.markedFor(cap.keyId)
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 4
                width: 6
                height: 6
                radius: 3
                color: cap.selected ? "#0e0e10" : theme.accent
            }

            Rectangle {  // keeps the ring visible on a key the accent's own colour
                visible: cap.fill !== "" && cap.selected
                anchors.fill: parent
                anchors.margins: 3
                radius: Math.max(0, parent.radius - 3)
                color: "transparent"
                border.width: 1
                border.color: "#0e0e10"
            }
        }
    }

    // Key rectangles in the view's coordinates, the same as the drawn caps.
    function keyRect(k) {
        return Qt.rect(k.x * unit + gap / 2, k.y * unit + gap / 2, k.w * unit - gap, 4 * unit - gap)
    }
    function keyAt(px, py) {
        for (const k of keys) {
            const r = keyRect(k)
            if (px >= r.x && px < r.x + r.width && py >= r.y && py < r.y + r.height)
                return k.id
        }
        return -1
    }
    function keysIn(box) {
        return keys.filter(k => {
            const r = keyRect(k)
            return r.x < box.x + box.width && box.x < r.x + r.width
                && r.y < box.y + box.height && box.y < r.y + r.height
        }).map(k => k.id)
    }

    Rectangle {
        objectName: "selectionBox"
        visible: mouse.boxing
        x: mouse.box.x
        y: mouse.box.y
        width: mouse.box.width
        height: mouse.box.height
        color: Qt.alpha(theme.accent, 0.15)
        border.width: 1
        border.color: theme.accent
    }

    // One area over the whole drawing, so a drag can start on a key or between keys.
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true  // the page scrolls; a drag here draws the box instead
        cursorShape: view.hoveredKey >= 0 && !boxing ? Qt.PointingHandCursor : Qt.ArrowCursor

        property point start
        property bool boxing: false
        property rect box
        readonly property real threshold: 5

        function boxTo(px, py) {
            const x = Math.max(0, Math.min(px, view.width))
            const y = Math.max(0, Math.min(py, view.height))
            box = Qt.rect(Math.min(start.x, x), Math.min(start.y, y), Math.abs(x - start.x), Math.abs(y - start.y))
            view.boxKeys = view.keysIn(box)
        }

        onPressed: (m) => { start = Qt.point(m.x, m.y); boxing = false }
        onPositionChanged: (m) => {
            view.hoveredKey = view.keyAt(m.x, m.y)
            if (!pressed)
                return
            if (!boxing && Math.hypot(m.x - start.x, m.y - start.y) >= threshold)
                boxing = true
            if (boxing) {
                view.boxAdds = (m.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) !== 0
                boxTo(m.x, m.y)
            }
        }
        onReleased: (m) => {
            if (boxing) {
                boxTo(m.x, m.y)
                boxing = false
                view.boxSelected(view.boxKeys, m.modifiers)
                view.boxKeys = []
            } else {
                const id = view.keyAt(m.x, m.y)
                if (id >= 0)
                    view.keyClicked(id, m.modifiers)
            }
        }
        onCanceled: { boxing = false; view.boxKeys = [] }
        onExited: view.hoveredKey = -1
    }
}
