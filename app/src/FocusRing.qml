// SPDX-License-Identifier: GPL-3.0-or-later
// The keyboard focus mark: an accent outline over its parent, in the theme's corners. Shown when
// the parent has focus from the keyboard (visualFocus), or any focus for items without it.
import QtQuick

Rectangle {
    property Item target: parent
    property bool shown: target && (target.visualFocus !== undefined ? target.visualFocus : target.activeFocus)

    objectName: "focusRing"
    anchors.fill: target
    z: 10
    visible: shown
    radius: theme.radius
    color: "transparent"
    border.width: Math.max(2, Math.round(2 * ui.unit))
    border.color: theme.accent
}
