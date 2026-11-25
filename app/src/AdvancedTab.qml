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
            return "{" + Object.keys(o).filter(k => !(k === "stages" && (o.pattern >= 0 || o.stages === "00 00 00 00"))).sort()
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
                             rows: [0, 1, 2, 3].map(() => ({ action: "", pattern: 0 })) }
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
        // A slot the draft hasn't (the type changed while the picker was open) is empty.
        if (n === 0) return d.action ?? ""
        if (n === 1) return d.tap ?? ""
        if (n >= 20) return d.rows?.[n - 20]?.action ?? ""
        return d.points?.[n - 10]?.action ?? ""
    }
    function setSlot(n, code) {
        page.edit(d => {
            if (n === 0) {
                d.action = code
                delete d.own
            } else if (n === 1) d.tap = code
            else if (n >= 20) {
                d.rows[n - 20].action = code
                if (code !== "" && d.rows[n - 20].pattern <= 0 && !page.customRow(d.rows[n - 20]))
                    d.rows[n - 20].pattern = 1  // a new row fires once at the first point
                if (code === "")
                    d.rows[n - 20].pattern = 0
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
    // One label per row pattern (backend order: not used, single strokes, then several). A pattern
    // of several strokes names the points short, in time order, to fit the list's width:
    // "Once ↓1 ↓2, held ↑1→↑2" (↓1 = going down, 1st point).
    readonly property var pointNames: ["↓1", "↓2", "↑1", "↑2"]
    readonly property var patternLabels: page.limits.dksPatterns.map(p => {
        if (p.length === 0)
            return qsTr("Not used")
        if (p.length === 1) {
            const s = p[0]
            return s.from === s.to ? qsTr("Once, %1").arg(page.stageNames[s.from])
                                   : qsTr("Held, %1 → %2").arg(page.stageNames[s.from]).arg(page.stageNames[s.to])
        }
        let parts = []
        for (let i = 0; i < p.length; ++i) {
            if (p[i].from === p[i].to) {
                let taps = [page.pointNames[p[i].from]]
                while (i + 1 < p.length && p[i + 1].from === p[i + 1].to)
                    taps.push(page.pointNames[p[++i].from])
                parts.push((parts.length === 0 ? qsTr("Once %1") : qsTr("once %1")).arg(taps.join(" ")))
            } else {
                const hold = page.pointNames[p[i].from] + "→" + page.pointNames[p[i].to]
                parts.push((parts.length === 0 ? qsTr("Held %1") : qsTr("held %1")).arg(hold))
            }
        }
        return parts.join(", ")
    })
    function customRow(r) {
        return !(r.pattern >= 0) && r.action !== "" && r.stages !== undefined && r.stages !== "00 00 00 00"
    }

    // Whether every action the function needs is there: Save stays off until then (the empty
    // slots say "Choose…", no other hint).
    function complete() {
        const d = page.draft
        if (!d)
            return false
        switch (d.type) {
        case "modTap": return d.action !== "" && d.tap !== ""
        case "toggle":
        case "release": return d.action !== "" || d.own === true
        case "multiPoint": return d.points.some(p => p.action !== "")
        case "dks": return d.rows.some(r => r.action !== "")
        case "socd": return d.key2 >= 0
        }
        return false
    }
    // What is wrong with the values, in the user's words: Save is off while there is something.
    function problem() {
        const d = page.draft
        if (!d || !page.complete())
            return ""
        if (d.type === "dks" && (d.depths[0] >= d.depths[1] || d.depths[3] >= d.depths[2]))
            return qsTr("Each 2nd point going down, and each 1st point coming up, must be deeper than the other point.")
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
        if (page.dirty && page.complete() && page.problem() === "")
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

    // ---- layout ---------------------------------------------------------------
    // One column from the left edge, under the keyboard: the function, then its settings top to
    // bottom, then Save. Opening an action slot shows the picker to the right of the column; the
    // column stays where it is (in a window too narrow for both, the picker takes its place). The
    // keys that have a function are listed beside the keyboard (KeysPage).
    readonly property real rowHeight: 34 * ui.unit
    readonly property real controlsWidth: 470 * ui.unit
    readonly property real slotWidth: 150 * ui.unit
    readonly property bool picking: page.draft !== null && page.slot >= 0
    readonly property bool pickerBeside: page.width >= page.labelWidth + page.controlsWidth + 24 * ui.unit + 320 * ui.unit
    readonly property real labelWidth: Math.ceil(Math.max(...[qsTr("When held"), qsTr("When tapped"),
        qsTr("Hold time"), qsTr("Key action"), qsTr("On release"), qsTr("Point %1").arg(3), qsTr("Action %1").arg(4),
        qsTr("Second key"), qsTr("Both pressed"), qsTr("Going down"), qsTr("Coming up")].map(t => page.measure(t))))
        + 16 * ui.unit
    // DKS points, two to a row: "↓1", slider, value, "mm" each.
    readonly property real pointLabelWidth: Math.ceil(Math.max(...page.pointNames.map(t => page.measure(t)))) + 2
    readonly property real pointBoxWidth: 56 * ui.unit
    readonly property real pointSliderWidth: (page.controlsWidth - 16 * ui.unit) / 2 - page.pointLabelWidth
                                             - page.pointBoxWidth - 3 * 8 * ui.unit - page.measure("mm") - 2
    FontMetrics { id: metrics; font.pixelSize: ui.body }
    function measure(t) {
        return metrics.advanceWidth(t)
    }

    // An action slot: shows its action; click it, then pick to the right of the column.
    component SlotButton: Chip {
        property int slot: 0
        objectName: "slot_" + slot
        Layout.preferredWidth: page.slotWidth
        Layout.maximumHeight: page.rowHeight
        text: slot === 0 && page.draft && page.draft.own ? qsTr("Each key's own action") : page.actionName(page.slotCode(slot))
        on: page.slot === slot
        onClicked: page.slot = page.slot === slot ? -1 : slot
    }
    // A setting's name, in the label column.
    component RowLabel: Label {
        Layout.preferredWidth: page.labelWidth
        Layout.minimumHeight: page.rowHeight
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 24 * ui.unit

        ColumnLayout {
            id: editor
            objectName: "advancedEditor"
            visible: page.selectedKey >= 0 && (page.pickerBeside || !page.picking)
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: page.labelWidth + page.controlsWidth
            Layout.maximumWidth: page.labelWidth + page.controlsWidth
            spacing: 4 * ui.unit

            readonly property string type: page.draft !== null ? page.draft.type : ""

            // ---- the function -----------------------------------------------
            Label {
                text: qsTr("Advanced function")
                font.pixelSize: ui.section
                font.weight: Font.DemiBold
            }
            Flow {
                id: typeCard
                objectName: "typeChips"
                Layout.fillWidth: true
                Layout.topMargin: 4 * ui.unit
                Layout.bottomMargin: 8 * ui.unit
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

            // ---- Mod-Tap, Toggle, Release: the actions, then the hold time ----
            RowLayout {
                visible: editor.type === "modTap" || editor.type === "toggle" || editor.type === "release"
                spacing: 0
                RowLabel { text: page.slotName(0) }
                SlotButton { slot: 0 }
            }
            RowLayout {
                visible: editor.type === "modTap"
                spacing: 0
                RowLabel { text: page.slotName(1) }
                SlotButton { slot: 1 }
            }
            RowLayout {
                visible: editor.type === "modTap" || editor.type === "toggle"
                spacing: 0
                RowLabel { text: qsTr("Hold time") }
                Slider {
                    id: holdSlider
                    FocusRing {}
                    objectName: "holdSlider"
                    Layout.leftMargin: -leftPadding  // the track lines up with the action buttons
                    Layout.preferredWidth: 200 * ui.unit
                    Layout.maximumHeight: page.rowHeight
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
                Label {
                    objectName: "holdValue"
                    Layout.leftMargin: 8 * ui.unit
                    font.features: { "tnum": 1 }
                    text: qsTr("%1 ms").arg(holdSlider.pressed ? Math.round(holdSlider.value)
                                            : page.draft && page.draft.holdMs !== undefined ? page.draft.holdMs : 0)
                }
            }

            // ---- Multi-point: each point's action and depth, one row each -----
            Repeater {
                model: editor.type === "multiPoint" ? 3 : 0
                delegate: RowLayout {
                    required property int index
                    spacing: 0
                    RowLabel { text: page.slotName(10 + index) }
                    SlotButton { slot: 10 + index }
                    MmSlider {
                        objectName: "pointDepth" + index
                        Layout.fillWidth: false
                        labelWidth: 0
                        sliderWidth: 140 * ui.unit
                        shown: page.draft.points[index].depth
                        from: page.limits.minDepth
                        to: page.limits.maxDepth
                        step: 0.01
                        decimals: 2
                        french: page.french
                        onCommit: (v) => page.edit(d => d.points[index].depth = v)
                    }
                    AppButton {
                        kind: "quiet"
                        text: qsTr("Clear")
                        opacity: page.slotCode(10 + index) !== "" ? 1 : 0
                        enabled: page.slotCode(10 + index) !== ""
                        onClicked: page.setSlot(10 + index, "")
                    }
                }
            }

            // ---- Dynamic keystroke: the four points, then the four actions ----
            // The points two by two, named as the patterns name them (↓1 = going down, 1st point).
            Repeater {
                model: editor.type === "dks" ? 2 : 0
                delegate: RowLayout {
                    id: pair
                    required property int index
                    spacing: 0
                    RowLabel { text: pair.index === 0 ? qsTr("Going down") : qsTr("Coming up") }
                    Repeater {
                        model: 2
                        delegate: MmSlider {
                            required property int index
                            readonly property int point: 2 * pair.index + index
                            objectName: "dksDepth" + point
                            Layout.fillWidth: false
                            Layout.leftMargin: index === 1 ? 16 * ui.unit : 0
                            labelWidth: page.pointLabelWidth
                            boxWidth: page.pointBoxWidth
                            sliderWidth: page.pointSliderWidth
                            label: page.pointNames[point]
                            shown: page.draft.depths[point]
                            from: page.limits.minDepth
                            to: page.limits.maxDepth
                            step: 0.01
                            decimals: 2
                            french: page.french
                            onCommit: (v) => page.edit(d => d.depths[point] = v)
                        }
                    }
                }
            }
            Repeater {
                model: editor.type === "dks" ? 4 : 0
                delegate: RowLayout {
                    id: row
                    required property int index
                    readonly property var rowData: page.draft.rows[row.index]
                    readonly property bool custom: page.customRow(row.rowData)
                    readonly property var options: row.custom
                        ? page.patternLabels.concat([qsTr("Another pattern (kept as it is)")])
                        : page.patternLabels
                    Layout.topMargin: row.index === 0 ? 8 * ui.unit : 0
                    spacing: 0
                    RowLabel { text: page.slotName(20 + row.index) }
                    SlotButton { slot: 20 + row.index }
                    AppComboBox {
                        objectName: "dksStroke" + row.index
                        Layout.leftMargin: 8 * ui.unit
                        Layout.preferredWidth: page.controlsWidth - page.slotWidth - 8 * ui.unit
                        enabled: row.rowData.action !== ""
                        model: row.options
                        currentIndex: row.custom ? row.options.length - 1 : Math.max(0, row.rowData.pattern)
                        // The single strokes' names are long: the list opens half as wide again.
                        popup.width: Math.max(width, implicitWidth, 1.5 * width)
                        onActivated: (i) => {
                            if (row.custom && i === row.options.length - 1)
                                return
                            page.edit(d => {
                                d.rows[row.index].pattern = i
                                if (i === 0) {
                                    d.rows[row.index].action = ""
                                    d.rows[row.index].stages = "00 00 00 00"
                                }
                            })
                        }
                    }
                }
            }

            // ---- SOCD: the other key and the rule -----------------------------
            RowLayout {
                visible: editor.type === "socd" && page.selection.length !== 2
                spacing: 0
                RowLabel { text: qsTr("Second key") }
                AppComboBox {
                    id: secondKey
                    objectName: "socdSecondKey"
                    Layout.preferredWidth: page.slotWidth
                    readonly property var ids: backend.layout.map(k => k.id).filter(id => id !== (page.draft ? page.draft.key : -1))
                    model: secondKey.ids.map(id => page.keyName(id))
                    currentIndex: page.draft && page.draft.key2 !== undefined ? secondKey.ids.indexOf(page.draft.key2) : -1
                    displayText: currentIndex < 0 ? qsTr("Choose") : currentText
                    onActivated: (i) => page.edit(d => d.key2 = secondKey.ids[i])
                }
            }
            RowLayout {
                visible: editor.type === "socd"
                spacing: 0
                RowLabel { text: qsTr("Both pressed") }
                AppComboBox {
                    id: mode
                    objectName: "socdMode"
                    Layout.preferredWidth: page.controlsWidth - 60 * ui.unit
                    readonly property var modes: ["lastInput", "neutral", "key1", "key2"]
                    readonly property string first: page.draft ? page.keyName(page.draft.key) : ""
                    readonly property string second: page.draft && page.draft.key2 >= 0 ? page.keyName(page.draft.key2) : qsTr("the second key")
                    model: [qsTr("The last one pressed wins"), qsTr("Neither: they cancel out"),
                            qsTr("%1 always wins").arg(mode.first), qsTr("%1 always wins").arg(mode.second)]
                    currentIndex: page.draft && page.draft.mode !== undefined ? mode.modes.indexOf(page.draft.mode) : 0
                    onActivated: (i) => page.edit(d => d.mode = mode.modes[i])
                }
            }

            // ---- Save, at the bottom of the column ----------------------------
            RowLayout {
                visible: page.draft !== null || page.withRecords.length > 0
                Layout.topMargin: 12 * ui.unit
                spacing: 8 * ui.unit
                AppButton {
                    objectName: "saveAdvanced"
                    visible: page.draft !== null
                    kind: "primary"
                    text: qsTr("Save")
                    enabled: page.dirty && page.complete() && page.problem() === ""
                    onClicked: page.save()
                }
                AppButton {
                    visible: page.draft !== null
                    text: qsTr("Discard changes")
                    enabled: page.dirty
                    onClicked: page.load()
                }
                AppButton {
                    objectName: "removeAdvanced"
                    visible: page.withRecords.length > 0
                    text: page.saved !== null && page.saved.type === "socd" ? qsTr("Remove the SOCD pair") : qsTr("Remove")
                    onClicked: {
                        backend.removeAdvanced(page.withRecords)
                        page.draft = null
                    }
                }
            }
            // What is wrong with the values, and what saving takes away.
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

        // ---- the picker for the slot being filled, right of the column -------
        Card {
            visible: page.picking
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.maximumWidth: 640 * ui.unit
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
        Item { Layout.fillWidth: true; visible: !page.picking }
    }
    // Takes the room the column doesn't need, so it stays right under the keyboard.
    Item { Layout.fillHeight: true }
}
