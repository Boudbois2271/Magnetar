// SPDX-License-Identifier: GPL-3.0-or-later
// Global lighting: on/off and brightness, effect and speed, colour or rainbow.
// Controls show what the keyboard reported; a control being dragged or edited keeps the
// user's value until they let go.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: page

    // Indexed by effect number (00 … 13), as decoded in akko-protocol.md.
    readonly property var effectNames: [
        qsTr("Off"), qsTr("Steady"), qsTr("Breathing"), qsTr("Dream rainbow"),
        qsTr("Instant trigger"), qsTr("Walking in the rain"), qsTr("Rainbow wheel"), qsTr("Ripple"),
        qsTr("Starry night"), qsTr("Snow trail"), qsTr("Endless flow"), qsTr("Flowing water"),
        qsTr("Shadow follow"), qsTr("Sine wave"), qsTr("Left-right scan"), qsTr("Spinning windmill"),
        qsTr("Colorful waterfall"), qsTr("Blossom prosperity"), qsTr("Spinning storm"),
        qsTr("Custom (per-key colours)")
    ]
    readonly property var swatches: [
        "#ff0000", "#ff6a00", "#ffd000", "#00ff00", "#00ffc8", "#00a0ff",
        "#0000ff", "#8000ff", "#ff00c8", "#ffffff"
    ]
    readonly property int labelWidth: 120

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            // Nothing to show until the keyboard has been read once: no made-up values.
            visible: backend.ready
            enabled: backend.connected
            spacing: 20

            Item { implicitHeight: 12 }

            Label {
                Layout.leftMargin: 32
                text: qsTr("Lighting")
                font.pixelSize: 26
                font.bold: true
            }

            // ---- on/off and brightness -----------------------------------------
            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Lights")

                RowLayout {
                    Label {
                        Layout.preferredWidth: page.labelWidth
                        text: qsTr("Lights on")
                    }
                    Item { Layout.fillWidth: true }
                    Switch {
                        objectName: "lightsSwitch"
                        checked: backend.lightsOn
                        onToggled: backend.setLightsOn(checked)
                    }
                }
                RowLayout {
                    Label {
                        Layout.preferredWidth: page.labelWidth
                        text: qsTr("Brightness")
                    }
                    Slider {
                        id: brightness
                        objectName: "brightnessSlider"
                        Layout.fillWidth: true
                        from: 0
                        to: backend.maxBrightness
                        stepSize: 1
                        snapMode: Slider.SnapAlways
                        onMoved: backend.setBrightness(Math.round(value))
                        Binding on value {
                            when: !brightness.pressed
                            value: backend.brightness
                            restoreMode: Binding.RestoreNone
                        }
                    }
                    Label {
                        Layout.preferredWidth: 64
                        horizontalAlignment: Text.AlignRight
                        text: Math.round(brightness.value) + " / " + backend.maxBrightness
                        font.features: { "tnum": 1 }
                    }
                }
            }

            // ---- effect and speed ----------------------------------------------
            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Effect")

                RowLayout {
                    Label {
                        Layout.preferredWidth: page.labelWidth
                        text: qsTr("Effect")
                    }
                    ComboBox {
                        objectName: "effectBox"
                        Layout.preferredWidth: 300
                        model: page.effectNames
                        currentIndex: backend.effect
                        onActivated: (index) => backend.setEffect(index)
                    }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {
                    Label {
                        Layout.preferredWidth: page.labelWidth
                        text: qsTr("Speed")
                    }
                    Slider {
                        id: speed
                        objectName: "speedSlider"
                        Layout.fillWidth: true
                        from: 0
                        to: 4
                        stepSize: 1
                        snapMode: Slider.SnapAlways
                        onMoved: backend.setSpeed(Math.round(value))
                        Binding on value {
                            when: !speed.pressed
                            value: backend.speed
                            restoreMode: Binding.RestoreNone
                        }
                    }
                    Label {
                        Layout.preferredWidth: 64
                        horizontalAlignment: Text.AlignRight
                        text: Math.round(speed.value) + " / 4"
                    }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.55
                    font.pixelSize: 12
                    text: qsTr("Each effect keeps its own colour and speed on the keyboard, so switching effect brings back that effect's settings.")
                }
            }

            // ---- colour ----------------------------------------------------------
            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Colour")

                RowLayout {
                    spacing: 16

                    // What the keyboard is showing now.
                    Rectangle {
                        implicitWidth: 56
                        implicitHeight: 56
                        radius: Math.min(4, theme.radius)
                        color: backend.color
                        visible: !backend.rainbow
                        border.color: "#30ffffff"
                    }
                    Rectangle {
                        implicitWidth: 56
                        implicitHeight: 56
                        radius: Math.min(4, theme.radius)
                        visible: backend.rainbow
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#ff0000" }
                            GradientStop { position: 0.33; color: "#00ff00" }
                            GradientStop { position: 0.66; color: "#0000ff" }
                            GradientStop { position: 1.0; color: "#ff00ff" }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 10
                        Repeater {
                            model: page.swatches
                            delegate: Rectangle {
                                required property string modelData
                                readonly property bool current: !backend.rainbow && backend.color === modelData
                                width: 30
                                height: 30
                                radius: 15
                                color: modelData
                                border.width: current ? 3 : 1
                                border.color: current ? theme.accent : "#30ffffff"
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: backend.setColor(parent.modelData)
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Label {
                        Layout.preferredWidth: page.labelWidth
                        text: qsTr("Hex colour")
                    }
                    TextField {
                        id: hex
                        objectName: "hexField"
                        Layout.preferredWidth: 140
                        font.family: "monospace"
                        validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{0,6}/ }
                        onAccepted: if (!backend.setColor(text)) hex.text = backend.color
                        Binding on text {
                            when: !hex.activeFocus
                            value: backend.color
                            restoreMode: Binding.RestoreNone
                        }
                    }
                    Label {
                        opacity: 0.55
                        font.pixelSize: 12
                        text: qsTr("Press Enter to apply")
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Label {
                        Layout.preferredWidth: page.labelWidth
                        text: qsTr("Rainbow")
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.55
                        font.pixelSize: 12
                        text: qsTr("Cycle through all colours instead of one fixed colour.")
                    }
                    Switch {
                        objectName: "rainbowSwitch"
                        checked: backend.rainbow
                        onToggled: backend.setRainbow(checked)
                    }
                }
            }

            Item { implicitHeight: 20 }
        }
    }
}
