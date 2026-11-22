// SPDX-License-Identifier: GPL-3.0-or-later
// Mural: paint keys and show the picture live on the keyboard. Click a key or drag a box to
// paint with the brush colour. Nothing is saved on the board: while the picture is shown the
// board's own lighting is paused, and it comes back by itself when Stop is pressed or the page
// is left.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    property string brush: "#ff0000"
    // Black is "off" for that key.
    readonly property var swatches: [
        "#ff0000", "#ff6a00", "#ffd000", "#00ff00", "#00ffc8", "#00a0ff",
        "#0000ff", "#8000ff", "#ff00c8", "#ffffff", "#000000"
    ]
    readonly property int labelWidth: 120

    KeyNames { id: names }

    onVisibleChanged: if (!visible) backend.stopMural()

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            visible: backend.ready
            enabled: backend.connected && !backend.restarting
            spacing: 20

            Item { implicitHeight: 12 }

            Label {
                Layout.leftMargin: 32
                text: qsTr("Mural")
                font.pixelSize: 26
                font.bold: true
            }

            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Live picture")

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                    text: qsTr("Paint keys below and show them live on the keyboard. Nothing is saved on the keyboard: its own lighting comes back a moment after you stop or leave this page.")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    Button {
                        objectName: "muralButton"
                        text: backend.muralRunning ? qsTr("Stop") : qsTr("Show on keyboard")
                        highlighted: true
                        onClicked: backend.muralRunning ? backend.stopMural() : backend.startMural()
                    }
                    Label {
                        objectName: "muralStatus"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.7
                        text: backend.muralRunning ? qsTr("Showing on the keyboard") : ""
                    }
                }
            }

            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Brush")

                RowLayout {
                    spacing: 16

                    Rectangle {
                        objectName: "muralBrush"
                        implicitWidth: 56
                        implicitHeight: 56
                        radius: Math.min(4, theme.radius)
                        color: page.brush
                        border.color: "#30ffffff"
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 0  // wrap to the card instead of widening it
                        spacing: 10
                        Repeater {
                            model: page.swatches
                            delegate: Rectangle {
                                required property string modelData
                                readonly property bool current: page.brush === modelData
                                objectName: "muralSwatch_" + modelData.slice(1)
                                width: 30
                                height: 30
                                radius: 15
                                color: modelData
                                border.width: current ? 3 : 1
                                border.color: current ? theme.accent : "#50ffffff"
                                ToolTip.visible: modelData === "#000000" && swatchMouse.containsMouse
                                ToolTip.text: qsTr("Black: the key stays dark")
                                MouseArea {
                                    id: swatchMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: page.brush = parent.modelData
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    spacing: 16
                    Label {
                        Layout.preferredWidth: page.labelWidth
                        text: qsTr("Hex colour")
                    }
                    TextField {
                        id: hex
                        objectName: "muralHexField"
                        Layout.preferredWidth: 140
                        font.family: "monospace"
                        validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{0,6}/ }
                        onAccepted: {
                            const c = (text.startsWith("#") ? text : "#" + text).toLowerCase()
                            if (/^#[0-9a-f]{6}$/.test(c))
                                page.brush = c
                            else
                                text = page.brush
                        }
                        Binding on text {
                            when: !hex.activeFocus
                            value: page.brush
                            restoreMode: Binding.RestoreNone
                        }
                    }
                }
            }

            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Picture")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.7
                        text: qsTr("Click a key or drag a box to paint with the brush.")
                    }
                    Button {
                        objectName: "muralFill"
                        text: qsTr("Fill all keys")
                        onClicked: backend.fillMural(page.brush)
                    }
                }

                KeyboardView {
                    objectName: "muralKeyboard"
                    Layout.fillWidth: true
                    Layout.maximumWidth: implicitWidth
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 8
                    labelFor: function (id) {
                        return backend.keymapReady ? names.capName(backend.keymap[0][String(id)]) : ""
                    }
                    colorFor: function (id) {
                        return backend.muralColors[String(id)] || ""
                    }
                    onKeyClicked: (id, modifiers) => backend.paintMural([id], page.brush)
                    onBoxSelected: (ids, modifiers) => backend.paintMural(ids, page.brush)
                }
            }

            Item { implicitHeight: 12 }
        }
    }
}
