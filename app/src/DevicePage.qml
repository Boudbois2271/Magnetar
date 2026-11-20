// SPDX-License-Identifier: GPL-3.0-or-later
// What the app knows about the connected keyboard, and what the app is (and isn't).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 20

        Label {
            text: qsTr("Device")
            font.pixelSize: 26
            font.bold: true
        }

        Card {
            title: qsTr("Keyboard")

            GridLayout {
                columns: 2
                columnSpacing: 32
                rowSpacing: 10

                Label { text: qsTr("Model"); opacity: 0.6 }
                Label { text: "Akko TAC75 HE (38ee:0016)" }

                Label { text: qsTr("Firmware"); opacity: 0.6 }
                Label { objectName: "firmwareLabel"; text: backend.firmware || "—" }

                Label { text: qsTr("Onboard profiles"); opacity: 0.6 }
                Label { text: backend.ready ? backend.profileCount : "—" }

                Label { text: qsTr("Device node"); opacity: 0.6 }
                Label { text: backend.devicePath || "—"; font.family: "monospace" }

                Label { text: qsTr("Status"); opacity: 0.6 }
                Label { text: backend.connected ? qsTr("Connected") : qsTr("Not connected") }
            }
        }

        Card {
            title: qsTr("About")

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Magnetar is a free, open-source configurator. It is not made, endorsed or supported by Akko or the keyboard's firmware vendor. Writing settings changes what is stored on the keyboard.")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.6
                text: qsTr("License: GPL-3.0-or-later")
            }
        }

        Item { Layout.fillHeight: true }
    }
}
