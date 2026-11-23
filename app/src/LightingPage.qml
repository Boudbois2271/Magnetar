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
    readonly property real labelWidth: 120 * ui.unit

    KeyNames { id: names }

    // The colour a key's LED shows, as read back: nothing with the lights or the effect off;
    // the per-key colours under Custom; a hue across the board for rainbow; else the global
    // colour. Brightness mixes it into the unlit cap: at 0 the cap looks unlit.
    function previewColor(k) {
        if (!backend.lightsOn || backend.effect === 0)
            return ""
        let led
        if (backend.effect === backend.customEffect)
            led = backend.colorsReady ? backend.keyColors[String(k.id)] : undefined
        else if (backend.rainbow)
            led = Qt.hsva((k.x + k.w / 2) / 66, 1, 1, 1)
        else
            led = backend.color
        if (led === undefined)
            return ""
        const level = backend.maxBrightness > 0 ? backend.brightness / backend.maxBrightness : 1
        return Qt.tint(theme.capTop, Qt.alpha(led, level)).toString()
    }
    readonly property var keyById: {
        const m = {}
        for (const k of backend.layout)
            m[k.id] = k
        return m
    }

    // Two columns, so the page fits the window without scrolling.
    PageLayout {
        title: qsTr("Lighting")
        // Nothing to show until the keyboard has been read once: no made-up values.
        visible: backend.ready
        enabled: backend.connected

        // What the board shows now, a picture only.
        KeyboardView {
            objectName: "lightingPreview"
            Layout.fillWidth: true
            Layout.maximumWidth: implicitWidth
            maxKeySize: 44 * ui.unit
            interactive: false
            labelFor: function (id) {
                return backend.keymapReady ? names.capName(backend.keymap[0][String(id)]) : ""
            }
            colorFor: function (id) { return page.previewColor(page.keyById[id]) }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16 * ui.unit
            uniformCellSizes: true

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: 16 * ui.unit

                Card {
                    title: qsTr("Lights")
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.preferredWidth: page.labelWidth
                            text: qsTr("Lights on")
                        }
                        Item { Layout.fillWidth: true }
                        AppSwitch {
                            objectName: "lightsSwitch"
                            checked: backend.lightsOn
                            onToggled: backend.setLightsOn(checked)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.preferredWidth: page.labelWidth
                            text: qsTr("Brightness")
                        }
                        Slider {
                            id: brightness
                            FocusRing {}
                            objectName: "brightnessSlider"
                            Layout.fillWidth: true
                            from: 0
                            to: backend.maxBrightness
                            // No stepSize: Material would draw a tick dot per step.
                            onMoved: backend.setBrightness(Math.round(value))
                            Binding on value {
                                when: !brightness.pressed
                                value: backend.brightness
                                restoreMode: Binding.RestoreNone
                            }
                        }
                        Label {
                            Layout.preferredWidth: 64 * ui.unit
                            horizontalAlignment: Text.AlignRight
                            text: Math.round(brightness.value) + " / " + backend.maxBrightness
                            font.features: { "tnum": 1 }
                        }
                    }
                }
                Card {
                    title: qsTr("Effect")
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.preferredWidth: page.labelWidth
                            text: qsTr("Effect")
                        }
                        AppComboBox {
                            objectName: "effectBox"
                            Layout.fillWidth: true
                            Layout.maximumWidth: 300 * ui.unit
                            model: page.effectNames
                            currentIndex: backend.effect
                            onActivated: (index) => backend.setEffect(index)
                        }
                        Item { Layout.fillWidth: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.preferredWidth: page.labelWidth
                            text: qsTr("Speed")
                        }
                        Slider {
                            id: speed
                            FocusRing {}
                            objectName: "speedSlider"
                            Layout.fillWidth: true
                            from: 0
                            to: 4
                            // No stepSize: Material would draw a tick dot per step.
                            onMoved: backend.setSpeed(Math.round(value))
                            Binding on value {
                                when: !speed.pressed
                                value: backend.speed
                                restoreMode: Binding.RestoreNone
                            }
                        }
                        Label {
                            Layout.preferredWidth: 64 * ui.unit
                            horizontalAlignment: Text.AlignRight
                            text: Math.round(speed.value) + " / 4"
                            font.features: { "tnum": 1 }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: 16 * ui.unit

                Card {
                    title: qsTr("Colour")
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16 * ui.unit
                        // What the keyboard is showing now.
                        Rectangle {
                            implicitWidth: 56 * ui.unit
                            implicitHeight: 56 * ui.unit
                            radius: theme.radius
                            color: backend.color
                            visible: !backend.rainbow
                            border.color: Qt.alpha(theme.foreground, 0.19)
                        }
                        Rectangle {
                            implicitWidth: 56 * ui.unit
                            implicitHeight: 56 * ui.unit
                            radius: theme.radius
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
                            spacing: 10 * ui.unit
                            Repeater {
                                model: page.swatches
                                delegate: Rectangle {
                                    required property string modelData
                                    readonly property bool current: !backend.rainbow && backend.color === modelData
                                    width: 30 * ui.unit
                                    height: 30 * ui.unit
                                    radius: theme.radius
                                    color: modelData
                                    border.width: current ? 3 : 1
                                    border.color: current ? theme.accent : Qt.alpha(theme.foreground, 0.19)
                                    activeFocusOnTab: true
                                    Keys.onSpacePressed: backend.setColor(modelData)
                                    Keys.onReturnPressed: backend.setColor(modelData)
                                    FocusRing { anchors.margins: -4 * ui.unit }
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
                        Layout.fillWidth: true
                        Label {
                            Layout.preferredWidth: page.labelWidth
                            text: qsTr("Hex colour")
                        }
                        AppTextField {
                            id: hex
                            objectName: "hexField"
                            Layout.preferredWidth: 140 * ui.unit
                            font.family: "monospace"
                            validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{0,6}/ }
                            onAccepted: if (!backend.setColor(text)) hex.text = backend.color
                            Binding on text {
                                when: !hex.activeFocus
                                value: backend.color
                                restoreMode: Binding.RestoreNone
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.preferredWidth: page.labelWidth
                            text: qsTr("Rainbow")
                        }
                        Item { Layout.fillWidth: true }
                        AppSwitch {
                            objectName: "rainbowSwitch"
                            checked: backend.rainbow
                            onToggled: backend.setRainbow(checked)
                        }
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
