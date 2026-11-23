// SPDX-License-Identifier: GPL-3.0-or-later
// A drop-down list at the app's one control height (34 u), the height of buttons and chips. Its
// list sits on a theme surface: Material's elevated popup background doesn't paint here.
import QtQuick
import QtQuick.Controls

ComboBox {
    implicitHeight: 34 * ui.unit
    popup.background: Rectangle {
        radius: theme.radius
        color: theme.overlay
        border.color: theme.chipHover
    }
    FocusRing {}
}
