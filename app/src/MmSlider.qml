// SPDX-License-Identifier: GPL-3.0-or-later
// A distance in mm: slider and typed value. Sends commit(value) once, on release or Enter,
// and only when it changes something.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: ms
    property string label
    property var shown        // the selected keys' most common value
    property bool mixed: false
    property real highest: 0  // largest value among the selected keys
    property real from: 0
    property real to: 1
    property real step: 0.005
    property int decimals: 3
    property bool french: false
    signal commit(real value)

    readonly property bool beyond: ms.highest > ms.to + 1e-9
    Layout.fillWidth: true
    spacing: 0

    function send(v) {
        v = Math.min(ms.to, Math.max(ms.from, Math.round(v / ms.step) * ms.step))
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

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Label {
            Layout.fillWidth: true
            text: ms.label
            elide: Text.ElideRight
        }
        Label {
            visible: ms.mixed
            text: qsTr("varies")
            opacity: 0.55
        }
        TextField {
            id: typed
            objectName: ms.objectName + "Field"
            Layout.preferredWidth: 76
            horizontalAlignment: TextInput.AlignRight
            enabled: !ms.beyond
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,1}([.,][0-9]{0,3})?/ }
            Binding on text {
                when: !typed.activeFocus
                value: ms.text(ms.shown)
                restoreMode: Binding.RestoreNone
            }
            onEditingFinished: {
                const v = Number(text.replace(",", "."))
                if (text !== "" && !isNaN(v))
                    ms.send(v)
                focus = false
            }
        }
        Label { text: "mm" }
    }
    Slider {
        id: slider
        objectName: ms.objectName + "Bar"
        Layout.fillWidth: true
        from: ms.from
        to: ms.to
        // No stepSize: Material draws a tick per step. send() rounds to the step instead.
        enabled: !ms.beyond
        onPressedChanged: if (!pressed) ms.send(value)
        onMoved: if (!pressed) ms.send(value)  // arrow keys
        Binding on value {
            when: !slider.pressed
            value: ms.shown === undefined ? ms.from : ms.shown
            restoreMode: Binding.RestoreNone
        }
    }
    Label {
        Layout.fillWidth: true
        visible: ms.beyond
        text: qsTr("Some selected keys are set beyond %1 mm, so Magnetar leaves this value as it is.").arg(ms.text(ms.to))
        wrapMode: Text.WordWrap
        opacity: 0.6
    }
}
