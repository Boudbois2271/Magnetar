// SPDX-License-Identifier: GPL-3.0-or-later
// A page's frame: the title, then the content, inside the page margins. Pages fit the window
// instead of scrolling; a list that grows scrolls inside its own box.
// The header row is as tall as a tab bar on every page, so titles sit at one place whether a
// page has tabs (Keys) or not; a page drawing its own header gives its row `headerHeight`.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    property string title  // empty: the page draws its own header
    readonly property real headerHeight: tabProbe.implicitHeight
    default property alias content: body.data

    anchors.fill: parent
    anchors.leftMargin: 32 * ui.unit
    anchors.rightMargin: 32 * ui.unit
    anchors.topMargin: 20 * ui.unit
    anchors.bottomMargin: 20 * ui.unit
    spacing: 16 * ui.unit

    TabButton {
        id: tabProbe  // measures the header row, never shown
        visible: false
        text: "Ag"
    }
    PageTitle {
        visible: parent.title !== ""
        text: parent.title
        Layout.preferredHeight: parent.headerHeight
        verticalAlignment: Text.AlignVCenter
    }
    ColumnLayout {
        id: body
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 16 * ui.unit
    }
}
