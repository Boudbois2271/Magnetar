// SPDX-License-Identifier: GPL-3.0-or-later
// Advanced keys: click a key, choose a type, fill in its actions and depths, save. The drawing
// and the list show what the keyboard read back; the editor holds a draft until Save.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: page

    property int selectedKey: -1
    property var draft: null   // the record being edited, same shape as backend.advancedKeys
    property int slot: -1      // the action slot the picker fills: 0 action, 1 tap, 10+i point, 20+i DKS row
    readonly property var limits: backend.advancedLimits
    readonly property bool french: translator.language.indexOf("fr") === 0

    KeyNames { id: names }

    readonly property var types: ["modTap", "toggle", "release", "multiPoint", "dks", "socd"]

    function typeName(t) {
        switch (t) {
        case "modTap": return qsTr("Mod-Tap")
        case "toggle": return qsTr("Toggle")
        case "release": return qsTr("Release trigger")
        case "multiPoint": return qsTr("Multi-point")
        case "dks": return qsTr("Dynamic keystroke")
        case "socd": return qsTr("SOCD")
        }
        return t
    }
    function typeTag(t) {
        switch (t) {
        case "modTap": return "MT"
        case "toggle": return "TGL"
        case "release": return "END"
        case "multiPoint": return "MPT"
        case "dks": return "DKS"
        case "socd": return "SOCD"
        }
        return "?"
    }
    function typeHelp(t) {
        switch (t) {
        case "modTap": return qsTr("Does one thing when tapped and another when held longer than the hold time.")
        case "toggle": return qsTr("A tap switches the key to staying pressed, the next tap releases it. Held longer than the hold time, it works as a normal key.")
        case "release": return qsTr("Sends its action when the key is let go instead of when it is pressed.")
        case "multiPoint": return qsTr("Up to three actions, each sent when the key goes past its depth.")
        case "dks": return qsTr("Up to four actions, each sent once or held between the four points of the key's travel: two going down, two coming up.")
        case "socd": return qsTr("Two keys that cancel each other out, like left and right in a game. Choose what wins when both are down.")
        }
        return ""
    }

    function copy(o) { return o === null || o === undefined ? null : JSON.parse(JSON.stringify(o)) }
    // Same content, whatever the order of the fields; a DKS row's stage bytes only count when
    // the editor can't draw them.
    function canon(o) {
        if (Array.isArray(o))
            return "[" + o.map(page.canon).join(",") + "]"
        if (o !== null && typeof o === "object")
            return "{" + Object.keys(o).filter(k => !(k === "stages" && (o.from >= 0 || o.stages === "00 00 00 00"))).sort()
                .map(k => k + ":" + page.canon(o[k])).join(",") + "}"
        return JSON.stringify(o)
    }

    function recordFor(id) {
        for (const r of backend.advancedKeys)
            if (r.key === id || (r.type === "socd" && r.key2 === id))
                return r
        return null
    }
    readonly property var saved: page.selectedKey >= 0 ? page.recordFor(page.selectedKey) : null
    readonly property bool dirty: page.draft !== null && (page.saved === null || page.canon(page.draft) !== page.canon(page.saved))

    function keyName(id) {
        const layer = backend.keymapReady && backend.keymap.length > 0 ? backend.keymap[0] : ({})
        return names.capName(layer[String(id)])
    }
    function actionName(code) {
        return code === "" ? qsTr("Choose…") : names.name(backend.actionInfo(code))
    }

    function newDraft(type) {
        const key = page.selectedKey
        switch (type) {
        case "modTap": return { type: type, key: key, action: "", tap: "", holdMs: page.limits.defaultHoldMs }
        case "toggle": {
            const layer = backend.keymapReady ? backend.keymap[0] : ({})
            const own = layer[String(key)]
            return { type: type, key: key, action: own ? own.code : "", holdMs: page.limits.defaultHoldMs }
        }
        case "release": return { type: type, key: key, action: "" }
        case "multiPoint": return { type: type, key: key,
                                    points: page.limits.pointDepths.map(d => ({ action: "", depth: d })) }
        case "dks": return { type: type, key: key, depths: page.limits.dksDepths.slice(),
                             rows: [0, 1, 2, 3].map(() => ({ action: "", from: -1, to: -1 })) }
        case "socd": return { type: type, key: key, key2: -1, mode: "lastInput" }
        }
        return null
    }
    function load() {
        // Not page.saved: this runs from onSelectedKeyChanged, maybe before that binding updates.
        page.draft = page.copy(page.selectedKey >= 0 ? page.recordFor(page.selectedKey) : null)
        page.slot = -1
    }
    function edit(change) {
        let d = page.copy(page.draft)
        change(d)
        page.draft = d
    }
    function chooseType(t) {
        page.draft = page.saved !== null && page.saved.type === t ? page.copy(page.saved) : page.newDraft(t)
        page.slot = -1
    }

    // Action slots: what the picker reads and fills.
    function slotCode(n) {
        const d = page.draft
        if (!d || n < 0)
            return ""
        if (n === 0) return d.action
        if (n === 1) return d.tap
        if (n >= 20) return d.rows[n - 20].action
        return d.points[n - 10].action
    }
    function setSlot(n, code) {
        page.edit(d => {
            if (n === 0) d.action = code
            else if (n === 1) d.tap = code
            else if (n >= 20) {
                d.rows[n - 20].action = code
                if (code !== "" && d.rows[n - 20].from < 0 && !page.customRow(d.rows[n - 20]))
                    d.rows[n - 20].from = d.rows[n - 20].to = 0  // a new row fires once at the first point
                if (code === "")
                    d.rows[n - 20].from = d.rows[n - 20].to = -1
            } else
                d.points[n - 10].action = code
        })
    }
    function slotName(n) {
        const d = page.draft
        if (!d || n < 0)
            return ""
        if (n === 0)
            return d.type === "modTap" ? qsTr("When held") : d.type === "release" ? qsTr("On release") : qsTr("Key action")
        if (n === 1) return qsTr("When tapped")
        if (n >= 20) return qsTr("Action %1").arg(n - 19)
        return qsTr("Point %1").arg(n - 9)
    }

    // DKS: the four points of the travel, and what a row can do with them.
    readonly property var stageNames: [qsTr("going down, 1st point"), qsTr("going down, 2nd point"),
                                       qsTr("coming up, 1st point"), qsTr("coming up, 2nd point")]
    readonly property var strokes: {
        let s = [{ label: qsTr("Not used"), from: -1, to: -1 }]
        for (let i = 0; i < 4; ++i)
            s.push({ label: qsTr("Once, %1").arg(page.stageNames[i]), from: i, to: i })
        for (let a = 0; a < 4; ++a)
            for (let b = a + 1; b < 4; ++b)
                s.push({ label: qsTr("Held, %1 → %2").arg(page.stageNames[a]).arg(page.stageNames[b]), from: a, to: b })
        return s
    }
    function customRow(r) {
        return r.from < 0 && r.action !== "" && r.stages !== undefined && r.stages !== "00 00 00 00"
    }

    // Why Save is greyed out, in the user's words.
    function problem() {
        const d = page.draft
        if (!d)
            return ""
        switch (d.type) {
        case "modTap":
            if (d.action === "" || d.tap === "")
                return qsTr("Choose both actions.")
            break
        case "toggle":
        case "release":
            if (d.action === "")
                return qsTr("Choose an action.")
            break
        case "multiPoint":
            if (d.points.every(p => p.action === ""))
                return qsTr("Choose at least one action.")
            break
        case "dks":
            if (d.rows.every(r => r.action === ""))
                return qsTr("Choose at least one action.")
            if (d.depths[0] >= d.depths[1] || d.depths[3] >= d.depths[2])
                return qsTr("Each 2nd point going down, and each 1st point coming up, must be deeper than the other point.")
            break
        case "socd":
            if (d.key2 < 0)
                return qsTr("Choose the second key.")
            break
        }
        return backend.advancedValid(d) ? "" : qsTr("Some values are out of range.")
    }
    // What saving takes away besides this key's own record.
    function replaces() {
        const d = page.draft
        if (!d)
            return ""
        let lost = []
        if (page.saved !== null && page.saved.type !== d.type)
            lost.push(qsTr("Saving replaces its %1.").arg(page.typeName(page.saved.type)))
        if (page.saved !== null && page.saved.type === "socd" && d.type !== "socd") {
            const other = page.saved.key === page.selectedKey ? page.saved.key2 : page.saved.key
            lost.push(qsTr("%1 loses its SOCD too.").arg(page.keyName(other)))
        }
        if (d.type === "socd" && d.key2 >= 0) {
            const r = page.recordFor(d.key2)
            if (r !== null && r !== page.saved)
                lost.push(qsTr("Saving removes the %1 on %2.").arg(page.typeName(r.type)).arg(page.keyName(d.key2)))
        }
        return lost.join(" ")
    }
    function save() {
        if (page.dirty && page.problem() === "")
            backend.saveAdvanced([page.draft])
    }

    onSelectedKeyChanged: page.load()
    // The read-back replaces the draft unless the user is in the middle of an edit.
    Connections {
        target: backend
        function onStateChanged() {
            if (!page.dirty || (page.saved !== null && page.draft !== null && page.canon(page.saved) === page.canon(page.draft)))
                page.load()
        }
    }

    // An action slot: shows its action; click it, then pick in the card below.
    component SlotButton: Chip {
        property int index: 0
        objectName: "slot_" + index
        implicitWidth: Math.max(120, contentItem.implicitWidth + 24)
        implicitHeight: 40
        text: page.actionName(page.slotCode(index))
        on: page.slot === index
        onClicked: page.slot = page.slot === index ? -1 : index
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            // Nothing to show until the advanced keys have been read: no made-up values.
            visible: backend.ready && backend.advancedReady && backend.keymapReady
            enabled: backend.connected
            spacing: 20

            Item { implicitHeight: 12 }

            Label {
                Layout.leftMargin: 32
                text: qsTr("Advanced keys")
                font.pixelSize: 26
                font.bold: true
            }

            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Keys with an advanced function")

                KeyboardView {
                    objectName: "advancedKeyboard"
                    Layout.fillWidth: true
                    Layout.maximumWidth: implicitWidth
                    Layout.alignment: Qt.AlignHCenter
                    labelFor: function (id) {
                        const r = page.recordFor(id)
                        return r !== null ? page.typeTag(r.type) : page.keyName(id)
                    }
                    // No dot: the type's short name on the key already says it.
                    selectedKey: page.selectedKey
                    // A SOCD pair is edited from its first key, whichever of the two is clicked.
                    onKeyClicked: (id) => {
                        const r = page.recordFor(id)
                        page.selectedKey = r !== null && r.type === "socd" ? r.key : id
                    }
                }

                Label {
                    visible: backend.advancedKeys.length === 0
                    Layout.fillWidth: true
                    text: qsTr("None yet. Click a key on the drawing to give it one.")
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                }
                Flow {
                    objectName: "advancedList"
                    Layout.fillWidth: true
                    visible: backend.advancedKeys.length > 0
                    spacing: 6
                    Repeater {
                        model: backend.advancedKeys
                        delegate: Chip {
                            required property var modelData
                            on: page.selectedKey === modelData.key || (modelData.type === "socd" && page.selectedKey === modelData.key2)
                            text: page.typeName(modelData.type) + " · " + page.keyName(modelData.key)
                                  + (modelData.type === "socd" ? " + " + page.keyName(modelData.key2) : "")
                            onClicked: page.selectedKey = modelData.key
                        }
                    }
                }
            }

            // ---- selected key: type ----------------------------------------------
            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: page.selectedKey < 0 ? qsTr("Selected key") : qsTr("Selected key: %1").arg(page.keyName(page.selectedKey))

                Label {
                    visible: page.selectedKey < 0
                    text: qsTr("Click a key on the drawing.")
                    opacity: 0.7
                }
                Label {
                    visible: page.selectedKey >= 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: page.saved === null ? qsTr("No advanced function. Choose one:")
                                              : qsTr("Now: %1. Change it below, or choose another type:").arg(page.typeName(page.saved.type))
                    opacity: 0.7
                }
                Flow {
                    objectName: "typeChips"
                    visible: page.selectedKey >= 0
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: page.types
                        delegate: Chip {
                            required property string modelData
                            on: page.draft !== null && page.draft.type === modelData
                            text: page.typeName(modelData)
                            onClicked: page.chooseType(modelData)
                        }
                    }
                }
            }

            // ---- editor ---------------------------------------------------------
            Card {
                id: editor
                objectName: "advancedEditor"
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                visible: page.draft !== null
                title: page.draft !== null ? page.typeName(page.draft.type) : ""

                readonly property string type: page.draft !== null ? page.draft.type : ""

                Label {
                    Layout.fillWidth: true
                    text: page.typeHelp(editor.type)
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                }

                // Mod-Tap, Toggle, Release: one or two actions.
                GridLayout {
                    visible: editor.type === "modTap" || editor.type === "toggle" || editor.type === "release"
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 8
                    Label { text: page.slotName(0) }
                    SlotButton { index: 0 }
                    Label { visible: editor.type === "modTap"; text: page.slotName(1) }
                    SlotButton { visible: editor.type === "modTap"; index: 1 }
                }

                // Hold time: Mod-Tap and Toggle.
                ColumnLayout {
                    visible: editor.type === "modTap" || editor.type === "toggle"
                    Layout.fillWidth: true
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Hold time") }
                        Label {
                            objectName: "holdValue"
                            text: qsTr("%1 ms").arg(page.draft && page.draft.holdMs !== undefined ? page.draft.holdMs : 0)
                        }
                    }
                    Slider {
                        id: holdSlider
                        objectName: "holdSlider"
                        Layout.fillWidth: true
                        from: page.limits.minHoldMs
                        to: page.limits.maxHoldMs
                        // No stepSize: Material draws a tick per step. Values are rounded instead.
                        Binding on value {
                            when: !holdSlider.pressed
                            value: page.draft && page.draft.holdMs !== undefined ? page.draft.holdMs : page.limits.defaultHoldMs
                            restoreMode: Binding.RestoreNone
                        }
                        onMoved: page.edit(d => d.holdMs = Math.round(value))
                    }
                }

                // Multi-point: three actions, each with its depth.
                Repeater {
                    model: editor.type === "multiPoint" ? 3 : 0
                    delegate: GridLayout {
                        id: point
                        required property int index
                        Layout.fillWidth: true
                        columns: editor.width > 900 ? 2 : 1
                        columnSpacing: 32
                        RowLayout {
                            Layout.preferredWidth: 320
                            spacing: 12
                            Label { text: page.slotName(10 + point.index) }
                            SlotButton { index: 10 + point.index }
                            Button {
                                flat: true
                                text: qsTr("Clear")
                                visible: page.slotCode(10 + point.index) !== ""
                                onClicked: page.setSlot(10 + point.index, "")
                            }
                        }
                        MmSlider {
                            objectName: "pointDepth" + point.index
                            label: qsTr("Depth")
                            shown: page.draft.points[point.index].depth
                            from: page.limits.minDepth
                            to: page.limits.maxDepth
                            step: 0.01
                            decimals: 2
                            french: page.french
                            onCommit: (v) => page.edit(d => d.points[point.index].depth = v)
                        }
                    }
                }

                // DKS: the four points (two columns when there is room), then four rows of
                // action + when.
                GridLayout {
                    visible: editor.type === "dks"
                    Layout.fillWidth: true
                    columns: editor.width > 900 ? 2 : 1
                    columnSpacing: 32
                    rowSpacing: 8
                Repeater {
                    model: editor.type === "dks" ? 4 : 0
                    delegate: MmSlider {
                        required property int index
                        objectName: "dksDepth" + index
                        label: page.stageNames[index].charAt(0).toUpperCase() + page.stageNames[index].slice(1)
                        shown: page.draft.depths[index]
                        from: page.limits.minDepth
                        to: page.limits.maxDepth
                        step: 0.01
                        decimals: 2
                        french: page.french
                        onCommit: (v) => page.edit(d => d.depths[index] = v)
                    }
                }
                }
                Repeater {
                    model: editor.type === "dks" ? 4 : 0
                    delegate: Flow {
                        id: row
                        required property int index
                        readonly property var rowData: page.draft.rows[row.index]
                        readonly property bool custom: page.customRow(row.rowData)
                        readonly property var options: row.custom
                            ? page.strokes.concat([{ label: qsTr("Another pattern (kept as it is)"), from: -1, to: -1 }])
                            : page.strokes
                        Layout.fillWidth: true
                        spacing: 10
                        Label { text: page.slotName(20 + row.index); height: 40; verticalAlignment: Text.AlignVCenter }
                        SlotButton { index: 20 + row.index }
                        ComboBox {
                            objectName: "dksStroke" + row.index
                            width: Math.min(480, editor.width - 40)
                            enabled: row.rowData.action !== ""
                            model: row.options.map(o => o.label)
                            currentIndex: {
                                if (row.custom)
                                    return row.options.length - 1
                                for (let i = 0; i < row.options.length; ++i)
                                    if (row.options[i].from === row.rowData.from && row.options[i].to === row.rowData.to)
                                        return i
                                return 0
                            }
                            onActivated: (i) => {
                                const o = row.options[i]
                                if (row.custom && i === row.options.length - 1)
                                    return
                                page.edit(d => {
                                    d.rows[row.index].from = o.from
                                    d.rows[row.index].to = o.to
                                    if (o.from < 0) {
                                        d.rows[row.index].action = ""
                                        d.rows[row.index].stages = "00 00 00 00"
                                    }
                                })
                            }
                        }
                    }
                }

                // SOCD: the other key and the rule.
                GridLayout {
                    visible: editor.type === "socd"
                    Layout.fillWidth: true
                    columns: editor.width > 600 ? 2 : 1
                    columnSpacing: 16
                    rowSpacing: 8
                    Label { text: qsTr("Second key") }
                    ComboBox {
                        id: secondKey
                        objectName: "socdSecondKey"
                        Layout.fillWidth: true
                        Layout.maximumWidth: 240
                        readonly property var ids: backend.layout.map(k => k.id).filter(id => id !== (page.draft ? page.draft.key : -1))
                        model: secondKey.ids.map(id => page.keyName(id))
                        currentIndex: page.draft && page.draft.key2 !== undefined ? secondKey.ids.indexOf(page.draft.key2) : -1
                        displayText: currentIndex < 0 ? qsTr("Choose") : currentText
                        onActivated: (i) => page.edit(d => d.key2 = secondKey.ids[i])
                    }
                    Label { text: qsTr("When both are down") }
                    ComboBox {
                        id: mode
                        objectName: "socdMode"
                        Layout.fillWidth: true
                        Layout.maximumWidth: 320
                        readonly property var modes: ["lastInput", "neutral", "key1", "key2"]
                        readonly property string first: page.draft ? page.keyName(page.draft.key) : ""
                        readonly property string second: page.draft && page.draft.key2 >= 0 ? page.keyName(page.draft.key2) : qsTr("the second key")
                        model: [qsTr("The last one pressed wins"), qsTr("Neither: they cancel out"),
                                qsTr("%1 always wins").arg(mode.first), qsTr("%1 always wins").arg(mode.second)]
                        currentIndex: page.draft && page.draft.mode !== undefined ? mode.modes.indexOf(page.draft.mode) : 0
                        onActivated: (i) => page.edit(d => d.mode = mode.modes[i])
                    }
                }

                Label {
                    objectName: "advancedProblem"
                    Layout.fillWidth: true
                    visible: text !== "" && page.dirty
                    text: page.problem()
                    wrapMode: Text.WordWrap
                    color: theme.accent
                }
                Label {
                    Layout.fillWidth: true
                    visible: text !== "" && page.dirty
                    text: page.replaces()
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 12
                    Button {
                        objectName: "saveAdvanced"
                        text: qsTr("Save")
                        highlighted: true
                        enabled: page.dirty && page.problem() === ""
                        onClicked: page.save()
                    }
                    Button {
                        text: qsTr("Discard changes")
                        enabled: page.dirty
                        onClicked: page.load()
                    }
                    Button {
                        objectName: "removeAdvanced"
                        visible: page.saved !== null
                        text: page.saved !== null && page.saved.type === "socd" ? qsTr("Remove the SOCD pair") : qsTr("Remove")
                        onClicked: {
                            backend.removeAdvanced([page.selectedKey])
                            page.draft = null
                        }
                    }
                }
            }

            // ---- picker for the slot being filled -------------------------------
            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                visible: page.draft !== null && page.slot >= 0
                title: qsTr("%1: choose an action").arg(page.slotName(page.slot))

                ActionPicker {
                    Layout.fillWidth: true
                    offerNothing: false
                    currentCode: page.slotCode(page.slot)
                    onPicked: (code) => page.setSlot(page.slot, code)
                }
            }

            Item { implicitHeight: 12 }
        }
    }
}
