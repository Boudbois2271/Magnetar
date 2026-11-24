// SPDX-License-Identifier: GPL-3.0-or-later
// Mural: the workbench for lighting effects the app streams to the keys (Mural frames, 08 04).
// One effect at a time is tried here, then moves to the effect list on Lighting, as the
// Visualiser did. The drawing shows what is being sent now, whatever is sending it.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    objectName: "muralPage"

    // As large as on the Lighting page, leaving room for the text above it.
    readonly property real drawingWidth: {
        const room = frame.height - frame.headerHeight - 2 * frame.spacing - intro.implicitHeight
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
            text: qsTr("New effects streamed by the app are tried here, one at a time, before they join the effects on Lighting. None in progress: the Visualiser is on Lighting. The drawing shows what is being sent.")
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
        Item { Layout.fillHeight: true }
    }
}
