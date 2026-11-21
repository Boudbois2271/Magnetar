// SPDX-License-Identifier: GPL-3.0-or-later
// Per-key travel: select keys on the drawing, then set their actuation point, Rapid Trigger and
// dead zones. Every value shown is what the keyboard read back; a change is sent when a slider
// is let go (or a value typed), and the last one can be undone.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: page

    property var selection: []  // key IDs
    readonly property var travel: backend.travelReady ? backend.travel : ({})
    readonly property var limits: backend.travelLimits
    readonly property bool french: translator.language.indexOf("fr") === 0

    // ---- selection --------------------------------------------------------
    function toggle(id) {
        const i = page.selection.indexOf(id)
        let s = page.selection.slice()
        if (i >= 0)
            s.splice(i, 1)
        else
            s.push(id)
        page.selection = s
    }
    function allKeys() {
        return backend.layout.map(k => k.id)
    }
    function invert() {
        page.selection = page.allKeys().filter(id => page.selection.indexOf(id) < 0)
    }

    // ---- values of the selected keys --------------------------------------
    function values(field) {
        return page.selection.map(id => page.travel[String(id)]).filter(t => t !== undefined).map(t => t[field])
    }
    // The value most of the selected keys have, like the vendor app shows.
    function common(field) {
        const v = page.values(field)
        let best = undefined, bestCount = 0, counts = {}
        for (const x of v) {
            counts[x] = (counts[x] || 0) + 1
            if (counts[x] > bestCount) {
                best = x
                bestCount = counts[x]
            }
        }
        return best
    }
    function mixed(field) {
        const v = page.values(field)
        return v.some(x => x !== v[0])
    }
    function count(field) {
        return page.values(field).filter(x => x).length
    }
    function highest(field) {
        return Math.max.apply(null, [0].concat(page.values(field)))
    }

    function mm(v, decimals) {
        if (v === undefined)
            return ""
        const s = Number(v).toFixed(decimals)
        return page.french ? s.replace(".", ",") : s
    }
    function capLabel(id) {
        const t = page.travel[String(id)]
        return t === undefined ? "" : page.mm(t.actuation, 2)
    }
    function hasExtras(id) {
        const t = page.travel[String(id)]
        return t !== undefined && (t.rapidTrigger || t.deadZones)
    }

    function change(fields) {
        if (page.selection.length > 0)
            backend.changeTravel(page.selection, fields)
    }


    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            // Nothing to show until the travel has been read: no made-up values.
            visible: backend.ready && backend.travelReady
            enabled: backend.connected
            spacing: 20

            Item { implicitHeight: 12 }

            RowLayout {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                spacing: 16
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Travel")
                    font.pixelSize: 26
                    font.bold: true
                }
                Button {
                    objectName: "undoTravel"
                    text: qsTr("Undo last change")
                    enabled: backend.canUndoTravel
                    onClicked: backend.undoTravel()
                }
            }

            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Keys")

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Click keys to select them. Each key shows its actuation point in mm; a dot means Rapid Trigger or dead zones are on.")
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                }

                KeyboardView {
                    objectName: "travelKeyboard"
                    Layout.fillWidth: true
                    Layout.maximumWidth: implicitWidth
                    Layout.alignment: Qt.AlignHCenter
                    labelFor: page.capLabel
                    markedFor: page.hasExtras
                    selectedKeys: page.selection
                    onKeyClicked: (id) => page.toggle(id)
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Chip { text: qsTr("All keys"); onClicked: page.selection = page.allKeys() }
                    Chip { text: qsTr("Invert"); onClicked: page.invert() }
                    Chip { text: qsTr("None"); onClicked: page.selection = [] }
                    Label {
                        height: 34
                        leftPadding: 10
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("Selected: %1").arg(page.selection.length)
                        opacity: 0.7
                    }
                }
            }

            Label {
                Layout.leftMargin: 32
                visible: page.selection.length === 0
                text: qsTr("Select one or more keys to change their travel.")
                opacity: 0.7
            }

            GridLayout {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                visible: page.selection.length > 0
                columns: page.width > 1350 ? 3 : 1
                columnSpacing: 20
                rowSpacing: 20

                // ---- actuation -------------------------------------------------
                Card {
                    Layout.alignment: Qt.AlignTop
                    title: qsTr("Actuation point")

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("How far a key goes down before it types. Lower is quicker, higher forgives resting fingers.")
                        wrapMode: Text.WordWrap
                        opacity: 0.7
                    }
                    MmSlider {
                        objectName: "actuationSlider"
                        label: qsTr("Actuation")
                        shown: page.common("actuation")
                        mixed: page.mixed("actuation")
                        highest: page.highest("actuation")
                        from: page.limits.minActuation
                        to: page.limits.maxActuation
                        step: 0.01
                        decimals: 2
                        french: page.french
                        onCommit: (v) => page.change({ actuation: v })
                    }
                }

                // ---- Rapid Trigger ---------------------------------------------
                Card {
                    id: rtCard
                    Layout.alignment: Qt.AlignTop
                    title: qsTr("Rapid Trigger")

                    property bool separate: false
                    readonly property int onCount: page.count("rapidTrigger")
                    readonly property bool on: onCount === page.selection.length
                    readonly property bool sameValues: !page.mixed("press") && !page.mixed("release")
                                                       && page.common("press") === page.common("release")
                    readonly property bool linked: !separate && sameValues

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("The key lets go as soon as it starts coming back up, and types again as soon as it moves down, wherever it is in its travel.")
                        wrapMode: Text.WordWrap
                        opacity: 0.7
                    }
                    Switch {
                        objectName: "rapidTriggerSwitch"
                        text: rtCard.onCount > 0 && !rtCard.on
                              ? qsTr("On for %1 of %2 keys").arg(rtCard.onCount).arg(page.selection.length)
                              : qsTr("Rapid Trigger")
                        checked: rtCard.on
                        onToggled: page.change({ rapidTrigger: checked })
                    }
                    Switch {
                        text: qsTr("Separate press and release sensitivity")
                        enabled: rtCard.on
                        checked: !rtCard.linked
                        onToggled: {
                            rtCard.separate = checked
                            // Linking again gives release the press value, so one slider is true.
                            if (!checked && !rtCard.sameValues) {
                                const v = page.common("press")
                                page.change({ press: v, release: v })
                            }
                        }
                    }
                    MmSlider {
                        objectName: "sensitivitySlider"
                        visible: rtCard.linked
                        enabled: rtCard.on
                        label: qsTr("Sensitivity")
                        shown: page.common("press")
                        mixed: page.mixed("press") || page.mixed("release")
                        highest: Math.max(page.highest("press"), page.highest("release"))
                        from: page.limits.minRapidTrigger
                        to: page.limits.maxRapidTrigger
                        step: page.limits.step
                        french: page.french
                        onCommit: (v) => page.change({ press: v, release: v })
                    }
                    MmSlider {
                        objectName: "pressSlider"
                        visible: !rtCard.linked
                        enabled: rtCard.on
                        label: qsTr("Press (types again after)")
                        shown: page.common("press")
                        mixed: page.mixed("press")
                        highest: page.highest("press")
                        from: page.limits.minRapidTrigger
                        to: page.limits.maxRapidTrigger
                        step: page.limits.step
                        french: page.french
                        onCommit: (v) => page.change({ press: v })
                    }
                    MmSlider {
                        objectName: "releaseSlider"
                        visible: !rtCard.linked
                        enabled: rtCard.on
                        label: qsTr("Release (lets go after)")
                        shown: page.common("release")
                        mixed: page.mixed("release")
                        highest: page.highest("release")
                        from: page.limits.minRapidTrigger
                        to: page.limits.maxRapidTrigger
                        step: page.limits.step
                        french: page.french
                        onCommit: (v) => page.change({ release: v })
                    }
                }

                // ---- dead zones ------------------------------------------------
                Card {
                    id: dzCard
                    Layout.alignment: Qt.AlignTop
                    title: qsTr("Dead zones")

                    readonly property int onCount: page.count("deadZones")
                    readonly property bool on: onCount === page.selection.length

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Movement ignored at the top of the travel (stops accidental presses) and at the bottom (makes releases more reliable).")
                        wrapMode: Text.WordWrap
                        opacity: 0.7
                    }
                    Switch {
                        objectName: "deadZoneSwitch"
                        text: dzCard.onCount > 0 && !dzCard.on
                              ? qsTr("On for %1 of %2 keys").arg(dzCard.onCount).arg(page.selection.length)
                              : qsTr("Dead zones")
                        checked: dzCard.on
                        onToggled: page.change({ deadZones: checked })
                    }
                    MmSlider {
                        objectName: "deadTopSlider"
                        enabled: dzCard.on
                        label: qsTr("Top")
                        shown: page.common("deadTop")
                        mixed: page.mixed("deadTop")
                        highest: page.highest("deadTop")
                        from: 0
                        to: page.limits.maxDeadZone
                        step: page.limits.step
                        french: page.french
                        onCommit: (v) => page.change({ deadTop: v })
                    }
                    MmSlider {
                        objectName: "deadBottomSlider"
                        enabled: dzCard.on
                        label: qsTr("Bottom")
                        shown: page.common("deadBottom")
                        mixed: page.mixed("deadBottom")
                        highest: page.highest("deadBottom")
                        from: 0
                        to: page.limits.maxDeadZone
                        step: page.limits.step
                        french: page.french
                        onCommit: (v) => page.change({ deadBottom: v })
                    }
                }
            }

            Item { implicitHeight: 12 }
        }
    }
}
