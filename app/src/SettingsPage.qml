// SPDX-License-Identifier: GPL-3.0-or-later
// Settings: base functions (Win Key Lock, Combo Optimization, Adaptive Calibration), the way into
// key calibration, polling rate, and what the app knows about the keyboard.
// Controls show what the keyboard reported; after a change they go back to that read-back.
// Calibration opens as its own screen pushed over Settings; leaving Settings closes it.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    // Coming back to Settings shows Settings, not a calibration screen left open.
    onVisibleChanged: if (!visible && stack.depth > 1) stack.pop(null, StackView.Immediate)

    // One setting: name on the left, a switch on the right.
    component SettingRow: RowLayout {
        id: row
        property string name
        property bool value
        signal toggled(bool on)

        Layout.fillWidth: true
        spacing: 16 * ui.unit

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2 * ui.unit
            Label { Layout.fillWidth: true; text: row.name }
        }
        Switch {
            id: toggle
            objectName: row.objectName + "Switch"
            checked: row.value
            onToggled: {
                row.toggled(checked)
                checked = Qt.binding(() => row.value)  // show the read-back, not the click
            }
        }
    }

    StackView {
        id: stack
        objectName: "settingsStack"
        anchors.fill: parent
        initialItem: settings
    }

    Component {
        id: calibrationScreen
        CalibrationPage { objectName: "calibrationPage" }
    }

    ScrollView {
        id: settings
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            // Nothing to show until the keyboard has been read once: no made-up values.
            visible: backend.ready
            enabled: backend.connected && !backend.restarting
            spacing: 20 * ui.unit

            Item { implicitHeight: 12 * ui.unit }

            Label {
                Layout.leftMargin: 32 * ui.unit
                text: qsTr("Settings")
                font.pixelSize: Math.round(26 * ui.unit)
                font.bold: true
            }

            Card {
                Layout.leftMargin: 32 * ui.unit
                Layout.rightMargin: 32 * ui.unit
                title: qsTr("Keyboard")

                SettingRow {
                    objectName: "winKeyLock"
                    name: qsTr("Win Key Lock")
                    value: backend.winKeyLock
                    onToggled: (on) => backend.setWinKeyLock(on)
                }
                SettingRow {
                    objectName: "comboOptimization"
                    name: qsTr("Combo Optimization")
                    value: backend.comboOptimization
                    onToggled: (on) => backend.setComboOptimization(on)
                }
                SettingRow {
                    objectName: "adaptiveCalibration"
                    name: qsTr("Adaptive Calibration")
                    value: backend.adaptiveCalibration
                    onToggled: (on) => backend.setAdaptiveCalibration(on)
                }
            }

            Card {
                Layout.leftMargin: 32 * ui.unit
                Layout.rightMargin: 32 * ui.unit
                title: qsTr("Calibration")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16 * ui.unit

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2 * ui.unit
                        Label { Layout.fillWidth: true; text: qsTr("Key travel") }
                    }
                    Button {
                        objectName: "openCalibrationButton"
                        text: qsTr("Calibrate")
                        onClicked: stack.push(calibrationScreen)
                    }
                }
            }

            Card {
                Layout.leftMargin: 32 * ui.unit
                Layout.rightMargin: 32 * ui.unit
                title: qsTr("Connection")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16 * ui.unit

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2 * ui.unit
                        Label { Layout.fillWidth: true; text: qsTr("Polling rate") }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            opacity: 0.55
                            font.pixelSize: Math.round(12 * ui.unit)
                            visible: backend.restarting
                            text: qsTr("Restarting the keyboard…")
                        }
                    }
                    ComboBox {
                        id: rateBox
                        objectName: "pollingRateBox"
                        Layout.preferredWidth: 150 * ui.unit
                        model: backend.pollingRates
                        currentIndex: backend.pollingRates.indexOf(backend.pollingRate)
                        displayText: qsTr("%1 Hz").arg(currentValue)
                        delegate: ItemDelegate {
                            required property int index
                            required property int modelData
                            width: ListView.view ? ListView.view.width : implicitWidth
                            text: qsTr("%1 Hz").arg(modelData)
                            highlighted: rateBox.highlightedIndex === index
                        }
                        onActivated: (index) => {
                            const hz = backend.pollingRates[index]
                            if (hz !== backend.pollingRate)
                                backend.setPollingRate(hz)
                            currentIndex = Qt.binding(() => backend.pollingRates.indexOf(backend.pollingRate))
                        }
                    }
                }
            }

            

            Card {
                Layout.leftMargin: 32 * ui.unit
                Layout.rightMargin: 32 * ui.unit
                title: qsTr("Device")

                GridLayout {
                    columns: 2
                    columnSpacing: 32 * ui.unit
                    rowSpacing: 10 * ui.unit

                    Label { text: qsTr("Model"); opacity: 0.6 }
                    Label { text: "Akko TAC75 HE (38ee:0016)" }

                    Label { text: qsTr("Firmware"); opacity: 0.6 }
                    Label { objectName: "firmwareLabel"; text: backend.firmware || "—" }

                    Label { text: qsTr("Onboard profiles"); opacity: 0.6 }
                    Label { text: backend.profileCount }

                    Label { text: qsTr("Device node"); opacity: 0.6 }
                    Label { text: backend.devicePath || "—"; font.family: "monospace" }

                    Label { text: qsTr("Status"); opacity: 0.6 }
                    Label { text: backend.connected ? qsTr("Connected") : qsTr("Not connected") }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 8 * ui.unit
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                    text: qsTr("Magnetar is a free, open-source configurator. It is not made, endorsed or supported by Akko or the keyboard's firmware vendor. Writing settings changes what is stored on the keyboard.")
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.55
                    font.pixelSize: Math.round(12 * ui.unit)
                    text: qsTr("License: GPL-3.0-or-later")
                }
            }

            Item { implicitHeight: 20 * ui.unit }
        }
    }
}
