// SPDX-License-Identifier: GPL-3.0-or-later
// The keyboard drawn from backend.layout (quarter key widths), scaled to the item's width.
// The page decides what each key says and which keys are selected. A click reports the
// keyboard modifiers held (Ctrl/Shift add to the selection).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Item {
    id: view

    property var keys: backend.layout
    property var labelFor: function (id) { return "" }  // text on a key
    property var markedFor: function (id) { return false }  // small dot: differs from factory
    property var selectedKeys: []
    signal keyClicked(int id, int modifiers)

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
            readonly property bool selected: view.selectedKeys.indexOf(keyId) >= 0

            objectName: "key_" + keyId
            x: modelData.x * view.unit + view.gap / 2
            y: modelData.y * view.unit + view.gap / 2
            width: modelData.w * view.unit - view.gap
            height: 4 * view.unit - view.gap
            radius: Math.min(theme.radius, 6)
            color: selected ? theme.accent : (mouse.containsMouse ? "#2a2a31" : "#202026")
            border.width: 1
            border.color: selected ? theme.accent : "#2e2e36"

            Label {
                anchors.fill: parent
                anchors.margins: view.unit < 10 ? 1 : 3
                text: view.labelFor(cap.keyId)
                color: cap.selected ? "#0e0e10" : Material.foreground
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

            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: (m) => view.keyClicked(cap.keyId, m.modifiers)
            }
        }
    }
}
