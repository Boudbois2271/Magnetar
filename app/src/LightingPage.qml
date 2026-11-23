// SPDX-License-Identifier: GPL-3.0-or-later
// Lighting: on/off and brightness, effect and speed, colour or rainbow, and the colour of each
// key under the Custom effect: with keys selected on the drawing, the colour controls set those
// keys instead of the whole board. Controls show what the keyboard reported; a control being
// dragged or edited keeps the user's value until they let go.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: page
    objectName: "lightingPage"

    // ---- selection: keys whose own colour is being set ---------------------
    property var selection: []  // key IDs, in the order they were picked
    // A click selects only that key, or clears it when it is the only one selected; Ctrl or
    // Shift adds it, or takes it out. A box selects the keys in it; with Ctrl or Shift it adds
    // them. As on the Keys page.
    function click(id, modifiers) {
        if (modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) {
            const i = page.selection.indexOf(id)
            let s = page.selection.slice()
            if (i >= 0)
                s.splice(i, 1)
            else
                s.push(id)
            page.selection = s
        } else if (page.selection.length === 1 && page.selection[0] === id) {
            page.selection = []
        } else {
            page.selection = [id]
        }
    }
    function box(ids, modifiers) {
        if (modifiers & (Qt.ControlModifier | Qt.ShiftModifier))
            page.selection = page.selection.concat(ids.filter(id => page.selection.indexOf(id) < 0))
        else
            page.selection = ids.slice()
    }
    function select(ids) {
        page.selection = ids.slice()
    }
    function allKeys() {
        return backend.layout.map(k => k.id)
    }

    readonly property bool perKey: page.selection.length > 0
    readonly property bool customOn: backend.effect === backend.customEffect
    readonly property var keyColors: backend.colorsReady ? backend.keyColors : ({})
    // The colour every selected key has, or "" when they differ.
    readonly property var picked: page.selection.map(id => page.keyColors[String(id)]).filter(c => c !== undefined)
    readonly property string shared: page.picked.length > 0 && page.picked.every(c => c === page.picked[0]) ? page.picked[0] : ""
    // What the colour controls show: the selected keys' colour, or the board's.
    readonly property string shownColor: page.perKey ? page.shared : backend.color

    // A colour for the selected keys, or for the whole board when none is selected.
    function apply(color) {
        return page.perKey ? backend.setKeyColors(page.selection, color) : backend.setColor(color)
    }

    // Indexed by effect number (00 … 13), as decoded in akko-protocol.md.
    readonly property var effectNames: [
        qsTr("Off"), qsTr("Steady"), qsTr("Breathing"), qsTr("Dream rainbow"),
        qsTr("Instant trigger"), qsTr("Walking in the rain"), qsTr("Rainbow wheel"), qsTr("Ripple"),
        qsTr("Starry night"), qsTr("Snow trail"), qsTr("Endless flow"), qsTr("Flowing water"),
        qsTr("Shadow follow"), qsTr("Sine wave"), qsTr("Left-right scan"), qsTr("Spinning windmill"),
        qsTr("Colorful waterfall"), qsTr("Blossom prosperity"), qsTr("Spinning storm"),
        qsTr("Custom (per-key colours)")
    ]
    // Black is "off" for a key; the board's colour has no black.
    readonly property var swatches: [
        "#ff0000", "#ff6a00", "#ffd000", "#00ff00", "#00ffc8", "#00a0ff",
        "#0000ff", "#8000ff", "#ff00c8", "#ffffff"
    ].concat(page.perKey ? ["#000000"] : [])
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
        id: frame
        title: qsTr("Lighting")
        // Nothing to show until the keyboard has been read once: no made-up values.
        visible: backend.ready
        enabled: backend.connected

        // What the board shows now; a click or a box picks keys to colour one by one.
        // The box takes in the drawing's drag margin, so a drag that starts there reaches it.
        Item {
            readonly property real margin: preview.dragMargin
            Layout.preferredWidth: preview.width + 2 * margin
            Layout.preferredHeight: preview.height + selectionBar.height + 2 * margin
            Layout.leftMargin: -margin
            Layout.topMargin: -margin
            Layout.bottomMargin: -margin

            KeyboardView {
                id: preview
                objectName: "lightingPreview"
                x: parent.margin
                y: parent.margin
                width: Math.min(implicitWidth, frame.width)
                height: implicitHeight
                maxKeySize: 44 * ui.unit
                squareBottom: true
                labelFor: function (id) {
                    return backend.keymapReady ? names.capName(backend.keymap[0][String(id)]) : ""
                }
                colorFor: function (id) { return page.previewColor(page.keyById[id]) }
                selectedKeys: page.selection
                onKeyClicked: (id, modifiers) => page.click(id, modifiers)
                onBoxSelected: (ids, modifiers) => page.box(ids, modifiers)
            }

            // Attached under the plate, on the same surface.
            Rectangle {
                id: selectionBar
                x: preview.x
                y: preview.y + preview.height
                width: preview.width
                height: 34 * ui.unit + preview.inset
                bottomLeftRadius: theme.radius
                bottomRightRadius: theme.radius
                color: theme.plate

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: preview.inset
                    anchors.rightMargin: preview.inset
                    spacing: 6 * ui.unit

                    AppButton { text: qsTr("All keys"); onClicked: page.select(page.allKeys()) }
                    AppButton {
                        text: qsTr("Invert")
                        onClicked: page.select(page.allKeys().filter(id => page.selection.indexOf(id) < 0))
                    }
                    AppButton {
                        visible: page.perKey
                        text: qsTr("None")
                        onClicked: page.select([])
                    }
                    Label {
                        objectName: "lightingSelectedCount"
                        visible: page.perKey
                        Layout.leftMargin: 10 * ui.unit
                        text: page.selection.length === 1 ? qsTr("1 key selected")
                                                          : qsTr("%1 keys selected").arg(page.selection.length)
                        color: theme.secondary
                        font.features: { "tnum": 1 }
                    }
                    Item { Layout.fillWidth: true }

                    // Key colours only show under the Custom effect.
                    Label {
                        objectName: "customHint"
                        visible: page.perKey && !page.customOn
                        text: qsTr("Custom effect is off")
                        color: theme.secondary
                    }
                    AppButton {
                        objectName: "switchToCustom"
                        visible: page.perKey && !page.customOn
                        kind: "primary"
                        text: qsTr("Switch to Custom")
                        onClicked: backend.setEffect(backend.customEffect)
                    }
                }
            }
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
                    // With keys selected, the colour goes to them, not to the board.
                    title: page.perKey ? qsTr("Key color") : qsTr("Colour")
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16 * ui.unit
                        // What the keyboard is showing now; for the selected keys, "Mixed" when
                        // they differ.
                        Rectangle {
                            objectName: "selectedColor"
                            implicitWidth: 56 * ui.unit
                            implicitHeight: 56 * ui.unit
                            radius: theme.radius
                            color: page.shownColor !== "" ? page.shownColor : "transparent"
                            visible: page.perKey || !backend.rainbow
                            border.color: Qt.alpha(theme.foreground, 0.19)
                            Label {
                                anchors.centerIn: parent
                                visible: page.shownColor === ""
                                text: qsTr("Mixed")
                                font.pixelSize: ui.small
                                color: theme.secondary
                            }
                        }
                        Rectangle {
                            implicitWidth: 56 * ui.unit
                            implicitHeight: 56 * ui.unit
                            radius: theme.radius
                            visible: !page.perKey && backend.rainbow
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
                                    readonly property bool current: (page.perKey || !backend.rainbow)
                                                                    && page.shownColor === modelData
                                    objectName: "swatch_" + modelData.slice(1)
                                    width: 30 * ui.unit
                                    height: 30 * ui.unit
                                    radius: theme.radius
                                    color: modelData
                                    border.width: current ? 3 : 1
                                    border.color: current ? theme.accent : Qt.alpha(theme.foreground, 0.19)
                                    ToolTip.visible: modelData === "#000000" && swatchMouse.containsMouse
                                    ToolTip.text: qsTr("Black: the key stays dark")
                                    activeFocusOnTab: true
                                    Keys.onSpacePressed: page.apply(modelData)
                                    Keys.onReturnPressed: page.apply(modelData)
                                    FocusRing { anchors.margins: -4 * ui.unit }
                                    MouseArea {
                                        id: swatchMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: page.apply(parent.modelData)
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
                            onAccepted: if (!page.apply(text)) hex.text = page.shownColor
                            Binding on text {
                                when: !hex.activeFocus
                                value: page.shownColor
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
