// SPDX-License-Identifier: GPL-3.0-or-later
// A number with − and + at the app's one control height (34 u), the height of buttons and chips.
import QtQuick
import QtQuick.Controls

SpinBox {
    implicitHeight: 34 * ui.unit
    topPadding: 0
    bottomPadding: 0
}
