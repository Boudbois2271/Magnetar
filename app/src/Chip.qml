// SPDX-License-Identifier: GPL-3.0-or-later
// A small button for picking among many: filled with the accent colour when `on`.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

AbstractButton {
    id: chip
    property bool on: false

    implicitWidth: label.implicitWidth + 24
    implicitHeight: 34
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4

    background: Rectangle {
        radius: Math.min(theme.radius, 8)
        color: chip.on ? theme.accent : (chip.hovered ? theme.chipHover : theme.chip)
        border.width: chip.visualFocus ? 2 : 0
        border.color: theme.accent
    }
    contentItem: Label {
        id: label
        text: chip.text
        color: chip.on ? theme.accentForeground : Material.foreground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
