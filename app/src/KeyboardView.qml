// SPDX-License-Identifier: GPL-3.0-or-later
// The keyboard drawn from backend.layout (quarter key widths), scaled to the item's width:
// a plate (the case) with flat caps on it, each a lighter top face on a darker skirt and its
// legend top-left, like real keycaps. The page decides what each key says and which keys are
// selected. A click reports the keyboard modifiers held (Ctrl/Shift add to the selection).
// Dragging draws a box and reports every key it touches, with the modifiers held at the end of
// the drag. With interactive off it is a picture: no hover, clicks or box.
// From the keyboard: Tab reaches the drawing, the arrows move a focus ring between keys, Space or
// Enter clicks the ringed key (with Ctrl/Shift held, as with the mouse).
// Data on the caps, all optional: a depth fill from the top (how far the key goes down before it
// acts), a short badge at the cap's foot, and a legend lit in a key's LED colour.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Item {
    id: view

    property var keys: backend.layout
    property var labelFor: function (id) { return "" }  // text on a key
    property var markedFor: function (id) { return false }  // small dot: differs from factory
    property var colorFor: function (id) { return "" }  // the cap's colour when not ""
    property var depthFor: function (id) { return -1 }  // 0..1 of the travel, filled from the top; < 0: none
    property var badgeFor: function (id) { return "" }  // short text at the cap's foot
    property var legendColorFor: function (id) { return "" }  // the legend lit in this colour when not ""
    property var selectedKeys: []
    property bool interactive: true
    property bool squareBottom: false  // the plate goes on below it (a bar attached under it)
    signal keyClicked(int id, int modifiers)
    signal boxSelected(var ids, int modifiers)

    // While a box is drawn, the keys shown as selected: the box, plus the selection when
    // Ctrl or Shift will add to it.
    property var boxKeys: []
    property bool boxAdds: false
    readonly property bool boxing: mouse.boxing
    readonly property var shownKeys: boxing ? (boxAdds ? selectedKeys.concat(boxKeys) : boxKeys) : selectedKeys
    property int hoveredKey: -1
    property int focusKey: -1  // the key the arrows have reached; -1: the first key

    activeFocusOnTab: interactive
    readonly property int ringedKey: activeFocus ? (focusKey >= 0 ? focusKey : (keys.length > 0 ? keys[0].id : -1)) : -1

    // The nearest key in a direction, by the caps' centres; the key itself when there is none.
    function keyToward(id, dx, dy) {
        const from = keys.find(k => k.id === id)
        if (!from)
            return id
        const fc = Qt.point(from.x + from.w / 2, from.y + 2)
        let best = id, bestScore = Infinity
        for (const k of keys) {
            const c = Qt.point(k.x + k.w / 2, k.y + 2)
            const along = (c.x - fc.x) * dx + (c.y - fc.y) * dy  // distance in the direction
            const across = Math.abs((c.x - fc.x) * dy) + Math.abs((c.y - fc.y) * dx)
            if (k.id === id || along <= 0.5)
                continue
            const score = along + 2 * across
            if (score < bestScore) { bestScore = score; best = k.id }
        }
        return best
    }
    Keys.onPressed: (event) => {
        const moves = { [Qt.Key_Left]: [-1, 0], [Qt.Key_Right]: [1, 0], [Qt.Key_Up]: [0, -1], [Qt.Key_Down]: [0, 1] }
        const move = moves[event.key]
        if (move) {
            view.focusKey = view.keyToward(view.ringedKey, move[0], move[1])
            event.accepted = true
        } else if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (view.ringedKey >= 0)
                view.keyClicked(view.ringedKey, event.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))
            event.accepted = true
        }
    }

    readonly property int columns: 66  // 16.5 keys
    readonly property int rows: 26     // 6 rows and the gaps
    readonly property real bezel: 1.5  // the plate around the keys, in the same quarter units
    readonly property real unit: width / (columns + 2 * bezel)
    readonly property real gap: Math.max(2, unit * 0.35)
    readonly property real inset: bezel * unit

    // Keys grow with the window up to this size (one key width, in pixels), then the drawing
    // stays put: on a wide window it would push everything else off screen.
    property real maxKeySize: 56 * ui.unit
    implicitWidth: (columns + 2 * bezel) * maxKeySize / 4
    implicitHeight: (rows + 2 * bezel) * unit
    readonly property real dragMargin: mouse.margin  // a drag may start this far outside the plate
    readonly property real aspect: (columns + 2 * bezel) / (rows + 2 * bezel)  // width / height

    Rectangle {
        objectName: "plate"
        anchors.fill: parent
        topLeftRadius: theme.radius
        topRightRadius: theme.radius
        bottomLeftRadius: view.squareBottom ? 0 : theme.radius
        bottomRightRadius: view.squareBottom ? 0 : theme.radius
        color: theme.plate
    }

    Repeater {
        model: view.keys
        delegate: Item {
            id: cap
            required property var modelData
            readonly property int keyId: modelData.id
            readonly property bool selected: view.shownKeys.indexOf(keyId) >= 0
            readonly property bool hovered: view.hoveredKey === keyId
            readonly property string fill: view.colorFor(keyId)
            readonly property string lit: view.legendColorFor(keyId)
            readonly property real depth: view.depthFor(keyId)
            readonly property string badge: view.badgeFor(keyId)
            // A cap with its own colour or a lit legend keeps it when selected: a ring marks it.
            readonly property bool ringed: fill !== "" || lit !== ""
            // A lit legend on a light theme sits on a dark cap, as LEDs show through dark caps:
            // white or yellow would vanish on the light one.
            readonly property bool darkCap: lit !== "" && !theme.dark
            // Light colours get dark text, dark ones light text.
            readonly property bool lightFill: fill !== "" && (0.299 * Qt.color(fill).r + 0.587 * Qt.color(fill).g + 0.114 * Qt.color(fill).b) > 0.55
            // The top face: the key's colour when it has one (kept when selected: a ring marks
            // it instead), else the accent when selected, else the theme's cap.
            readonly property color color: fill !== "" ? fill
                : darkCap ? Qt.tint(theme.foreground, Qt.alpha(theme.background, hovered ? 0.24 : 0.16))
                : selected && !ringed ? theme.accent
                : hovered ? Qt.tint(theme.capTop, Qt.alpha(theme.foreground, 0.08)) : theme.capTop
            readonly property color skirt: fill !== "" ? Qt.darker(fill, 1.4)
                : darkCap ? theme.foreground
                : selected && !ringed ? Qt.darker(theme.accent, 1.35) : theme.capSkirt
            // The skirt shows a little at the sides and top, more at the front, like a cap seen
            // from above the front edge.
            readonly property real side: Math.max(1, height * 0.08)

            objectName: "key_" + keyId
            x: view.keyRect(modelData).x
            y: view.keyRect(modelData).y
            width: modelData.w * view.unit - view.gap
            height: 4 * view.unit - view.gap

            // First child: tests and pages read the legend as the cap's first item.
            Label {
                z: 1
                anchors.fill: face
                anchors.margins: Math.max(1, cap.height * 0.06)
                text: view.labelFor(cap.keyId)
                color: cap.fill !== "" ? (cap.lightFill ? "#0e0e10" : "#f0f0f0")
                     : cap.lit !== "" ? (Qt.color(cap.lit).hslLightness > 0 ? cap.lit : theme.secondary)  // black: the key is dark
                     : cap.selected ? theme.accentForeground : Material.foreground
                font.weight: cap.lit !== "" && Qt.color(cap.lit).hslLightness > 0 ? Font.DemiBold : Font.Normal
                // Sized from the key (about a quarter of the cap's height, like printed legends),
                // so it grows with the drawing; long names shrink on their own, then are cut.
                font.pixelSize: Math.max(Math.round(9 * ui.unit), Math.round(view.unit * 1.1))
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: Math.round(7 * ui.unit)
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignTop
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Rectangle {  // skirt
                anchors.fill: parent
                radius: Math.min(theme.radius, cap.height * 0.2)
                color: cap.skirt
                // A coloured cap hovered gets an outline, so its colour stays true.
                border.width: cap.ringed && cap.hovered && !cap.selected ? 1 : 0
                border.color: theme.foreground
            }

            Rectangle {
                id: face
                anchors.fill: parent
                anchors.leftMargin: cap.side
                anchors.rightMargin: cap.side
                anchors.topMargin: cap.side * 0.5
                anchors.bottomMargin: cap.side * 1.8
                radius: Math.max(0, Math.min(theme.radius, cap.height * 0.2) - cap.side)
                color: cap.color

                // How deep the key acts: a gauge at the right edge, filled from the top like the
                // key going down, ending on a tick at the actuation point.
                Item {
                    objectName: "depth"
                    visible: cap.depth >= 0
                    readonly property color ink: cap.selected && !cap.ringed ? theme.accentForeground : theme.accent
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: Math.max(2, cap.height * 0.08)
                    width: Math.max(3, view.unit * 0.35)
                    Rectangle {  // the whole travel
                        anchors.fill: parent
                        color: Qt.alpha(parent.ink, 0.2)
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: parent.height * Math.min(1, Math.max(0, cap.depth))
                        color: parent.ink
                    }
                }
            }

            Label {
                objectName: "badge"
                visible: cap.badge !== ""
                z: 1
                anchors.left: face.left
                anchors.right: face.right
                anchors.bottom: face.bottom
                anchors.margins: Math.max(1, cap.height * 0.06)
                text: cap.badge
                color: cap.selected && !cap.ringed ? theme.accentForeground : theme.accent
                font.pixelSize: Math.max(Math.round(9 * ui.unit), Math.round(view.unit * 0.9))
                font.weight: Font.DemiBold
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: Math.round(6 * ui.unit)
                elide: Text.ElideRight
            }

            Rectangle {  // a coloured cap keeps its colour when selected: a ring marks it instead
                visible: cap.ringed && cap.selected
                z: 2
                anchors.fill: parent
                radius: Math.min(theme.radius, cap.height * 0.2)
                color: "transparent"
                border.width: Math.max(2, 2 * ui.unit)
                border.color: theme.accent

                Rectangle {  // keeps the ring visible on a key the accent's own colour
                    anchors.fill: parent
                    anchors.margins: parent.border.width
                    radius: Math.max(0, parent.radius - parent.border.width)
                    color: "transparent"
                    border.width: 1
                    border.color: theme.accentForeground
                }
            }

            Rectangle {  // the keyboard focus
                objectName: "focusRing"
                visible: view.ringedKey === cap.keyId
                z: 3
                anchors.fill: parent
                anchors.margins: -Math.max(2, Math.round(2 * ui.unit)) - 1
                radius: Math.min(theme.radius, cap.height * 0.2)
                color: "transparent"
                border.width: Math.max(2, Math.round(2 * ui.unit))
                border.color: theme.accent
            }

            Rectangle {
                visible: view.markedFor(cap.keyId)
                anchors.top: face.top
                anchors.right: face.right
                anchors.margins: Math.max(2, cap.height * 0.08)
                // Left of the depth gauge when there is one.
                anchors.rightMargin: cap.depth >= 0 ? 2 * Math.max(2, cap.height * 0.08) + Math.max(3, view.unit * 0.35)
                                                    : Math.max(2, cap.height * 0.08)
                width: Math.max(4, cap.height * 0.12)
                height: width
                radius: width / 2
                color: cap.selected && !cap.ringed ? theme.accentForeground : theme.accent
            }
        }
    }

    // Key rectangles in the view's coordinates, the same as the drawn caps.
    function keyRect(k) {
        return Qt.rect(inset + k.x * unit + gap / 2, inset + k.y * unit + gap / 2, k.w * unit - gap, 4 * unit - gap)
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

    // One area over the whole drawing and a margin around it, so a drag can start on a key,
    // between keys or outside the keyboard. Handlers get view coordinates (m.x - margin).
    MouseArea {
        id: mouse
        readonly property real margin: 16  // stays inside the page's 20 px spacing
        anchors.fill: parent
        anchors.margins: -margin
        enabled: view.interactive
        hoverEnabled: true
        preventStealing: true  // the page scrolls; a drag here draws the box instead
        cursorShape: view.hoveredKey >= 0 && !boxing ? Qt.PointingHandCursor : Qt.ArrowCursor

        property point start
        property bool boxing: false
        property rect box
        readonly property real threshold: 5

        function boxTo(px, py) {
            const x = Math.max(-margin, Math.min(px, view.width + margin))
            const y = Math.max(-margin, Math.min(py, view.height + margin))
            box = Qt.rect(Math.min(start.x, x), Math.min(start.y, y), Math.abs(x - start.x), Math.abs(y - start.y))
            view.boxKeys = view.keysIn(box)
        }

        onPressed: (m) => { start = Qt.point(m.x - margin, m.y - margin); boxing = false }
        onPositionChanged: (m) => {
            view.hoveredKey = view.keyAt(m.x - margin, m.y - margin)
            if (!pressed)
                return
            if (!boxing && Math.hypot(m.x - margin - start.x, m.y - margin - start.y) >= threshold)
                boxing = true
            if (boxing) {
                view.boxAdds = (m.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) !== 0
                boxTo(m.x - margin, m.y - margin)
            }
        }
        onReleased: (m) => {
            if (boxing) {
                boxTo(m.x - margin, m.y - margin)
                boxing = false
                view.boxSelected(view.boxKeys, m.modifiers)
                view.boxKeys = []
            } else {
                const id = view.keyAt(m.x - margin, m.y - margin)
                if (id >= 0)
                    view.keyClicked(id, m.modifiers)
            }
        }
        onCanceled: { boxing = false; view.boxKeys = [] }
        onExited: view.hoveredKey = -1
        onEnabledChanged: if (!enabled) { boxing = false; view.boxKeys = []; view.hoveredKey = -1 }
    }
}
