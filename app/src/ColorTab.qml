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
    spacing: 16 * ui.unit

    property var selection: []  // key IDs
    readonly property var colors: backend.colorsReady ? backend.keyColors : ({})
    readonly property bool customOn: backend.effect === backend.customEffect
    // Black is "off" for that key.
    readonly property var swatches: [
        "#ff0000", "#ff6a00", "#ffd000", "#00ff00", "#00ffc8", "#00a0ff",
        "#0000ff", "#8000ff", "#ff00c8", "#ffffff", "#000000"
    ]
    readonly property real labelWidth: 120 * ui.unit
    // The page's controls sit in three equal columns.
    readonly property real column: (width - 48 * ui.unit) / 3
    function span(n) { return n * column + (n - 1) * 24 * ui.unit }

    KeyNames { id: names }

    readonly property var labelFor: function (id) {
        return backend.keymapReady ? names.capName(backend.keymap[0][String(id)]) : ""
    }
    readonly property var markedFor: function (id) { return false }
    // The legends lit in each key's LED colour, only under Custom: otherwise the board doesn't
    // show them, and the page says so in one line (on the selection bar).
    readonly property var legendColorFor: function (id) {
        return tab.customOn ? tab.colors[String(id)] || "" : ""
    }

    // The colour every selected key has, or "" when they differ.
    readonly property var picked: tab.selection.map(id => tab.colors[String(id)]).filter(c => c !== undefined)
    readonly property string shared: tab.picked.length > 0 && tab.picked.every(c => c === tab.picked[0]) ? tab.picked[0] : ""

    function apply(color) {
        if (tab.selection.length > 0)
            return backend.setKeyColors(tab.selection, color)
        return false
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 3
        columnSpacing: 24 * ui.unit

        Card {
            visible: tab.selection.length > 0
            Layout.columnSpan: 2
            Layout.preferredWidth: tab.span(2)
            Layout.maximumWidth: tab.span(2)
            Layout.alignment: Qt.AlignTop
            title: qsTr("Colour")

            RowLayout {
                spacing: 16 * ui.unit

                // What the selected keys have now; striped when they differ.
                Rectangle {
                    objectName: "selectedColor"
                    implicitWidth: 56 * ui.unit
                    implicitHeight: 56 * ui.unit
                    radius: Math.min(4, theme.radius)
                    color: tab.shared !== "" ? tab.shared : "transparent"
                    border.color: Qt.alpha(theme.foreground, 0.19)
                    Label {
                        anchors.centerIn: parent
                        visible: tab.shared === ""
                        text: qsTr("Mixed")
                        font.pixelSize: ui.small
                        color: theme.secondary
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 10 * ui.unit
                    Repeater {
                        model: tab.swatches
                        delegate: Rectangle {
                            required property string modelData
                            readonly property bool current: tab.shared === modelData
                            objectName: "swatch_" + modelData.slice(1)
                            width: 30 * ui.unit
                            height: 30 * ui.unit
                            radius: width / 2
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
                AppTextField {
                    id: hex
                    objectName: "keyHexField"
                    Layout.preferredWidth: 140 * ui.unit
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

    Item { Layout.fillHeight: true }
}
