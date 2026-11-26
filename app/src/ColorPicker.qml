// SPDX-License-Identifier: GPL-3.0-or-later
// A colour picker under its title: a saturation (x) / brightness (y) field and a hue strip, then
// a hex field, a row of classic colours and two rows of saved colours. `compact` (a short window)
// puts the hex field on the title's row. It shows `color` (what the board
// reported) and reports a choice through `picked`, once per choice: when the field or the strip
// is let go, or a moment after the last arrow key. While a choice is under way, `dragging` is
// true and `liveColor` is the colour under the marker.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GridLayout {
    id: picker

    property string title
    property bool compact: false
    property string color  // "#rrggbb", or "" when there is no one colour to show (mixed keys)
    property var classics: []
    property bool markCurrent: true  // outline the swatch of the current colour
    signal picked(string color)

    // The colour under the marker. A grey keeps its hue, so the strip doesn't jump.
    property real h: 0
    property real s: 0
    property real v: 1
    property bool shown: false  // h, s, v show a colour (not so while the keys are mixed)
    readonly property bool dragging: fieldMouse.pressed || hueMouse.pressed || nudged.running
    readonly property string liveColor: colorPalette.hsvToHex(h, s, v)

    // Back to what the board reported. A choice under way keeps the user's value; after it,
    // the picker keeps it until the read-back changes `color`.
    function sync() {
        picker.shown = picker.color !== ""
        if (!picker.shown || picker.liveColor === picker.color)
            return
        const c = colorPalette.hexToHsv(picker.color, picker.h, picker.s)
        if (c.length === 3) {
            picker.h = c[0]
            picker.s = c[1]
            picker.v = c[2]
        }
    }
    onColorChanged: if (!picker.dragging) picker.sync()
    Component.onCompleted: picker.sync()

    // A colour chosen whole (a swatch, a saved colour, the hex field): sent at once.
    function choose(color) {
        const c = colorPalette.hexToHsv(color, picker.h, picker.s)
        if (c.length !== 3)
            return false
        picker.h = c[0]
        picker.s = c[1]
        picker.v = c[2]
        picker.shown = true
        picker.picked(colorPalette.normalized(color))
        return true
    }
    // Arrow keys move the marker; the colour goes once they stop.
    function nudge(dh, ds, dv) {
        picker.h = (picker.h + dh + 1) % 1
        picker.s = Math.min(1, Math.max(0, picker.s + ds))
        picker.v = Math.min(1, Math.max(0, picker.v + dv))
        picker.shown = true
        nudged.restart()
    }
    Timer {
        id: nudged
        interval: 300
        onTriggered: picker.picked(picker.liveColor)
    }

    columns: 3
    columnSpacing: 10 * ui.unit
    rowSpacing: 6 * ui.unit

    // As wide as it wants to be: every classic colour on one row. Given less, the swatches wrap.
    readonly property real swatchSize: 26 * ui.unit
    readonly property real swatchGap: 8 * ui.unit
    readonly property int naturalColumns: Math.max(10, picker.classics.length)
    readonly property real sideWidth: picker.naturalColumns * picker.swatchSize + (picker.naturalColumns - 1) * picker.swatchGap
    readonly property real naturalWidth: 176 * ui.unit + 14 * ui.unit + 2 * picker.columnSpacing + 6 * ui.unit + picker.sideWidth
    // The narrowest it gets: four swatches a row.
    readonly property real leastWidth: picker.naturalWidth - picker.sideWidth + 4 * picker.swatchSize + 3 * picker.swatchGap
    // Its height at naturalWidth, not compact: the title, the hex row, the classic colours on one
    // row, the saved ones ten a row.
    readonly property real rowHeight: 34 * ui.unit
    function rowsHeight(n) { return n * picker.swatchSize + (n - 1) * picker.swatchGap }
    readonly property real naturalHeight: 2 * picker.rowHeight + picker.rowSpacing + 2 * 8 * ui.unit + picker.rowsHeight(1)
                                          + picker.rowsHeight(Math.ceil(colorPalette.slotCount / 10))

    Label {
        objectName: "colorLabel"
        Layout.row: 0
        Layout.column: 0
        Layout.columnSpan: picker.compact ? 2 : 3
        Layout.fillWidth: true
        Layout.minimumHeight: picker.rowHeight
        Layout.maximumHeight: picker.rowHeight
        verticalAlignment: Text.AlignVCenter
        text: picker.title
        elide: Text.ElideRight
        font.pixelSize: ui.section
        font.weight: Font.DemiBold
    }

    // ---- saturation / brightness -------------------------------------------
    Rectangle {
        id: field
        objectName: "colorField"
        Layout.row: 1
        Layout.column: 0
        Layout.rowSpan: picker.compact ? 1 : 2
        Layout.alignment: Qt.AlignTop
        Layout.preferredWidth: 176 * ui.unit
        Layout.preferredHeight: picker.sideHeight  // as tall as the swatches beside it
        radius: theme.radius
        color: Qt.hsva(picker.h, 1, 1, 1)  // the pure hue, washed to white then darkened
        activeFocusOnTab: true
        Keys.onPressed: (event) => {
            const step = event.modifiers & Qt.ShiftModifier ? 0.1 : 0.01
            if (event.key === Qt.Key_Left) picker.nudge(0, -step, 0)
            else if (event.key === Qt.Key_Right) picker.nudge(0, step, 0)
            else if (event.key === Qt.Key_Up) picker.nudge(0, 0, step)
            else if (event.key === Qt.Key_Down) picker.nudge(0, 0, -step)
            else return
            event.accepted = true
        }

        Rectangle {
            anchors.fill: parent
            radius: theme.radius
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#ffffffff" }
                GradientStop { position: 1; color: "#00ffffff" }
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: theme.radius
            gradient: Gradient {
                GradientStop { position: 0; color: "#00000000" }
                GradientStop { position: 1; color: "#ff000000" }
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: theme.radius
            color: "transparent"
            border.color: Qt.alpha(theme.foreground, 0.19)
        }
        // A white ring with a dark edge: seen on any colour.
        Rectangle {
            objectName: "colorMarker"
            visible: picker.shown
            x: picker.s * field.width - width / 2
            y: (1 - picker.v) * field.height - height / 2
            width: 14 * ui.unit
            height: width
            radius: width / 2
            color: "transparent"
            border.width: Math.max(2, Math.round(2 * ui.unit))
            border.color: "#ffffff"
            Rectangle {
                anchors.fill: parent
                anchors.margins: -1
                radius: width / 2
                color: "transparent"
                border.color: "#99000000"
            }
        }
        MouseArea {
            id: fieldMouse
            anchors.fill: parent
            preventStealing: true
            cursorShape: Qt.CrossCursor
            function at(mouse) {
                picker.s = Math.min(1, Math.max(0, mouse.x / width))
                picker.v = 1 - Math.min(1, Math.max(0, mouse.y / height))
                picker.shown = true
            }
            onPressed: (mouse) => { field.forceActiveFocus(); at(mouse) }
            onPositionChanged: (mouse) => at(mouse)
            onReleased: picker.picked(picker.liveColor)
        }
        FocusRing { anchors.margins: -3 * ui.unit }
    }

    // ---- hue, red at the top -------------------------------------------------
    Rectangle {
        id: hueStrip
        objectName: "hueStrip"
        Layout.row: 1
        Layout.column: 1
        Layout.rowSpan: picker.compact ? 1 : 2
        Layout.alignment: Qt.AlignTop
        Layout.preferredWidth: 14 * ui.unit
        Layout.preferredHeight: picker.sideHeight
        radius: theme.radius
        border.color: Qt.alpha(theme.foreground, 0.19)
        gradient: Gradient {
            GradientStop { position: 0 / 6; color: "#ff0000" }
            GradientStop { position: 1 / 6; color: "#ffff00" }
            GradientStop { position: 2 / 6; color: "#00ff00" }
            GradientStop { position: 3 / 6; color: "#00ffff" }
            GradientStop { position: 4 / 6; color: "#0000ff" }
            GradientStop { position: 5 / 6; color: "#ff00ff" }
            GradientStop { position: 6 / 6; color: "#ff0000" }
        }
        activeFocusOnTab: true
        Keys.onPressed: (event) => {
            const step = event.modifiers & Qt.ShiftModifier ? 1 / 36 : 1 / 360
            if (event.key === Qt.Key_Up) picker.nudge(-step, 0, 0)
            else if (event.key === Qt.Key_Down) picker.nudge(step, 0, 0)
            else return
            event.accepted = true
        }

        // A notch across the strip, white with a dark edge like the marker.
        Rectangle {
            visible: picker.shown
            x: -3 * ui.unit
            y: picker.h * hueStrip.height - height / 2
            width: hueStrip.width + 6 * ui.unit
            height: 6 * ui.unit
            radius: height / 2
            color: Qt.hsva(picker.h, 1, 1, 1)
            border.width: Math.max(2, Math.round(2 * ui.unit))
            border.color: "#ffffff"
            Rectangle {
                anchors.fill: parent
                anchors.margins: -1
                radius: height / 2
                color: "transparent"
                border.color: "#99000000"
            }
        }
        MouseArea {
            id: hueMouse
            anchors.fill: parent
            anchors.margins: -3 * ui.unit  // the notch overhangs: so does the grip
            preventStealing: true
            cursorShape: Qt.PointingHandCursor
            function at(mouse) {
                // Just under 1 at the bottom: hue 1 is red again, and the notch should stay put.
                picker.h = Math.min(0.9999, Math.max(0, (mouse.y - 3 * ui.unit) / hueStrip.height))
                picker.shown = true
            }
            onPressed: (mouse) => { hueStrip.forceActiveFocus(); at(mouse) }
            onPositionChanged: (mouse) => at(mouse)
            onReleased: picker.picked(picker.liveColor)
        }
        FocusRing { anchors.margins: -3 * ui.unit }
    }

    // ---- hex, classic colours, saved colours -------------------------------------
    // The hex row over the swatches, or on the title's row when compact.
    readonly property real sideHeight: (picker.compact ? 0 : hexRow.implicitHeight + 8 * ui.unit) + side.implicitHeight
    RowLayout {
        id: hexRow
        Layout.row: picker.compact ? 0 : 1
        Layout.column: 2
        Layout.alignment: Qt.AlignTop
        Layout.leftMargin: 6 * ui.unit
        spacing: 8 * ui.unit
        // The colour under the marker, as the LED shows it.
        Rectangle {
            objectName: "selectedColor"
            implicitWidth: 34 * ui.unit
            implicitHeight: 34 * ui.unit
            radius: theme.radius
            color: picker.shown ? picker.liveColor : "transparent"
            border.color: Qt.alpha(theme.foreground, 0.19)
        }
        AppTextField {
            id: hex
            objectName: "hexField"
            Layout.preferredWidth: 110 * ui.unit
            font.family: "monospace"
            // Selected keys of different colours.
            placeholderText: picker.shown ? "" : qsTr("Mixed")
            validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{0,6}/ }
            onAccepted: if (!picker.choose(text)) hex.text = picker.shown ? picker.liveColor : ""
            Binding on text {
                when: !hex.activeFocus
                value: picker.shown ? picker.liveColor : ""
                restoreMode: Binding.RestoreNone
            }
        }
    }
    ColumnLayout {
        id: side
        Layout.row: picker.compact ? 1 : 2
        Layout.column: 2
        Layout.alignment: Qt.AlignTop
        Layout.topMargin: picker.compact ? 0 : 8 * ui.unit - picker.rowSpacing
        Layout.leftMargin: 6 * ui.unit
        Layout.fillWidth: true
        Layout.preferredWidth: picker.sideWidth
        Layout.minimumWidth: 4 * picker.swatchSize + 3 * picker.swatchGap
        spacing: 8 * ui.unit

        Flow {
            Layout.fillWidth: true
            spacing: picker.swatchGap
            Repeater {
                model: picker.classics
                delegate: Swatch {
                    required property string modelData
                    objectName: "swatch_" + modelData.slice(1)
                    value: modelData
                    tip: modelData === "#000000" ? qsTr("Black: the key stays dark") : ""
                    onChosen: picker.choose(modelData)
                }
            }
        }

        // Saved colours: an empty slot saves the current colour; right-click (or the menu key)
        // replaces or removes a saved one.
        Grid {
            objectName: "savedColors"
            columns: Math.max(4, Math.min(10, Math.floor((side.width + picker.swatchGap) / (picker.swatchSize + picker.swatchGap))))
            spacing: picker.swatchGap
            Repeater {
                model: colorPalette.slotCount
                delegate: Swatch {
                    id: savedSlot
                    required property int index
                    objectName: "savedColor_" + index
                    value: colorPalette.saved[index]
                    tip: value === "" ? qsTr("Save the current color here")
                                      : qsTr("Right-click to replace or remove")
                    onChosen: {
                        if (value !== "")
                            picker.choose(value)
                        else if (picker.shown)
                            colorPalette.save(index, picker.liveColor)
                    }
                    onEdit: (x, y) => {
                        if (value === "")
                            return
                        slotMenu.slot = index
                        slotMenu.popup(savedSlot, x, y)
                    }
                    Keys.onDeletePressed: colorPalette.remove(index)
                }
            }
        }
    }

    Menu {
        id: slotMenu
        objectName: "savedColorMenu"
        property int slot: -1
        MenuItem {
            text: qsTr("Replace with the current color")
            enabled: picker.shown
            onTriggered: colorPalette.save(slotMenu.slot, picker.liveColor)
        }
        MenuItem {
            text: qsTr("Remove")
            onTriggered: colorPalette.remove(slotMenu.slot)
        }
    }

    // One colour square: filled with `value`, or an empty slot ("") with a faint plus.
    component Swatch: Rectangle {
        id: swatch
        property string value
        property string tip
        readonly property bool current: picker.markCurrent && picker.color !== "" && picker.color === value
        signal chosen
        signal edit(real x, real y)

        width: picker.swatchSize
        height: picker.swatchSize
        radius: theme.radius
        color: value !== "" ? value : "transparent"
        border.width: current ? 3 : 1
        border.color: current ? theme.accent : Qt.alpha(theme.foreground, value !== "" ? 0.19 : 0.28)
        ToolTip.visible: tip !== "" && swatchMouse.containsMouse
        ToolTip.text: tip
        ToolTip.delay: 500
        activeFocusOnTab: true
        Keys.onSpacePressed: swatch.chosen()
        Keys.onReturnPressed: swatch.chosen()
        Keys.onMenuPressed: swatch.edit(0, swatch.height)

        Icon {
            anchors.centerIn: parent
            visible: swatch.value === ""
            name: "plus"
            size: 12 * ui.unit
            color: Qt.alpha(theme.foreground, swatchMouse.containsMouse ? 0.7 : 0.3)
        }
        FocusRing { anchors.margins: -4 * ui.unit }
        MouseArea {
            id: swatchMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: Qt.PointingHandCursor
            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton)
                    swatch.edit(mouse.x, mouse.y)
                else
                    swatch.chosen()
            }
        }
    }
}
