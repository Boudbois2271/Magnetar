// SPDX-License-Identifier: GPL-3.0-or-later
// The app's one button: primary (accent fill, the page's main action), secondary (chip fill)
// or quiet (text only, e.g. Back). Same height and corners as every other control.
import QtQuick
import QtQuick.Controls

AbstractButton {
    id: button
    property string kind: "secondary"  // "primary", "secondary" or "quiet"
    readonly property bool primary: button.kind === "primary"

    implicitWidth: Math.max(implicitHeight, label.implicitWidth + 24 * ui.unit)
    implicitHeight: 34 * ui.unit
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4

    background: Rectangle {
        radius: theme.radius
        color: button.primary
               ? (button.down ? (theme.dark ? Qt.lighter(theme.accent, 1.2) : Qt.darker(theme.accent, 1.15))
                  : button.hovered ? (theme.dark ? Qt.lighter(theme.accent, 1.1) : Qt.darker(theme.accent, 1.07))
                  : theme.accent)
               : button.kind === "quiet"
               ? (button.down ? theme.chipHover : button.hovered ? theme.chip : "transparent")
               : (button.down || button.hovered ? theme.chipHover : theme.chip)
        border.width: button.visualFocus ? 2 : 0
        border.color: button.primary ? theme.foreground : theme.accent
    }
    contentItem: Label {
        id: label
        text: button.text
        color: button.primary ? theme.accentForeground : theme.foreground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
