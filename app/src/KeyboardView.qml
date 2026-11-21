// SPDX-License-Identifier: GPL-3.0-or-later
// The keyboard drawn from backend.layout (quarter key widths), scaled to the item's width.
// The page decides what each key says and which keys are selected (one, or a set).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Item {
    id: view

    property var keys: backend.layout
    property var labelFor: function (id) { return "" }  // text on a key
    property var markedFor: function (id) { return false }  // small dot: differs from factory
    property int selectedKey: -1
    property var selectedKeys: []  // for pages that select several keys at once
    signal keyClicked(int id)

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
            readonly property bool selected: view.selectedKey === keyId || view.selectedKeys.indexOf(keyId) >= 0

            objectName: "key_" + keyId
            x: modelData.x * view.unit + view.gap / 2
            y: modelData.y * view.unit + view.gap / 2
            width: modelData.w * view.unit - view.gap
            height: 4 * view.unit - view.gap
            radius: Math.min(theme.radius, 6)
            color: selected ? theme.accent : (hover.hovered ? "#2a2a31" : "#202026")
            border.width: 1
            border.color: selected ? theme.accent : "#2e2e36"

            Label {
                anchors.fill: parent
                anchors.margins: 3
                text: view.labelFor(cap.keyId)
                color: cap.selected ? "#0e0e10" : Material.foreground
                font.pixelSize: Math.max(8, Math.min(13, view.unit * 2.6))
                fontSizeMode: Text.HorizontalFit  // long names shrink before they are cut
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

            HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: view.keyClicked(cap.keyId) }
        }
    }
}
