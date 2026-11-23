// SPDX-License-Identifier: GPL-3.0-or-later
// Key calibration, in three numbered steps: Start, press keys all the way down, Finish. While the session runs, each key
// shows how far down it has been, as a share of the travel the keyboard has stored for it.
// Opened from Settings, pushed on its StackView. Leaving it (Back, or another page in the sidebar)
// finishes the session, as the vendor app does (it has no cancel either).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    KeyNames { id: names }

    onVisibleChanged: if (!visible) backend.finishCalibration()
    Component.onDestruction: backend.finishCalibration()

    function depth(id) {
        const d = backend.calibrationDepth[String(id)]
        return d === undefined ? -1 : d
    }

    PageLayout {
        visible: backend.ready
        enabled: backend.connected && !backend.restarting

        RowLayout {
            Layout.leftMargin: -12 * ui.unit  // the back arrow lines up with the margin
            spacing: 8 * ui.unit
            AppButton {
                id: back
                objectName: "calibrationBack"
                kind: "quiet"
                text: qsTr("Back")
                implicitWidth: backContent.implicitWidth + 24 * ui.unit
                contentItem: Item {
                    implicitWidth: backContent.implicitWidth
                    Row {
                        id: backContent
                        anchors.centerIn: parent
                        spacing: 6 * ui.unit
                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: "arrow-left"
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: back.text
                        }
                    }
                }
                onClicked: { backend.finishCalibration(); page.StackView.view.pop() }
            }
            Label {
                text: qsTr("Calibration")
                font.pixelSize: ui.title
                font.weight: Font.DemiBold
            }
        }

        Card {
            title: qsTr("Key travel")

            // The sequence, as numbered steps; lines kept under about 70 characters.
            TextMetrics { id: line70; font: ui.font; text: "the quick brown fox jumps over a lazy dog ".repeat(2).slice(0, 70) }
            ColumnLayout {
                objectName: "calibrationSteps"
                Layout.maximumWidth: line70.advanceWidth + 30 * ui.unit  // and the step number
                spacing: 6 * ui.unit

                Repeater {
                    model: [
                        qsTr("Start calibration. The keys don't type until you finish."),
                        qsTr("Press each key you want to calibrate all the way down."),
                        qsTr("Finish. Each pressed key saves the lowest point it reached, so a key pressed only partway gets a shallower bottom. Keys you don't touch keep their calibration.")
                    ]
                    delegate: RowLayout {
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true
                        spacing: 10 * ui.unit
                        Label {
                            Layout.alignment: Qt.AlignTop
                            text: index + 1
                            font.weight: Font.DemiBold
                            font.features: { "tnum": 1 }
                            color: theme.secondary
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: modelData
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16 * ui.unit
                AppButton {
                    objectName: "calibrationButton"
                    kind: "primary"
                    text: backend.calibrating ? qsTr("Finish") : qsTr("Start calibration")
                    onClicked: backend.calibrating ? backend.finishCalibration() : backend.startCalibration()
                }
                Label {
                    objectName: "calibrationStatus"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: theme.secondary
                    font.features: { "tnum": 1 }
                    text: backend.calibrating
                          ? qsTr("%1 of %2 keys pressed all the way down").arg(backend.calibrationFull).arg(backend.layout.length)
                          : backend.calibrationSaved === 1 ? qsTr("Saved: 1 key got a new bottom.")
                          : backend.calibrationSaved >= 0 ? qsTr("Saved: %1 keys got a new bottom.").arg(backend.calibrationSaved)
                          : ""
                }
            }

            KeyboardView {
                objectName: "calibrationKeyboard"
                Layout.fillWidth: true
                Layout.maximumWidth: implicitWidth
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8 * ui.unit
                labelFor: function (id) {
                    const d = page.depth(id)
                    if (backend.calibrating && d >= 0)
                        return d + "%"
                    return backend.keymapReady ? names.capName(backend.keymap[0][String(id)]) : ""
                }
                colorFor: function (id) {
                    const d = page.depth(id)
                    if (!backend.calibrating || d < 0)
                        return ""
                    return d >= backend.calibrationFullPercent ? theme.success : theme.warning
                }
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: theme.secondary
                font.pixelSize: ui.small
                visible: backend.calibrating
                text: qsTr("Green: pressed all the way down (%1% or more of its stored travel). Amber: not yet.").arg(backend.calibrationFullPercent)
            }
        }
        Item { Layout.fillHeight: true }
    }
}
