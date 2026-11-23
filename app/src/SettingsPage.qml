// SPDX-License-Identifier: GPL-3.0-or-later
// Settings: base functions (Win Key Lock, Combo Optimization, Adaptive Calibration), the way into
// key calibration, polling rate, and what the app knows about the keyboard,
// and About (the not-affiliated notice and the licence).
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
        AppSwitch {
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

    // Two columns, so the page fits the window without scrolling.
    Item {
        id: settings

        PageLayout {
            title: qsTr("Settings")
            // Nothing to show until the keyboard has been read once: no made-up values.
            visible: backend.ready
            enabled: backend.connected && !backend.restarting

            RowLayout {
                Layout.fillWidth: true
                spacing: 16 * ui.unit
                uniformCellSizes: true

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 16 * ui.unit

                    Card {
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
                        title: qsTr("Calibration")

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16 * ui.unit

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2 * ui.unit
                                Label { Layout.fillWidth: true; text: qsTr("Key travel") }
                            }
                            AppButton {
                                objectName: "openCalibrationButton"
                                kind: "secondary"
                                text: qsTr("Calibrate")
                                onClicked: stack.push(calibrationScreen)
                            }
                        }
                    }

                    Card {
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
                                    color: theme.secondary
                                    font.pixelSize: ui.small
                                    visible: backend.restarting
                                    text: qsTr("Restarting the keyboard…")
                                }
                            }
                            AppComboBox {
                                id: rateBox
                                objectName: "pollingRateBox"
                                Layout.preferredWidth: 150 * ui.unit
                                model: backend.pollingRates
                                currentIndex: backend.pollingRates.indexOf(backend.pollingRate)
                                displayText: qsTr("%1 Hz").arg(currentValue)
                                font.features: { "tnum": 1 }
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
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 16 * ui.unit

                    Card {
                        title: qsTr("Device")

                        GridLayout {
                            columns: 2
                            columnSpacing: 16 * ui.unit
                            rowSpacing: 10 * ui.unit

                            Label { text: qsTr("Model"); color: theme.secondary }
                            Label { text: "Akko TAC75 HE (38ee:0016)" }

                            Label { text: qsTr("Firmware"); color: theme.secondary }
                            Label { objectName: "firmwareLabel"; text: backend.firmware || "—"; font.features: { "tnum": 1 } }

                            Label { text: qsTr("Onboard profiles"); color: theme.secondary }
                            Label { text: backend.profileCount; font.features: { "tnum": 1 } }

                            Label { text: qsTr("Device node"); color: theme.secondary }
                            Label { text: backend.devicePath || "—"; font.family: "monospace" }

                            Label { text: qsTr("Status"); color: theme.secondary }
                            Label { text: backend.connected ? qsTr("Connected") : qsTr("Not connected") }
                        }
                    }

                    // The app's one statement that it is unofficial (the README has it too).
                    Card {
                        objectName: "aboutSection"
                        title: qsTr("About")

                        Label {
                            objectName: "disclaimer"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: theme.secondary
                            text: qsTr("Magnetar is a free, open-source configurator. It is not made, endorsed or supported by Akko or the keyboard's firmware vendor. Writing settings changes what is stored on the keyboard.")
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: theme.secondary
                            font.pixelSize: ui.small
                            text: qsTr("License: GPL-3.0-or-later")
                        }
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }
    }
}
