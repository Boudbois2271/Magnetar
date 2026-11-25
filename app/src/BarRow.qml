// SPDX-License-Identifier: GPL-3.0-or-later
// A row of controls in two groups: `leading` at the start and `trailing` at the end, on one line
// when both fit; in a narrow window the trailing group goes on a line of its own under it.
import QtQuick
import QtQuick.Layouts

GridLayout {
    id: bar
    default property alias leading: lead.data
    property alias trailing: trail.data
    readonly property bool stacked: lead.implicitWidth + bar.columnSpacing + trail.implicitWidth > bar.width + 0.5

    columns: bar.stacked ? 1 : 2
    columnSpacing: 6 * ui.unit
    rowSpacing: 6 * ui.unit

    RowLayout {
        id: lead
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 6 * ui.unit
    }
    RowLayout {
        id: trail
        Layout.alignment: bar.stacked ? Qt.AlignLeft : Qt.AlignRight
        spacing: 6 * ui.unit
    }
}
