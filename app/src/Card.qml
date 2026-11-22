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
    implicitHeight: inner.implicitHeight + heading.implicitHeight + 52 * ui.unit
    radius: theme.radius
    color: theme.card

    Label {
        id: heading
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 20 * ui.unit
        text: card.title
        font.pixelSize: Math.round(13 * ui.unit)
        font.bold: true
        opacity: 0.7
    }

    ColumnLayout {
        id: inner
        anchors.top: heading.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20 * ui.unit
        anchors.topMargin: 12 * ui.unit
        spacing: 12 * ui.unit
    }
}
