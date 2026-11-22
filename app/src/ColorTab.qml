// SPDX-License-Identifier: GPL-3.0-or-later
// Keys page, Colour tab: the colour of each key under the Custom lighting effect. The drawing
// shows the colours the keyboard read back; a swatch or a typed hex colour goes to every
// selected key at once.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: tab
    spacing: 20

    property var selection: []  // key IDs
    readonly property var colors: backend.colorsReady ? backend.keyColors : ({})
    readonly property bool customOn: backend.effect === backend.customEffect
    // Black is "off" for that key.
    readonly property var swatches: [
        "#ff0000", "#ff6a00", "#ffd000", "#00ff00", "#00ffc8", "#00a0ff",
        "#0000ff", "#8000ff", "#ff00c8", "#ffffff", "#000000"
    ]
    readonly property int labelWidth: 120

    KeyNames { id: names }

    readonly property string drawingTitle: qsTr("Colour of each key")
    readonly property var labelFor: function (id) {
        return backend.keymapReady ? names.capName(backend.keymap[0][String(id)]) : ""
    }
    readonly property var markedFor: function (id) { return false }
    readonly property var colorFor: function (id) {
        return tab.colors[String(id)] || ""
    }

    // The colour every selected key has, or "" when they differ.
    readonly property var picked: tab.selection.map(id => tab.colors[String(id)]).filter(c => c !== undefined)
    readonly property string shared: tab.picked.length > 0 && tab.picked.every(c => c === tab.picked[0]) ? tab.picked[0] : ""

    function apply(color) {
        if (tab.selection.length > 0)
            return backend.setKeyColors(tab.selection, color)
        return false
    }

    // ---- the colours only show under Custom ---------------------------------
    Card {
        objectName: "customHint"
        Layout.leftMargin: 32
        Layout.rightMargin: 32
        visible: !tab.customOn
        title: qsTr("Custom effect is off")

        RowLayout {
            spacing: 16
            Button {
                objectName: "switchToCustom"
                text: qsTr("Switch to Custom")
                highlighted: true
                onClicked: backend.setEffect(backend.customEffect)
            }
        }
    }

    Card {
        Layout.leftMargin: 32
        Layout.rightMargin: 32
        visible: tab.selection.length > 0
        title: qsTr("Colour")

        RowLayout {
            spacing: 16

            // What the selected keys have now; striped when they differ.
            Rectangle {
                objectName: "selectedColor"
                implicitWidth: 56
                implicitHeight: 56
                radius: Math.min(4, theme.radius)
                color: tab.shared !== "" ? tab.shared : "transparent"
                border.color: Qt.alpha(theme.foreground, 0.19)
                Label {
                    anchors.centerIn: parent
                    visible: tab.shared === ""
                    text: qsTr("Mixed")
                    font.pixelSize: 11
                    opacity: 0.7
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 10
                Repeater {
                    model: tab.swatches
                    delegate: Rectangle {
                        required property string modelData
                        readonly property bool current: tab.shared === modelData
                        objectName: "swatch_" + modelData.slice(1)
                        width: 30
                        height: 30
                        radius: 15
                        color: modelData
                        border.width: current ? 3 : 1
                        border.color: current ? theme.accent : Qt.alpha(theme.foreground, 0.31)
                        ToolTip.visible: modelData === "#000000" && swatchMouse.containsMouse
                        ToolTip.text: qsTr("Black: the key stays dark")
                        MouseArea {
                            id: swatchMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tab.apply(parent.modelData)
                        }
                    }
                }
            }
        }

        RowLayout {
            Label {
                Layout.preferredWidth: tab.labelWidth
                text: qsTr("Hex colour")
            }
            TextField {
                id: hex
                objectName: "keyHexField"
                Layout.preferredWidth: 140
                font.family: "monospace"
                validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{0,6}/ }
                onAccepted: if (!tab.apply(text)) hex.text = tab.shared
                Binding on text {
                    when: !hex.activeFocus
                    value: tab.shared
                    restoreMode: Binding.RestoreNone
                }
            }
        }
    }
}
