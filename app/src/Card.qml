// SPDX-License-Identifier: GPL-3.0-or-later
// A titled panel that groups related settings on a page. Given less height than it needs, its
// content shrinks: a box that scrolls inside it (Layout.fillHeight) gives way.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card
    property string title
    default property alias content: inner.data

    Layout.fillWidth: true
    implicitHeight: inner.implicitHeight + heading.implicitHeight + 52 * ui.unit
    radius: theme.radius
    color: theme.card

    Label {
        id: heading
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 20 * ui.unit
        text: card.title
        font.pixelSize: ui.section
        font.weight: Font.DemiBold
    }

    ColumnLayout {
        id: inner
        anchors.top: heading.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20 * ui.unit
        anchors.topMargin: 12 * ui.unit
        spacing: 12 * ui.unit
    }
}
