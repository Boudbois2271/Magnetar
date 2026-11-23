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

    // Every row in the page is this tall, so rows sit at an even pitch whatever their control.
    readonly property real rowHeight: 34 * ui.unit

    // One setting: name on the left, its control right after it (the column is narrow).
    component OptionRow: RowLayout {
        id: option
        property string name
        Layout.fillWidth: true
        Layout.preferredHeight: page.rowHeight
        spacing: 16 * ui.unit
        Label { Layout.fillWidth: true; text: option.name; elide: Text.ElideRight }
    }
    // A switch setting: shows the read-back, not the click.
    component SettingRow: OptionRow {
        id: row
        property bool value
        signal toggled(bool on)
        AppSwitch {
            objectName: row.objectName + "Switch"
            checked: row.value
            onToggled: {
                row.toggled(checked)
                checked = Qt.binding(() => row.value)
            }
        }
    }
    // A fact about the keyboard: name in the secondary colour, then the value.
    component InfoRow: RowLayout {
        id: info
        property string name
        property string value
        property string valueName  // the value label's objectName
        property bool mono: false
        Layout.fillWidth: true
        Layout.preferredHeight: page.rowHeight
        spacing: 16 * ui.unit
        Label { Layout.preferredWidth: 170 * ui.unit; text: info.name; color: theme.secondary; elide: Text.ElideRight }
        Label {
            objectName: info.valueName
            Layout.fillWidth: true
            text: info.value
            elide: Text.ElideRight
            font.features: { "tnum": 1 }
            Component.onCompleted: if (info.mono) font.family = "monospace"
        }
    }
    // A card's rows, packed: the row height is their spacing.
    component Rows: ColumnLayout {
        Layout.fillWidth: true
        spacing: 0
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

    // One column, options first, then Device and About. Narrow, so each control sits near its
    // label.
    Item {
        id: settings

        PageLayout {
            title: qsTr("Settings")
            // Nothing to show until the keyboard has been read once: no made-up values.
            visible: backend.ready
            enabled: backend.connected && !backend.restarting

            ColumnLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 460 * ui.unit
                spacing: 8 * ui.unit

                Card {
                    title: qsTr("Keyboard")
                    Rows {
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
                }

                Card {
                    title: qsTr("Calibration")
                    Rows {
                        OptionRow {
                            name: qsTr("Key travel")
                            AppButton {
                                objectName: "openCalibrationButton"
                                kind: "secondary"
                                text: qsTr("Calibrate")
                                onClicked: stack.push(calibrationScreen)
                            }
                        }
                    }
                }

                Card {
                    title: qsTr("Connection")
                    Rows {
                        OptionRow {
                            name: qsTr("Polling rate")
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
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: theme.secondary
                            font.pixelSize: ui.small
                            visible: backend.restarting
                            text: qsTr("Restarting the keyboard…")
                        }
                    }
                }

                Card {
                    title: qsTr("Device")
                    Rows {
                        InfoRow { name: qsTr("Model"); value: "Akko TAC75 HE (38ee:0016)" }
                        InfoRow { name: qsTr("Firmware"); value: backend.firmware || "—"; valueName: "firmwareLabel" }
                        InfoRow { name: qsTr("Onboard profiles"); value: backend.profileCount }
                        InfoRow { name: qsTr("Device node"); value: backend.devicePath || "—"; mono: true }
                        InfoRow { name: qsTr("Status"); value: backend.connected ? qsTr("Connected") : qsTr("Not connected") }
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
            Item { Layout.fillHeight: true }
        }
    }
}
