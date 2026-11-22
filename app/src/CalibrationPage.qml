// SPDX-License-Identifier: GPL-3.0-or-later
// Key calibration: Start, press keys all the way down, Finish. While the session runs, each key
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

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            visible: backend.ready
            enabled: backend.connected && !backend.restarting
            spacing: 20 * ui.unit

            Item { implicitHeight: 12 * ui.unit }

            RowLayout {
                Layout.leftMargin: 20 * ui.unit
                Layout.rightMargin: 32 * ui.unit
                spacing: 8 * ui.unit
                ToolButton {
                    objectName: "calibrationBack"
                    text: "←  " + qsTr("Back")
                    font.pixelSize: Math.round(14 * ui.unit)
                    onClicked: { backend.finishCalibration(); page.StackView.view.pop() }
                }
                Label {
                    text: qsTr("Calibration")
                    font.pixelSize: Math.round(26 * ui.unit)
                    font.bold: true
                }
            }

            Card {
                Layout.leftMargin: 32 * ui.unit
                Layout.rightMargin: 32 * ui.unit
                title: qsTr("Key travel")

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                    text: qsTr("Teaches the keyboard where each key's travel ends. Start, press each key you want to calibrate all the way down, then Finish. Finish saves the lowest point each pressed key reached, so a key pressed only partway gets a shallower bottom. Keys you don't touch keep their calibration. The keys don't type until you finish.")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16 * ui.unit
                    Button {
                        objectName: "calibrationButton"
                        text: backend.calibrating ? qsTr("Finish") : qsTr("Start calibration")
                        highlighted: true
                        onClicked: backend.calibrating ? backend.finishCalibration() : backend.startCalibration()
                    }
                    Label {
                        objectName: "calibrationStatus"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.7
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
                    opacity: 0.55
                    font.pixelSize: Math.round(12 * ui.unit)
                    visible: backend.calibrating
                    text: qsTr("Green: pressed all the way down (%1% or more of its stored travel). Amber: not yet.").arg(backend.calibrationFullPercent)
                }
            }

            Item { implicitHeight: 12 * ui.unit }
        }
    }
}
