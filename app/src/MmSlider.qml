// SPDX-License-Identifier: GPL-3.0-or-later
// A distance in mm: slider and typed value. Sends commit(value) once, on release or Enter,
// and only when it changes something. While the slider is dragged, the box shows where it is.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: ms
    property string label
    property var shown        // the selected keys' most common value
    property bool mixed: false
    property real from: 0
    property real to: 1
    property real step: 0.005
    property int decimals: 3
    property bool french: false
    property bool heading: false  // the label is a section title (a section of one row)
    property real sliderWidth: 0  // a fixed slider length, on one row; else it fills the width
    property real boxWidth: 76 * ui.unit  // the typed value's box
    signal commit(real value)
    // Where the slider's track starts, from the left edge: other rows line up with it.
    readonly property real trackLeft: slider.x + slider.leftPadding

    Layout.fillWidth: true
    spacing: 0

    function snap(v) {
        return Math.min(ms.to, Math.max(ms.from, Math.round(v / ms.step) * ms.step))
    }
    function send(v) {
        v = ms.snap(v)
        if (!ms.mixed && ms.shown !== undefined && Math.abs(v - ms.shown) < ms.step / 2)
            return
        ms.commit(Number(v.toFixed(3)))
    }
    function text(v) {
        if (v === undefined)
            return ""
        const s = Number(v).toFixed(ms.decimals)
        return ms.french ? s.replace(".", ",") : s
    }

    // Name, slider, value and unit on one row when there is room; the slider below otherwise.
    property real labelWidth: 150 * ui.unit
    readonly property bool oneRow: ms.sliderWidth > 0 || ms.width >= ms.labelWidth + 200 * ui.unit
    GridLayout {
        Layout.fillWidth: true
        columns: 6
        columnSpacing: 8 * ui.unit
        rowSpacing: 0
        Label {
            Layout.row: 0
            Layout.column: 0
            Layout.fillWidth: !ms.oneRow
            Layout.preferredWidth: ms.oneRow ? ms.labelWidth : -1
            text: ms.label
            elide: Text.ElideRight
            font.pixelSize: ms.heading ? ui.section : ui.body
            font.weight: ms.heading ? Font.DemiBold : Font.Normal
        }
        // After the unit, so the sliders of a column keep one length.
        Label {
            Layout.row: 0
            Layout.column: 5
            visible: ms.mixed && !slider.pressed
            text: qsTr("varies")
            color: theme.secondary
        }
        AppTextField {
            id: typed
            objectName: ms.objectName + "Field"
            Layout.row: 0
            Layout.column: 3
            Layout.preferredWidth: ms.boxWidth
            horizontalAlignment: TextInput.AlignRight
            font.features: { "tnum": 1 }
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,1}([.,][0-9]{0,3})?/ }
            Binding on text {
                when: !typed.activeFocus
                value: ms.text(slider.pressed ? ms.snap(slider.value) : ms.shown)
                restoreMode: Binding.RestoreNone
            }
            onEditingFinished: {
                const v = Number(text.replace(",", "."))
                if (text !== "" && !isNaN(v))
                    ms.send(v)
                focus = false
            }
        }
        Label {
            Layout.row: 0
            Layout.column: 4
            text: "mm"
        }
        Slider {
            id: slider
            FocusRing {}
            objectName: ms.objectName + "Bar"
            Layout.fillWidth: ms.sliderWidth <= 0
            Layout.preferredWidth: ms.sliderWidth > 0 ? ms.sliderWidth : -1
            Layout.row: ms.oneRow ? 0 : 1
            Layout.column: ms.oneRow ? 1 : 0
            Layout.columnSpan: ms.oneRow ? 1 : 6
            Layout.maximumHeight: 34 * ui.unit  // the row's height, not Material's touch height
            from: ms.from
            to: ms.to
            // No stepSize: Material draws a tick per step. send() rounds to the step instead.
            onPressedChanged: if (!pressed) ms.send(value)
            onMoved: if (!pressed) ms.send(value)  // arrow keys
            Binding on value {
                when: !slider.pressed
                value: ms.shown === undefined ? ms.from : ms.shown
                restoreMode: Binding.RestoreNone
            }
        }
    }
}
