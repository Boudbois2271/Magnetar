// SPDX-License-Identifier: GPL-3.0-or-later
// A text field at the app's one control height (34 u), the height of buttons and chips. The
// placeholder stays inside the field until something is typed: Material's floats up into the
// outline on focus, where the focus ring draws over it. So Material's is hidden and its outline,
// which leaves a gap for it, is drawn here without one.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

TextField {
    id: field
    implicitHeight: 34 * ui.unit
    topPadding: 0
    bottomPadding: 0
    placeholderTextColor: "transparent"

    background: Rectangle {
        implicitWidth: 120 * ui.unit
        radius: theme.radius
        color: "transparent"
        border.width: 1
        border.color: field.enabled && field.hovered ? field.Material.primaryTextColor : field.Material.hintTextColor
    }

    Label {
        objectName: "placeholder"
        x: field.leftPadding
        width: field.width - field.leftPadding - field.rightPadding
        height: field.height
        visible: field.length === 0 && field.preeditText.length === 0
        text: field.placeholderText
        font: field.font
        color: field.Material.hintTextColor
        horizontalAlignment: field.effectiveHorizontalAlignment
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    FocusRing {}
}
