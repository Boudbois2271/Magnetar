// SPDX-License-Identifier: GPL-3.0-or-later
// One choice out of a few, as a row of joined chips. It shows `currentIndex` and only says what
// was clicked (activated): the page sets currentIndex from what the keyboard read back.
import QtQuick
import QtQuick.Controls

Row {
    id: segmented
    property var model: []  // the labels
    property int currentIndex: -1
    signal activated(int index)

    spacing: 1 * ui.unit

    Repeater {
        model: segmented.model
        delegate: AbstractButton {
            id: segment
            required property int index
            required property string modelData
            readonly property bool on: segmented.currentIndex === index
            readonly property bool first: index === 0
            readonly property bool last: index === segmented.model.length - 1

            objectName: segmented.objectName + "_" + index
            text: modelData
            implicitWidth: label.implicitWidth + 24 * ui.unit
            implicitHeight: 34 * ui.unit
            hoverEnabled: true
            opacity: enabled ? 1 : 0.4

            // Only the outer corners are rounded.
            background: Rectangle {
                topLeftRadius: segment.first ? theme.radius : 0
                bottomLeftRadius: segment.first ? theme.radius : 0
                topRightRadius: segment.last ? theme.radius : 0
                bottomRightRadius: segment.last ? theme.radius : 0
                color: segment.on ? theme.accent : (segment.hovered || segment.down ? theme.chipHover : theme.chip)
                border.width: segment.visualFocus ? 2 : 0
                border.color: segment.on ? theme.foreground : theme.accent
            }
            contentItem: Label {
                id: label
                text: segment.text
                color: segment.on ? theme.accentForeground : theme.foreground
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: segmented.activated(index)

            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }
    }
}
