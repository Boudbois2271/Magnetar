// SPDX-License-Identifier: GPL-3.0-or-later
// A titled panel that groups related settings on a page.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card
    property string title
    default property alias content: inner.data

    Layout.fillWidth: true
    implicitHeight: inner.implicitHeight + heading.implicitHeight + 52
    radius: theme.radius
    color: theme.card

    Label {
        id: heading
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 20
        text: card.title
        font.pixelSize: 13
        font.bold: true
        opacity: 0.7
    }

    ColumnLayout {
        id: inner
        anchors.top: heading.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        anchors.topMargin: 12
        spacing: 12
    }
}
