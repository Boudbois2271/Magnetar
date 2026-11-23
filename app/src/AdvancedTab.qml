// SPDX-License-Identifier: GPL-3.0-or-later
// Keys page, Advanced tab: choose a type for the selected keys, fill in its actions and depths,
// save. Several keys get the same advanced function each (SOCD takes exactly two: the pair). The
// drawing and the list show what the keyboard read back; the editor holds a draft until Save.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: page
    spacing: 16 * ui.unit

    property var selection: []
    signal selectKeys(var ids)
    readonly property int selectedKey: page.selection.length > 0 ? page.selection[0] : -1  // the first one picked
    property var draft: null   // the record being edited, same shape as backend.advancedKeys
    property int slot: -1      // the action slot the picker fills: 0 action, 1 tap, 10+i point, 20+i DKS row
    readonly property var limits: backend.advancedLimits
    readonly property bool french: translator.language.indexOf("fr") === 0
    // The page's controls sit in three equal columns.
    readonly property real column: (width - 48 * ui.unit) / 3
    function span(n) { return n * column + (n - 1) * 24 * ui.unit }

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
    // The name on the cap: as short as a keycap can take.
    function typeTag(t) {
        switch (t) {
        case "modTap": return qsTr("Mod-Tap")
        case "toggle": return qsTr("Toggle")
        case "release": return qsTr("Release")
        case "multiPoint": return qsTr("Multi")
        case "dks": return "DKS"
        case "socd": return "SOCD"
        }
        return "?"
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
    function strip(r) {
        let c = page.copy(r)
        delete c.key
        return c
    }
    // What the selection has now, as one record: the key's own (a SOCD pair from either key, or
    // both), or the record every selected key has the same. null otherwise.
    function sharedRecord() {
        const keys = page.selection
        if (keys.length === 0)
            return null
        const first = page.recordFor(keys[0])
        if (keys.length === 1)
            return first
        if (first !== null && first.type === "socd" && keys.length === 2 && (keys[1] === first.key || keys[1] === first.key2))
            return first
        const recs = keys.map(page.recordFor)
        if (recs.some(r => r === null || r.type === "socd"))
            return null
        const c = page.canon(page.strip(recs[0]))
        if (!recs.every(r => page.canon(page.strip(r)) === c))
            return null
        let t = page.copy(recs[0])
        t.key = keys[0]
        return t
    }
    readonly property var saved: page.sharedRecord()
    readonly property var withRecords: page.selection.filter(id => page.recordFor(id) !== null)
    readonly property bool multi: page.selection.length > 1 && !(page.saved !== null && page.saved.type === "socd")

    function labelFor(id) {
        return page.keyName(id)
    }
    function badgeFor(id) {
        const r = page.recordFor(id)
        return r !== null ? page.typeTag(r.type) : ""
    }
    readonly property var markedFor: function (id) { return false }  // the badge says it
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
            if (page.selection.length > 1)  // each key toggles its own action
                return { type: type, key: key, action: "", own: true, holdMs: page.limits.defaultHoldMs }
            const layer = backend.keymapReady ? backend.keymap[0] : ({})
            const own = layer[String(key)]
            return { type: type, key: key, action: own ? own.code : "", holdMs: page.limits.defaultHoldMs }
        }
        case "release": return { type: type, key: key, action: "" }
        case "multiPoint": return { type: type, key: key,
                                    points: page.limits.pointDepths.map(d => ({ action: "", depth: d })) }
        case "dks": return { type: type, key: key, depths: page.limits.dksDepths.slice(),
                             rows: [0, 1, 2, 3].map(() => ({ action: "", from: -1, to: -1 })) }
        case "socd": return { type: type, key: key, key2: page.selection.length === 2 ? page.selection[1] : -1, mode: "lastInput" }
        }
        return null
    }
    function load() {
        // Not page.saved: this runs from onSelectionChanged, maybe before that binding updates.
        page.draft = page.copy(page.sharedRecord())
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
            if (n === 0) {
                d.action = code
                delete d.own
            } else if (n === 1) d.tap = code
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
            if (d.action === "" && !d.own)
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
        return page.records().every(r => backend.advancedValid(r)) ? "" : qsTr("Some values are out of range.")
    }
    // The records Save sends: the draft on each selected key (a SOCD draft is already the pair).
    function records() {
        const d = page.draft
        if (!d)
            return []
        if (d.type === "socd")
            return [d]
        const layer = backend.keymapReady && backend.keymap.length > 0 ? backend.keymap[0] : ({})
        return page.selection.map(id => {
            let r = page.copy(d)
            r.key = id
            if (r.own) {
                const own = layer[String(id)]
                r.action = own ? own.code : ""
                delete r.own
            }
            return r
        })
    }
    // What saving takes away besides this key's own record.
    function replaces() {
        const d = page.draft
        if (!d)
            return ""
        if (page.selection.length > 1 && (page.saved === null || page.saved.type !== "socd")) {
            if (page.saved !== null || page.withRecords.length === 0)
                return ""
            return qsTr("Saving replaces the advanced function of %1 of these keys.").arg(page.withRecords.length)
        }
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
            backend.saveAdvanced(page.records())
    }

    onSelectionChanged: page.load()
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
        property int slot: 0
        objectName: "slot_" + slot
        implicitWidth: Math.max(120 * ui.unit, contentItem.implicitWidth + 24 * ui.unit)
        text: slot === 0 && page.draft && page.draft.own ? qsTr("Each key's own action") : page.actionName(page.slotCode(slot))
        on: page.slot === slot
        onClicked: page.slot = page.slot === slot ? -1 : slot
    }

    // Three columns: the function types and the keys that have one | the editor (two columns).
    // While an action slot is being filled, the picker takes the first column.
    GridLayout {
        id: grid
        Layout.fillWidth: true
        Layout.fillHeight: true
        columns: 3
        columnSpacing: 24 * ui.unit
        rowSpacing: 16 * ui.unit

        ColumnLayout {
            Layout.row: 0
            Layout.column: 0
            Layout.preferredWidth: page.span(1)
            Layout.maximumWidth: page.span(1)
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignTop
            visible: !(page.draft !== null && page.slot >= 0)
            spacing: 16 * ui.unit

            // ---- selected keys: type -------------------------------------------
            Card {
                id: typeCard
                visible: page.selectedKey >= 0
                Layout.alignment: Qt.AlignTop
                title: qsTr("Advanced function")

                Flow {
                    objectName: "typeChips"
                    Layout.fillWidth: true
                    spacing: 6 * ui.unit
                    Repeater {
                        model: page.types
                        delegate: Chip {
                            required property string modelData
                            objectName: "type_" + modelData
                            on: page.draft !== null && page.draft.type === modelData
                            // A SOCD pair is two keys: one (the other chosen below) or exactly two selected.
                            enabled: modelData !== "socd" || page.selection.length <= 2
                            text: page.typeName(modelData)
                            onClicked: page.chooseType(modelData)
                        }
                    }
                }
            }

            // ---- every key that has one: a click selects it ----------------------
            Card {
                visible: backend.advancedKeys.length > 0
                Layout.alignment: Qt.AlignTop
                Layout.fillHeight: true
                Layout.maximumHeight: implicitHeight
                title: qsTr("Keys with an advanced function")

                ScrollView {
                    objectName: "advancedListBox"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: list.implicitHeight
                    Layout.minimumHeight: Math.min(list.implicitHeight, 34 * ui.unit)
                    contentWidth: availableWidth

                    Flow {
                        id: list
                        objectName: "advancedList"
                        width: parent.width
                        spacing: 6 * ui.unit
                        Repeater {
                            model: backend.advancedKeys
                            delegate: Chip {
                                required property var modelData
                                on: page.selection.indexOf(modelData.key) >= 0
                                text: page.typeName(modelData.type) + " · " + page.keyName(modelData.key)
                                      + (modelData.type === "socd" ? " + " + page.keyName(modelData.key2) : "")
                                onClicked: page.selectKeys(modelData.type === "socd" ? [modelData.key, modelData.key2] : [modelData.key])
                            }
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }

        // ---- picker for the slot being filled -------------------------------
        Card {
            visible: page.draft !== null && page.slot >= 0
            Layout.row: 0
            Layout.column: 0
            Layout.preferredWidth: page.span(1)
            Layout.maximumWidth: page.span(1)
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true
            Layout.maximumHeight: implicitHeight
            title: qsTr("%1: choose an action").arg(page.slotName(page.slot))

            ActionPicker {
                id: picker
                Layout.fillWidth: true
                Layout.fillHeight: true
                offerNothing: false
                compact: true
                currentCode: page.slotCode(page.slot) || ""
                onPicked: (code) => page.setSlot(page.slot, code)
            }
        }

        // ---- editor ---------------------------------------------------------
        ColumnLayout {
            id: editor
            objectName: "advancedEditor"
            visible: page.draft !== null
            Layout.row: 0
            Layout.column: 1
            Layout.columnSpan: 2
            Layout.preferredWidth: page.span(2)
            Layout.maximumWidth: page.span(2)
            Layout.alignment: Qt.AlignTop
            spacing: 12 * ui.unit

            readonly property string type: page.draft !== null ? page.draft.type : ""
            readonly property real half: (page.span(2) - 24 * ui.unit) / 2

            // Mod-Tap, Toggle, Release: one or two actions; hold time beside them.
            GridLayout {
                visible: editor.type === "modTap" || editor.type === "toggle" || editor.type === "release"
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 24 * ui.unit

                GridLayout {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: editor.half
                    Layout.maximumWidth: editor.half
                    columns: 2
                    columnSpacing: 16 * ui.unit
                    rowSpacing: 8 * ui.unit
                    Label { text: page.slotName(0) }
                    SlotButton { slot: 0 }
                    Label { visible: editor.type === "modTap"; text: page.slotName(1) }
                    SlotButton { visible: editor.type === "modTap"; slot: 1 }
                }

                // Hold time: Mod-Tap and Toggle.
                ColumnLayout {
                    visible: editor.type === "modTap" || editor.type === "toggle"
                    Layout.preferredWidth: editor.half
                    Layout.maximumWidth: editor.half
                    Layout.alignment: Qt.AlignTop
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Hold time") }
                        Label {
                            objectName: "holdValue"
                            font.features: { "tnum": 1 }
                            text: qsTr("%1 ms").arg(page.draft && page.draft.holdMs !== undefined ? page.draft.holdMs : 0)
                        }
                    }
                    Slider {
                        id: holdSlider
                        FocusRing {}
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
            }

            // Multi-point: three actions, each with its depth, one row each.
            GridLayout {
                visible: editor.type === "multiPoint"
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 12 * ui.unit
                rowSpacing: 8 * ui.unit
                Repeater {
                    model: editor.type === "multiPoint" ? 3 : 0
                    delegate: Label {
                        required property int index
                        Layout.row: index
                        Layout.column: 0
                        text: page.slotName(10 + index)
                    }
                }
                Repeater {
                    model: editor.type === "multiPoint" ? 3 : 0
                    delegate: SlotButton {
                        required property int index
                        Layout.row: index
                        Layout.column: 1
                        slot: 10 + index
                    }
                }
                Repeater {
                    model: editor.type === "multiPoint" ? 3 : 0
                    delegate: AppButton {
                        required property int index
                        Layout.row: index
                        Layout.column: 2
                        kind: "quiet"
                        text: qsTr("Clear")
                        opacity: page.slotCode(10 + index) !== "" ? 1 : 0
                        enabled: page.slotCode(10 + index) !== ""
                        onClicked: page.setSlot(10 + index, "")
                    }
                }
                Repeater {
                    model: editor.type === "multiPoint" ? 3 : 0
                    delegate: MmSlider {
                        required property int index
                        Layout.row: index
                        Layout.column: 3
                        objectName: "pointDepth" + index
                        labelWidth: 90 * ui.unit
                        label: qsTr("Depth")
                        shown: page.draft.points[index].depth
                        from: page.limits.minDepth
                        to: page.limits.maxDepth
                        step: 0.01
                        decimals: 2
                        french: page.french
                        onCommit: (v) => page.edit(d => d.points[index].depth = v)
                    }
                }
            }

            // DKS: the four points beside the four rows of action + when.
            GridLayout {
                visible: editor.type === "dks"
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 24 * ui.unit

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: editor.half
                    Layout.maximumWidth: editor.half
                    spacing: 8 * ui.unit
                    Repeater {
                        model: editor.type === "dks" ? 4 : 0
                        delegate: MmSlider {
                            required property int index
                            objectName: "dksDepth" + index
                            labelWidth: 150 * ui.unit
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
                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: editor.half
                    Layout.maximumWidth: editor.half
                    spacing: 8 * ui.unit
                    Repeater {
                        model: editor.type === "dks" ? 4 : 0
                        delegate: RowLayout {
                            id: row
                            required property int index
                            readonly property var rowData: page.draft.rows[row.index]
                            readonly property bool custom: page.customRow(row.rowData)
                            readonly property var options: row.custom
                                ? page.strokes.concat([{ label: qsTr("Another pattern (kept as it is)"), from: -1, to: -1 }])
                                : page.strokes
                            Layout.fillWidth: true
                            spacing: 10 * ui.unit
                            SlotButton { slot: 20 + row.index }
                            AppComboBox {
                                objectName: "dksStroke" + row.index
                                Layout.fillWidth: true
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
                }
            }

            // SOCD: the other key and the rule.
            GridLayout {
                visible: editor.type === "socd"
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 16 * ui.unit
                rowSpacing: 8 * ui.unit
                Label { visible: secondKey.visible; text: qsTr("Second key") }
                AppComboBox {
                    id: secondKey
                    objectName: "socdSecondKey"
                    visible: page.selection.length !== 2
                    Layout.fillWidth: true
                    Layout.maximumWidth: 240 * ui.unit
                    readonly property var ids: backend.layout.map(k => k.id).filter(id => id !== (page.draft ? page.draft.key : -1))
                    model: secondKey.ids.map(id => page.keyName(id))
                    currentIndex: page.draft && page.draft.key2 !== undefined ? secondKey.ids.indexOf(page.draft.key2) : -1
                    displayText: currentIndex < 0 ? qsTr("Choose") : currentText
                    onActivated: (i) => page.edit(d => d.key2 = secondKey.ids[i])
                }
                Label { text: qsTr("When both are down") }
                AppComboBox {
                    id: mode
                    objectName: "socdMode"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 320 * ui.unit
                    readonly property var modes: ["lastInput", "neutral", "key1", "key2"]
                    readonly property string first: page.draft ? page.keyName(page.draft.key) : ""
                    readonly property string second: page.draft && page.draft.key2 >= 0 ? page.keyName(page.draft.key2) : qsTr("the second key")
                    model: [qsTr("The last one pressed wins"), qsTr("Neither: they cancel out"),
                            qsTr("%1 always wins").arg(mode.first), qsTr("%1 always wins").arg(mode.second)]
                    currentIndex: page.draft && page.draft.mode !== undefined ? mode.modes.indexOf(page.draft.mode) : 0
                    onActivated: (i) => page.edit(d => d.mode = mode.modes[i])
                }
            }


            RowLayout {
                Layout.fillWidth: true
                spacing: 12 * ui.unit
                AppButton {
                    objectName: "saveAdvanced"
                    Layout.alignment: Qt.AlignTop
                    kind: "primary"
                    text: qsTr("Save")
                    enabled: page.dirty && page.problem() === ""
                    onClicked: page.save()
                }
                AppButton {
                    Layout.alignment: Qt.AlignTop
                    text: qsTr("Discard changes")
                    enabled: page.dirty
                    onClicked: page.load()
                }
                AppButton {
                    objectName: "removeAdvanced"
                    Layout.alignment: Qt.AlignTop
                    visible: page.withRecords.length > 0
                    text: page.saved !== null && page.saved.type === "socd" ? qsTr("Remove the SOCD pair") : qsTr("Remove")
                    onClicked: {
                        backend.removeAdvanced(page.withRecords)
                        page.draft = null
                    }
                }
                // Why Save is off, and what saving takes away.
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 4 * ui.unit
                    spacing: 2 * ui.unit
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
                        color: theme.secondary
                    }
                }
            }
        }
    }
}
