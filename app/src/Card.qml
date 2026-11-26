// SPDX-License-Identifier: GPL-3.0-or-later
// A titled group on a page: a section title and the controls under it, set apart by space, not
// a box. `filled` puts it on a surface, for objects rather than groups (the macro list). Given
// less height than it needs, its content shrinks: a box that scrolls inside it
// (Layout.fillHeight) gives way.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card
    property string title
    property bool filled: false
    default property alias content: inner.data

    readonly property real padding: card.filled ? 20 * ui.unit : 0
    // Without a box, the space under a group is what separates it from the next.
    readonly property real space: card.filled ? 0 : 8 * ui.unit

    Layout.fillWidth: true
    implicitHeight: (card.title !== "" ? heading.implicitHeight + 10 * ui.unit : 0) + inner.implicitHeight + 2 * card.padding + card.space
    radius: theme.radius
    color: card.filled ? theme.card : "transparent"

    Label {
        id: heading
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: card.padding
        visible: card.title !== ""  // no title, no room for one
        text: card.title
        elide: Text.ElideRight
        font.pixelSize: ui.section
        font.weight: Font.DemiBold
    }

    ColumnLayout {
        id: inner
        anchors.top: card.title !== "" ? heading.bottom : parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: card.padding
        anchors.topMargin: card.title !== "" ? 10 * ui.unit : card.padding
        anchors.bottomMargin: card.padding + card.space
        spacing: 12 * ui.unit
    }
}
