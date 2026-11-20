// SPDX-License-Identifier: GPL-3.0-or-later
// Says, in the user's language, why the keyboard can't be reached, and offers a retry.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: banner
    objectName: "connectionBanner"
    visible: !backend.connected
    implicitHeight: visible ? row.implicitHeight + 28 : 0
    color: backend.errorKind === "" ? "#1d1d22" : "#3a1d1d"

    readonly property string message: {
        switch (backend.errorKind) {
        case "":           return qsTr("Connecting to the keyboard…")
        case "notFound":   return qsTr("Keyboard not found. Is it plugged in?")
        case "permission": return qsTr("No permission to use the keyboard. Install the udev rule from the README.")
        case "busy":       return qsTr("Another program is using the keyboard. Close the vendor web app, or wait a moment.")
        case "noResponse": return qsTr("The keyboard didn't answer.")
        default:           return qsTr("Something went wrong while talking to the keyboard.")
        }
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 16
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                Layout.fillWidth: true
                text: banner.message
                wrapMode: Text.WordWrap
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                visible: backend.errorKind !== ""
                // The raw message only adds something (a device node, a command) for these.
                readonly property bool detailed: ["permission", "noResponse", "other"].indexOf(backend.errorKind) >= 0
                text: qsTr("Trying again every few seconds.") + (detailed ? "  " + backend.errorDetail : "")
                wrapMode: Text.WordWrap
                opacity: 0.6
                font.pixelSize: 12
            }
        }
        Button {
            visible: backend.errorKind !== ""
            text: qsTr("Retry")
            flat: true
            onClicked: backend.refresh()
        }
    }
}
