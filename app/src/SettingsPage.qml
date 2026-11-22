// SPDX-License-Identifier: GPL-3.0-or-later
// Base Functions: Win Key Lock, Combo Optimization, Adaptive Calibration, polling rate.
// Controls show what the keyboard reported; after a change they go back to that read-back.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    // One setting: name and a plain description on the left, a switch on the right.
    component SettingRow: RowLayout {
        id: row
        property string name
        property string description
        property bool value
        signal toggled(bool on)

        Layout.fillWidth: true
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label { text: row.name }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.55
                font.pixelSize: 12
                text: row.description
            }
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

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            // Nothing to show until the keyboard has been read once: no made-up values.
            visible: backend.ready
            enabled: backend.connected && !backend.restarting
            spacing: 20

            Item { implicitHeight: 12 }

            Label {
                Layout.leftMargin: 32
                text: qsTr("Settings")
                font.pixelSize: 26
                font.bold: true
            }

            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Keyboard")

                SettingRow {
                    objectName: "winKeyLock"
                    name: qsTr("Win Key Lock")
                    description: qsTr("The Windows key does nothing, so it can't open a menu by accident.")
                    value: backend.winKeyLock
                    onToggled: (on) => backend.setWinKeyLock(on)
                }
                SettingRow {
                    objectName: "comboOptimization"
                    name: qsTr("Combo Optimization")
                    description: qsTr("The firmware's handling of key combinations. On out of the box.")
                    value: backend.comboOptimization
                    onToggled: (on) => backend.setComboOptimization(on)
                }
                SettingRow {
                    objectName: "adaptiveCalibration"
                    name: qsTr("Adaptive Calibration")
                    description: qsTr("Adjusts each key's calibration while you type. On out of the box.")
                    value: backend.adaptiveCalibration
                    onToggled: (on) => backend.setAdaptiveCalibration(on)
                }
            }

            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Connection")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label { text: qsTr("Polling rate") }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            opacity: 0.55
                            font.pixelSize: 12
                            text: backend.restarting
                                  ? qsTr("Restarting the keyboard…")
                                  : qsTr("How often the keyboard reports to the computer. The keyboard restarts when it changes, for about a second.")
                        }
                    }
                    ComboBox {
                        id: rateBox
                        objectName: "pollingRateBox"
                        Layout.preferredWidth: 150
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

            Item { implicitHeight: 20 }
        }
    }
}
