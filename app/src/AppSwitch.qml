// SPDX-License-Identifier: GPL-3.0-or-later
// An on/off switch in the theme's colours: accent track when on, a chip-coloured track when off.
// Material's own off track is a fixed lilac that ignores the Omarchy theme.
import QtQuick
import QtQuick.Controls

Switch {
    id: control
    opacity: enabled ? 1 : 0.4

    indicator: Rectangle {
        implicitWidth: 40 * ui.unit
        implicitHeight: 22 * ui.unit
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding)
                        : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        color: control.checked ? theme.accent : (control.hovered ? theme.chipHover : theme.chip)
        border.width: control.checked ? 0 : Math.max(1, Math.round(1.5 * ui.unit))
        border.color: theme.secondary

        Rectangle {
            readonly property real inset: 4 * ui.unit
            width: parent.height - 2 * inset
            height: width
            radius: width / 2
            x: control.visualPosition * (parent.width - width - 2 * inset) + inset
            y: inset
            color: control.checked ? theme.accentForeground : theme.secondary
        }
    }

    FocusRing {}
    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
