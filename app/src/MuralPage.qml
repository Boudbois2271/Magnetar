// SPDX-License-Identifier: GPL-3.0-or-later
// Mural: an audio visualiser on the keys. While on, what the computer plays lights the board as
// spectrum bars, as long as Magnetar is open (on any page, minimised too). Nothing is stored on
// the keyboard: it goes back to its own lighting when the visualiser stops. The drawing shows
// what is being sent; the settings are app preferences, kept between launches.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    objectName: "muralPage"

    readonly property real labelWidth: 140 * ui.unit
    readonly property real rowHeight: 34 * ui.unit
    readonly property real sliderWidth: 160 * ui.unit
    readonly property var swatches: [
        "#ff0000", "#ff6a00", "#ffd000", "#00ff00", "#00ffc8", "#00a0ff",
        "#0000ff", "#8000ff", "#ff00c8", "#ffffff"
    ]

    // As large as on the Lighting page, leaving room for the options under it.
    readonly property real drawingWidth: {
        const room = frame.height - frame.headerHeight - 3 * frame.spacing - intro.implicitHeight - options.implicitHeight
        const perWidth = 1 / preview.aspect
        return Math.max(400 * ui.unit, Math.min(frame.width, preview.implicitWidth, room / perWidth))
    }

    KeyNames { id: names }

    // A sent colour on the cap: its hue at full strength, mixed into the unlit cap as much as
    // the key is lit. A dark key shows the plain cap.
    function capColor(id) {
        const c = mural.colors[String(id)]
        if (c === undefined)
            return ""
        const led = Qt.color(c)
        const v = Math.max(led.r, led.g, led.b)
        if (v <= 0)
            return ""
        return Qt.tint(theme.capTop, Qt.rgba(led.r / v, led.g / v, led.b / v, v)).toString()
    }

    PageLayout {
        id: frame
        title: qsTr("Mural")

        Label {
            id: intro
            Layout.fillWidth: true
            text: qsTr("Spectrum bars from what your computer plays, while Magnetar is open. Nothing is stored on the keyboard: it goes back to its own lighting when the visualiser stops.")
            color: theme.secondary
            wrapMode: Text.WordWrap
        }

        KeyboardView {
            id: preview
            objectName: "muralPreview"
            Layout.preferredWidth: page.drawingWidth
            Layout.preferredHeight: implicitHeight
            width: page.drawingWidth
            maxKeySize: 68 * ui.unit
            interactive: false
            labelFor: function (id) {
                return backend.keymapReady ? names.capName(backend.keymap[0][String(id)]) : ""
            }
            colorFor: function (id) { return page.capColor(id) }
        }

        RowLayout {
            id: options
            objectName: "muralOptions"
            Layout.fillWidth: true
            spacing: 0

            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                spacing: 6 * ui.unit

                // Visualiser: a title carrying the switch.
                RowLayout {
                    Layout.minimumHeight: page.rowHeight
                    Layout.maximumHeight: page.rowHeight
                    spacing: 0
                    Label {
                        Layout.preferredWidth: page.labelWidth
                        text: qsTr("Visualiser")
                        font.pixelSize: ui.section
                        font.weight: Font.DemiBold
                    }
                    AppSwitch {
                        objectName: "muralSwitch"
                        Layout.maximumHeight: page.rowHeight
                        Layout.leftMargin: -indicator.x  // the track lines up with the other controls
                        checked: mural.on
                        onToggled: {
                            mural.on = checked
                            checked = Qt.binding(() => mural.on)
                        }
                    }
                    Label {
                        objectName: "muralNoSound"
                        visible: mural.on && !mural.hearing
                        Layout.leftMargin: 12 * ui.unit
                        text: qsTr("Can't hear the computer's sound")
                        color: theme.error
                    }
                }
                RowLayout {
                    Layout.minimumHeight: page.rowHeight
                    Layout.maximumHeight: page.rowHeight
                    spacing: 12 * ui.unit
                    Label {
                        Layout.preferredWidth: page.labelWidth - parent.spacing
                        text: qsTr("Sensitivity")
                    }
                    // Doubling steps: the middle is 1×, the ends a quarter and four times.
                    Slider {
                        id: sensitivity
                        FocusRing {}
                        objectName: "muralSensitivity"
                        Layout.preferredWidth: page.sliderWidth
                        Layout.preferredHeight: page.rowHeight
                        Layout.leftMargin: -leftPadding - handle.width / 2  // the track lines up
                        from: Math.log2(mural.sensitivityMin)
                        to: Math.log2(mural.sensitivityMax)
                        onMoved: mural.sensitivity = Math.pow(2, value)
                        Binding on value {
                            when: !sensitivity.pressed
                            value: Math.log2(mural.sensitivity)
                            restoreMode: Binding.RestoreNone
                        }
                    }
                }
                RowLayout {
                    Layout.minimumHeight: page.rowHeight
                    Layout.maximumHeight: page.rowHeight
                    spacing: 12 * ui.unit
                    Label {
                        Layout.preferredWidth: page.labelWidth - parent.spacing
                        text: qsTr("Colors")
                    }
                    Segmented {
                        objectName: "muralColorMode"
                        model: [qsTr("Rainbow"), qsTr("One color")]
                        currentIndex: mural.rainbow ? 0 : 1
                        onActivated: (index) => mural.rainbow = index === 0
                    }
                }
            }

            // The one colour; picking one switches to it.
            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                Layout.leftMargin: 48 * ui.unit
                spacing: 6 * ui.unit
                Label {
                    Layout.minimumHeight: page.rowHeight
                    Layout.maximumHeight: page.rowHeight
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Color")
                    font.pixelSize: ui.section
                    font.weight: Font.DemiBold
                }
                ColorPicker {
                    id: picker
                    objectName: "muralColorPicker"
                    color: mural.color
                    classics: page.swatches
                    markCurrent: !mural.rainbow
                    onPicked: (color) => {
                        mural.color = color
                        mural.rainbow = false
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
