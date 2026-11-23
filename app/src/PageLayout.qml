// SPDX-License-Identifier: GPL-3.0-or-later
// A page's frame: the title, then the content, inside the page margins. Pages fit the window
// instead of scrolling; a list that grows scrolls inside its own box.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    property string title  // empty: the page draws its own header
    default property alias content: body.data

    anchors.fill: parent
    anchors.leftMargin: 32 * ui.unit
    anchors.rightMargin: 32 * ui.unit
    anchors.topMargin: 20 * ui.unit
    anchors.bottomMargin: 20 * ui.unit
    spacing: 16 * ui.unit

    Label {
        visible: parent.title !== ""
        text: parent.title
        font.pixelSize: ui.title
        font.weight: Font.DemiBold
    }
    ColumnLayout {
        id: body
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 16 * ui.unit
    }
}
