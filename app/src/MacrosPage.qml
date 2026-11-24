// SPDX-License-Identifier: GPL-3.0-or-later
// Macros of the active profile: the list with the storage they use, and, for a selected or new
// macro, an editor that records key presses and releases with their delays. The list shows what the board read back; the
// editor's draft follows the read-back until it is changed.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: page

    property int index: -1             // the macro being edited, -1 = a new one
    property bool editing: false       // the editor is open (a macro selected, or a new one)
    property var draft: page.blank()   // {name, steps: [{press, usage, delay}]}
    property bool standard: false      // every delay is the standard one
    property bool recording: false
    property double lastEvent: -1      // ms timestamp of the last recorded step, -1 before the first
    property var held: []              // usages pressed during this recording
    property bool selectNew: false     // select the macro a save adds, once it is read back
    property string loadedFrom: "null" // the read-back the draft was loaded from
    property var newDraft: null        // an unsaved new macro, kept while its form is closed

    KeyNames { id: names }

    readonly property var limits: backend.macroLimits
    readonly property var macros: backend.macros
    readonly property var saved: page.index >= 0 && page.index < page.macros.length ? page.macros[page.index] : null

    // What Save sends: standard delays applied, and 1 ms on the last step, as the board keeps it.
    readonly property var record: {
        const n = page.draft.steps.length
        return { name: page.draft.name, steps: page.draft.steps.map((s, i) => ({
            press: s.press, usage: s.usage,
            delay: i === n - 1 ? 1 : page.standard ? page.limits.standardDelay : s.delay })) }
    }
    // The steps as listed: newest first while recording, so each new one shows at the top; the
    // draft (and the board) keep them in time order. The list fades out and back in around the
    // switch, so the rows don't jump.
    property bool newestFirst: false
    readonly property var shownSteps: page.newestFirst ? page.draft.steps.slice().reverse() : page.draft.steps
    function stepAt(row) {
        return page.newestFirst ? page.draft.steps.length - 1 - row : row
    }
    onRecordingChanged: {
        if (page.newestFirst === page.recording)
            return
        if (page.draft.steps.length < 2) {
            page.newestFirst = page.recording
            return
        }
        resort.restart()
    }
    SequentialAnimation {
        id: resort
        NumberAnimation { target: stepsView; property: "opacity"; to: 0; duration: 150; easing.type: Easing.InQuad }
        ScriptAction { script: page.newestFirst = page.recording }
        NumberAnimation { target: stepsView; property: "opacity"; to: 1; duration: 180; easing.type: Easing.OutQuad }
    }
    readonly property bool dirty: page.key(page.record) !== page.key(page.saved || page.blank())
    // Compares macros by content (the board's records list their fields in another order).
    function key(m) {
        return JSON.stringify([m.name, m.steps.map(s => [s.press, s.usage, s.delay])])
    }
    // Bytes in the board's storage: 4 for the directory entry, then name length, name, 4 per step.
    function sizeOf(m) {
        return 4 + 1 + m.name.length + 4 * m.steps.length
    }
    readonly property int usedAfterSave: backend.macroStorage.used - (page.saved ? page.saved.size + 4 : 0) + page.sizeOf(page.record)
    readonly property bool fits: page.usedAfterSave <= backend.macroStorage.max
    readonly property bool canSave: page.dirty && !page.recording && page.fits && backend.macroValid(page.record)

    // Keys of the active profile bound to macro i, on any layer.
    function boundKeys(i) {
        let n = 0
        for (const layer of backend.keymap)
            for (const id in layer)
                if (layer[id].kind === "macro" && layer[id].value === i)
                    ++n
        return n
    }

    function blank() {
        return { name: "", steps: [] }
    }
    function copy(m) {
        return { name: m.name, steps: m.steps.map(s => ({ press: s.press, usage: s.usage, delay: s.delay })) }
    }
    function load() {
        page.recording = false
        // Another macro shows at once, in time order.
        resort.stop()
        stepsView.opacity = 1
        page.newestFirst = false
        page.loadedFrom = JSON.stringify(page.saved)
        page.draft = page.saved ? page.copy(page.saved) : page.blank()
        const steps = page.draft.steps
        page.standard = steps.length > 1 && steps.slice(0, -1).every(s => s.delay === page.limits.standardDelay)
    }
    function select(i) {
        page.keepNewDraft()
        page.index = i
        page.editing = true
        page.load()
        // Reopening the new macro form brings back what was left in it.
        if (i < 0 && page.newDraft) {
            page.draft = page.copy(page.newDraft.draft)
            page.standard = page.newDraft.standard
        }
    }
    // Back to no macro selected: the editor closes.
    function close() {
        page.keepNewDraft()
        page.index = -1
        page.editing = false
        page.load()
    }
    // "New macro" opens the form, and closes it when it is already open.
    function toggleNew() {
        if (page.editing && page.index < 0)
            page.close()
        else
            page.select(-1)
    }
    // Leaving the new macro form keeps its unsaved draft for the next time it opens: nothing typed
    // or recorded is lost, and nothing asks.
    function keepNewDraft() {
        if (page.editing && page.index < 0)
            page.newDraft = page.dirty ? { draft: page.copy(page.draft), standard: page.standard } : null
    }
    function setSteps(steps) {
        page.draft = { name: page.draft.name, steps: steps }
    }
    function setName(name) {
        page.draft = { name: name, steps: page.draft.steps }
    }
    function setStep(i, change) {
        let steps = page.draft.steps.slice()
        steps[i] = Object.assign({}, steps[i], change)
        page.setSteps(steps)
    }
    function moveStep(i, by) {
        let steps = page.draft.steps.slice()
        const s = steps.splice(i, 1)[0]
        steps.splice(i + by, 0, s)
        page.setSteps(steps)
    }
    // Moves the row shown at `from` so it is shown at `to` (rows in the order on screen).
    function moveRow(from, to) {
        let rows = page.shownSteps.slice()
        const s = rows.splice(from, 1)[0]
        rows.splice(to, 0, s)
        page.setSteps(page.newestFirst ? rows.reverse() : rows)
    }

    // ---- dragging a step -------------------------------------------------------
    // Positions are scene points, read in the steps' own coordinates, so they stay right while
    // the list scrolls under the pointer.
    property int dragFrom: -1          // row being dragged, -1 = none
    property real dragStart: 0         // pointer y in the steps when the drag started
    property real dragY: 0             // how far the dragged row has moved
    property int dropGap: -1           // gap it would land in: 0 = above the first row
    property point dragScene
    readonly property bool dropShown: page.dragFrom >= 0 && page.dropGap >= 0
                                      && page.dropGap !== page.dragFrom && page.dropGap !== page.dragFrom + 1
    // The row it would land on, and the distance from one row to the next.
    readonly property int dropTarget: page.dropGap > page.dragFrom ? page.dropGap - 1 : page.dropGap
    readonly property real dragPitch: stepRows.count > 1 && stepRows.itemAt(1) ? stepRows.itemAt(1).y - stepRows.itemAt(0).y : 0
    function startDrag(row, sceneX, sceneY) {
        page.dragFrom = row
        page.dragStart = steps.mapFromItem(null, sceneX, sceneY).y
        page.dragTo(sceneX, sceneY)
    }
    function dragTo(sceneX, sceneY) {
        page.dragScene = Qt.point(sceneX, sceneY)
        const y = steps.mapFromItem(null, sceneX, sceneY).y
        page.dragY = y - page.dragStart
        let gap = 0
        for (let i = 0; i < stepRows.count; ++i) {
            const r = stepRows.itemAt(i)
            if (r && r.y + r.height / 2 < y)
                gap = i + 1
        }
        page.dropGap = gap
    }
    function endDrag(drop) {
        const from = page.dragFrom, gap = page.dropGap, move = drop && page.dropShown
        page.dragFrom = -1
        page.dragY = 0
        page.dropGap = -1
        if (move)
            page.moveRow(from, gap > from ? gap - 1 : gap)
    }
    // Near the top or bottom of the steps box, the list scrolls under the dragged row.
    Timer {
        interval: 16
        repeat: true
        running: page.dragFrom >= 0
        onTriggered: {
            const flick = stepsView.contentItem
            const v = stepsView.mapFromItem(null, page.dragScene.x, page.dragScene.y).y
            const edge = 32 * ui.unit, most = 14 * ui.unit
            const by = v < edge ? -most * Math.min(1, (edge - v) / edge)
                     : v > stepsView.height - edge ? most * Math.min(1, (v - stepsView.height + edge) / edge) : 0
            const y = Math.max(0, Math.min(flick.contentHeight - flick.height, flick.contentY + by))
            if (y !== flick.contentY) {
                flick.contentY = y
                page.dragTo(page.dragScene.x, page.dragScene.y)
            }
        }
    }

    function removeStep(i) {
        let steps = page.draft.steps.slice()
        steps.splice(i, 1)
        page.setSteps(steps)
    }
    // Removes every step, the name stays. While recording, the take starts over.
    function clearSteps() {
        page.setSteps([])
        page.held = []
        page.lastEvent = -1
    }
    function addKey(usage) {
        const d = page.limits.standardDelay
        page.setSteps(page.draft.steps.concat([{ press: true, usage: usage, delay: d },
                                               { press: false, usage: usage, delay: d }]))
    }

    // ---- recording -----------------------------------------------------------
    // Recording adds to the steps already there (only Clear removes them); each recorded step's
    // delay is the time until the next one, and the step before the take waits the standard delay.
    function startRecording() {
        page.held = []
        page.lastEvent = -1
        page.editing = true
        page.recording = true
        recorder.forceActiveFocus()
    }
    function stopRecording() {
        page.recording = false
    }
    function recordEvent(press, scanCode, now) {
        const usage = backend.usageForScanCode(scanCode)
        if (!page.recording || usage === 0)
            return
        // A release whose press came before Record isn't part of the macro.
        if (!press && page.held.indexOf(usage) < 0)
            return
        page.held = press ? page.held.concat([usage]) : page.held.filter(u => u !== usage)
        let steps = page.draft.steps.slice()
        if (steps.length > 0)
            steps[steps.length - 1] = Object.assign({}, steps[steps.length - 1], {
                delay: page.lastEvent < 0 ? page.limits.standardDelay
                       : Math.max(0, Math.min(page.limits.maxDelay, Math.round(now - page.lastEvent))) })
        steps.push({ press: press, usage: usage, delay: page.limits.standardDelay })
        page.lastEvent = now
        page.setSteps(steps)
        // The newest step is the top row: keep it in view.
        stepsView.contentItem.contentY = 0
    }

    function save() {
        if (!page.canSave)
            return
        page.selectNew = page.index < 0
        backend.saveMacro(page.index, page.record)
    }
    function remove() {
        if (page.index < 0)
            return
        backend.deleteMacro(page.index)
        page.close()
    }

    // A new macro, once saved, is selected; when the board's copy of the edited macro changes
    // (saved, another profile), the editor shows it. Plain re-reads keep the draft.
    onMacrosChanged: {
        if (page.selectNew && page.macros.length > 0) {
            page.selectNew = false
            page.select(page.macros.length - 1)
            page.newDraft = null  // it was saved
        } else if (page.index >= page.macros.length) {
            page.close()
        } else if (JSON.stringify(page.saved) !== page.loadedFrom) {
            page.load()
        }
    }

    // Takes the keys while recording, shortcuts included, so Esc, Space or Q can be recorded.
    Item {
        id: recorder
        focus: page.recording
        Keys.onShortcutOverride: (event) => event.accepted = page.recording
        Keys.onPressed: (event) => {
            if (page.recording) {
                if (!event.isAutoRepeat)
                    page.recordEvent(true, event.nativeScanCode, Date.now())
                event.accepted = true
            }
        }
        Keys.onReleased: (event) => {
            if (page.recording) {
                if (!event.isAutoRepeat)
                    page.recordEvent(false, event.nativeScanCode, Date.now())
                event.accepted = true
            }
        }
    }

    // An AppButton with a Lucide icon before its text.
    component IconButton: AppButton {
        id: iconButton
        property string iconName
        implicitWidth: iconRow.implicitWidth + 24 * ui.unit
        contentItem: Item {
            Row {
                id: iconRow
                anchors.centerIn: parent
                spacing: 8 * ui.unit
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: iconButton.iconName
                    color: iconLabel.color
                }
                Label {
                    id: iconLabel
                    anchors.verticalCenter: parent.verticalCenter
                    text: iconButton.text
                    color: iconButton.primary ? theme.accentForeground : theme.foreground
                }
            }
        }
    }

    PageLayout {
        title: qsTr("Macros")
        // Nothing to show until the macros have been read: no made-up values.
        visible: backend.macrosReady
        enabled: backend.connected

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 32 * ui.unit

            // ---- list and storage ------------------------------------------
            ColumnLayout {
                Layout.fillWidth: false
                Layout.preferredWidth: 300 * ui.unit
                Layout.fillHeight: true
                spacing: 10 * ui.unit

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Macros of profile %1").arg(backend.profile)
                    elide: Text.ElideRight
                    font.pixelSize: ui.section
                    font.weight: Font.DemiBold
                }

                // The list grows with the macros and scrolls in its box; "New macro" stays at
                // the bottom of it.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: theme.radius
                    color: theme.card

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6 * ui.unit
                        spacing: 6 * ui.unit

                        ListView {
                            objectName: "macroList"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 2 * ui.unit
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            model: page.macros
                            delegate: AbstractButton {
                                id: macroRow
                                required property int index
                                required property var modelData
                                readonly property bool current: page.editing && page.index === index
                                readonly property color ink: current ? theme.accentForeground : theme.foreground
                                objectName: "macro_" + index
                                width: ListView.view.width
                                implicitHeight: 34 * ui.unit
                                hoverEnabled: true
                                leftPadding: 12 * ui.unit
                                rightPadding: 12 * ui.unit
                                background: Rectangle {
                                    radius: theme.radius
                                    color: macroRow.current ? theme.accent
                                           : macroRow.down || macroRow.hovered ? theme.chipHover : "transparent"
                                }
                                contentItem: RowLayout {
                                    spacing: 12 * ui.unit
                                    Label {
                                        Layout.fillWidth: true
                                        text: macroRow.modelData.name
                                        color: macroRow.ink
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: macroRow.modelData.steps.length === 1 ? qsTr("1 step")
                                              : qsTr("%1 steps").arg(macroRow.modelData.steps.length)
                                        color: macroRow.current ? macroRow.ink : theme.secondary
                                        font.pixelSize: ui.small
                                        font.features: { "tnum": 1 }
                                    }
                                }
                                onClicked: page.select(index)
                                FocusRing { border.color: macroRow.current ? theme.accentForeground : theme.accent }
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                            }
                        }

                        IconButton {
                            objectName: "newMacro"
                            kind: page.editing && page.index < 0 ? "primary" : "quiet"
                            iconName: "plus"
                            text: qsTr("New macro")
                            onClicked: page.toggleNew()
                        }
                    }
                }

                ProgressBar {
                    Layout.fillWidth: true
                    from: 0
                    to: backend.macroStorage.max
                    value: backend.macroStorage.used
                }
                Label {
                    objectName: "macroStorageLabel"
                    text: qsTr("%1 / %2 bytes used").arg(backend.macroStorage.used).arg(backend.macroStorage.max)
                    color: theme.secondary
                    font.pixelSize: ui.small
                    font.features: { "tnum": 1 }
                }
            }

            // ---- editor ----------------------------------------------------
            // Only for a selected or new macro. Takes the height left, up to what it needs; the
            // steps scroll inside it.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 640 * ui.unit
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop
                visible: page.editing
                spacing: 16 * ui.unit

                Label {
                    Layout.fillWidth: true
                    text: page.saved ? qsTr("Edit macro %1").arg(page.index + 1) : qsTr("New macro")
                    elide: Text.ElideRight
                    font.pixelSize: ui.section
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12 * ui.unit
                    Label { text: qsTr("Name") }
                    AppTextField {
                        objectName: "macroName"
                        Layout.preferredWidth: 260 * ui.unit
                        placeholderText: qsTr("At least %1 characters").arg(page.limits.minName)
                        text: page.draft.name
                        maximumLength: page.limits.maxName
                        // The board stores names as plain ASCII.
                        validator: RegularExpressionValidator { regularExpression: /[\x20-\x7e]*/ }
                        onTextEdited: page.setName(text)
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16 * ui.unit
                    IconButton {
                        objectName: "recordButton"
                        kind: page.recording ? "primary" : "secondary"
                        iconName: page.recording ? "square" : "circle"
                        text: page.recording ? qsTr("Stop") : qsTr("Record")
                        onClicked: page.recording ? page.stopRecording() : page.startRecording()
                    }
                    AppSwitch {
                        objectName: "standardDelay"
                        text: qsTr("Standard delay (%1 ms)").arg(page.limits.standardDelay)
                        checked: page.standard
                        onToggled: page.standard = checked
                    }
                    Item { Layout.fillWidth: true }
                }

                // Adds a press and a release of one key, for editing by hand. Above the steps, so it stays put
                // as the list grows.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10 * ui.unit
                    AppComboBox {
                        id: addKeyBox
                        Layout.preferredWidth: 200 * ui.unit
                        model: page.addableKeys.map(u => names.usageName(u))
                        currentIndex: -1
                        displayText: currentIndex < 0 ? qsTr("Add a key…") : currentText
                    }
                    AppButton {
                        text: qsTr("Add")
                        enabled: addKeyBox.currentIndex >= 0 && !page.recording
                        onClicked: page.addKey(page.addableKeys[addKeyBox.currentIndex])
                    }
                    Item { Layout.fillWidth: true }
                }

                // One row per step: what happens, then the wait before the next step. A long
                // macro scrolls in this box; the rest of the page stays put.
                Rectangle {
                    id: stepsBox
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: steps.implicitHeight + 2 * stepsBox.pad
                    Layout.maximumHeight: steps.implicitHeight + 2 * stepsBox.pad
                    Layout.minimumHeight: Math.min(steps.implicitHeight, 80 * ui.unit) + 2 * stepsBox.pad
                    readonly property real pad: 6 * ui.unit
                    visible: page.draft.steps.length > 0
                    radius: theme.radius
                    color: theme.card

                    ScrollView {
                        id: stepsView
                        objectName: "macroSteps"
                        anchors.fill: parent
                        anchors.margins: stepsBox.pad
                        contentWidth: availableWidth

                        ColumnLayout {
                            id: steps
                            width: parent.width
                            spacing: 2 * ui.unit
                            Repeater {
                                id: stepRows
                                objectName: "macroStepRows"
                                model: page.shownSteps
                                delegate: Item {
                                    id: stepRow
                                    // index = row on screen; step = its place in the macro.
                                    required property int index
                                    required property var modelData
                                    readonly property int step: page.stepAt(index)
                                    readonly property bool last: step === page.draft.steps.length - 1
                                    readonly property bool dragged: page.dragFrom === index
                                    Layout.fillWidth: true
                                    implicitHeight: stepLine.implicitHeight
                                    // While a row is dragged it follows the pointer above the
                                    // others, and the rows between its place and where it would
                                    // land slide over to make room.
                                    readonly property real shift: {
                                        const f = page.dragFrom, t = page.dropTarget, i = stepRow.index
                                        if (f < 0 || stepRow.dragged)
                                            return 0
                                        return f < i && i <= t ? -page.dragPitch : t <= i && i < f ? page.dragPitch : 0
                                    }
                                    z: dragged ? 2 : 0
                                    transform: Translate {
                                        y: stepRow.dragged ? page.dragY : stepRow.shift
                                        Behavior on y {
                                            enabled: page.dragFrom >= 0 && !stepRow.dragged
                                            NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
                                        }
                                    }

                                    // The room it would land in, outlined (drawn from the dragged
                                    // row, so it stays put while the row moves).
                                    Rectangle {
                                        objectName: "dropSlot"
                                        visible: stepRow.dragged
                                        width: parent.width
                                        height: parent.height
                                        y: (page.dropTarget - stepRow.index) * page.dragPitch - page.dragY
                                        radius: theme.radius
                                        color: "transparent"
                                        border.width: Math.max(1, Math.round(ui.unit))
                                        border.color: theme.accent
                                    }
                                    Rectangle {
                                        anchors.fill: parent
                                        visible: stepRow.dragged
                                        radius: theme.radius
                                        color: theme.chipHover
                                    }

                                    RowLayout {
                                        id: stepLine
                                        width: parent.width
                                        spacing: 6 * ui.unit

                                        // Drag handle: press and move to put the step elsewhere.
                                        Item {
                                            objectName: "stepHandle"
                                            implicitWidth: 20 * ui.unit
                                            implicitHeight: 34 * ui.unit
                                            Icon {
                                                anchors.centerIn: parent
                                                name: "grip-vertical"
                                                color: grip.containsMouse || grip.pressed ? theme.foreground : theme.secondary
                                            }
                                            MouseArea {
                                                id: grip
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                preventStealing: true
                                                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                                onPressed: (mouse) => {
                                                    const p = mapToItem(null, mouse.x, mouse.y)
                                                    page.startDrag(stepRow.index, p.x, p.y)
                                                }
                                                onPositionChanged: (mouse) => {
                                                    if (pressed) {
                                                        const p = mapToItem(null, mouse.x, mouse.y)
                                                        page.dragTo(p.x, p.y)
                                                    }
                                                }
                                                onReleased: page.endDrag(true)
                                                onCanceled: page.endDrag(false)
                                            }
                                        }
                                        Chip {
                                            Layout.preferredWidth: 90 * ui.unit
                                            text: stepRow.modelData.press ? qsTr("↓ Press") : qsTr("↑ Release")
                                            onClicked: page.setStep(stepRow.step, { press: !stepRow.modelData.press })
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            leftPadding: 6 * ui.unit
                                            text: names.usageName(stepRow.modelData.usage)
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        AppTextField {
                                            Layout.preferredWidth: 72 * ui.unit
                                            visible: !stepRow.last
                                            enabled: !page.standard
                                            horizontalAlignment: Text.AlignRight
                                            font.features: { "tnum": 1 }
                                            inputMethodHints: Qt.ImhDigitsOnly
                                            validator: IntValidator { bottom: 0; top: page.limits.maxDelay }
                                            text: page.standard ? page.limits.standardDelay : stepRow.modelData.delay
                                            onEditingFinished: if (acceptableInput && Number(text) !== stepRow.modelData.delay)
                                                                   page.setStep(stepRow.step, { delay: Number(text) })
                                        }
                                        Label {
                                            Layout.preferredWidth: (72 + 6 + 24) * ui.unit
                                            visible: stepRow.last
                                            horizontalAlignment: Text.AlignRight
                                            text: qsTr("1 ms (end)")
                                            color: theme.secondary
                                            font.features: { "tnum": 1 }
                                        }
                                        Label {
                                            Layout.preferredWidth: 24 * ui.unit
                                            visible: !stepRow.last
                                            text: "ms"
                                            color: theme.secondary
                                        }
                                        AppButton {
                                            kind: "quiet"
                                            text: "↑"
                                            enabled: stepRow.index > 0
                                            onClicked: page.moveStep(stepRow.step, page.newestFirst ? 1 : -1)
                                        }
                                        AppButton {
                                            kind: "quiet"
                                            text: "↓"
                                            enabled: stepRow.index < page.draft.steps.length - 1
                                            onClicked: page.moveStep(stepRow.step, page.newestFirst ? -1 : 1)
                                        }
                                        AppButton {
                                            kind: "quiet"
                                            text: "✕"
                                            onClicked: page.removeStep(stepRow.step)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Label {
                    visible: !page.fits
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: theme.error
                    text: qsTr("This macro doesn't fit: the keyboard holds %1 bytes of macros, and saving it would use %2.")
                          .arg(backend.macroStorage.max).arg(page.usedAfterSave)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12 * ui.unit
                    AppButton {
                        objectName: "saveMacro"
                        kind: "primary"
                        text: qsTr("Save")
                        enabled: page.canSave
                        onClicked: page.save()
                    }
                    AppButton {
                        objectName: "clearMacro"
                        text: qsTr("Clear")
                        enabled: page.draft.steps.length > 0
                        onClicked: page.clearSteps()
                    }
                }

                // Under Save and Clear, inside the editor column.
                IconButton {
                    objectName: "deleteMacro"
                    visible: page.saved !== null
                    kind: "quiet"
                    iconName: "trash-2"
                    text: qsTr("Delete macro")
                    onClicked: deleteDialog.open()
                }

                Item { Layout.fillHeight: true }
            }

            // Keeps the list on the left when the editor is hidden or narrower than the page.
            Item { Layout.fillWidth: true }
        }
    }

    // Keys the "Add a key" list offers: letters, digits, symbols, F1–F12, editing, arrows, modifiers.
    readonly property var addableKeys: {
        let r = []
        const add = (from, to) => { for (let u = from; u <= to; ++u) r.push(u) }
        add(0x04, 0x52)
        add(0xe0, 0xe7)
        return r.filter(u => u !== 0x32)
    }

    Dialog {
        id: deleteDialog
        objectName: "deleteMacroDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        background: Rectangle {
            radius: theme.radius
            color: theme.overlay
        }
        Overlay.modal: Rectangle { color: Qt.alpha(theme.background, 0.7) }
        footer: Item {
            implicitHeight: deleteDialogButtons.implicitHeight + 24 * ui.unit
            implicitWidth: deleteDialogButtons.implicitWidth + 48 * ui.unit
            RowLayout {
                id: deleteDialogButtons
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.rightMargin: 24 * ui.unit
                spacing: 8 * ui.unit
                AppButton { text: qsTr("Cancel"); onClicked: deleteDialog.reject() }
                AppButton { kind: "primary"; text: qsTr("Delete macro"); onClicked: deleteDialog.accept() }
            }
        }
        readonly property int bound: page.saved ? page.boundKeys(page.index) : 0

        // The heading sits in the body: a long wrapped Material title makes a sizing loop.
        ColumnLayout {
            spacing: 12 * ui.unit
            Label {
                Layout.preferredWidth: 400 * ui.unit
                text: qsTr("Delete “%1”?").arg(page.saved ? page.saved.name : "")
                wrapMode: Text.WordWrap
                font.pixelSize: ui.section
                font.weight: Font.DemiBold
            }
            Label {
                Layout.preferredWidth: 400 * ui.unit
                wrapMode: Text.WordWrap
                text: deleteDialog.bound === 0 ? qsTr("No key runs it.")
                      : deleteDialog.bound === 1 ? qsTr("One key runs it; it will do nothing afterwards. Keys bound to the other macros keep theirs.")
                      : qsTr("%1 keys run it; they will do nothing afterwards. Keys bound to the other macros keep theirs.").arg(deleteDialog.bound)
            }
        }
        onAccepted: page.remove()
    }
}
