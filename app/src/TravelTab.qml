// SPDX-License-Identifier: GPL-3.0-or-later
// Keys page, Travel tab: the actuation point, Rapid Trigger and dead zones of the selected keys.
// Every value shown is what the keyboard read back; a change is sent when a slider is let go (or
// a value typed), and the last one can be undone.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: page
    spacing: 20 * ui.unit

    property var selection: []  // key IDs
    readonly property var travel: backend.travelReady ? backend.travel : ({})
    readonly property var limits: backend.travelLimits
    readonly property bool french: translator.language.indexOf("fr") === 0

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

    readonly property string drawingTitle: qsTr("Actuation point of each key")
    readonly property var labelFor: page.capLabel
    readonly property var markedFor: page.hasExtras

    function hasExtras(id) {
        const t = page.travel[String(id)]
        return t !== undefined && (t.rapidTrigger || t.deadZones)
    }

    function change(fields) {
        if (page.selection.length > 0)
            backend.changeTravel(page.selection, fields)
    }

    RowLayout {
        Layout.leftMargin: 32 * ui.unit
        Layout.rightMargin: 32 * ui.unit
        Item { Layout.fillWidth: true }
        Button {
            objectName: "undoTravel"
            text: qsTr("Undo last change")
            enabled: backend.canUndoTravel
            onClicked: backend.undoTravel()
        }
    }

    GridLayout {
        Layout.leftMargin: 32 * ui.unit
        Layout.rightMargin: 32 * ui.unit
        visible: page.selection.length > 0
        columns: page.width > 1350 * ui.unit ? 3 : 1
        columnSpacing: 20 * ui.unit
        rowSpacing: 20 * ui.unit

        // ---- actuation -------------------------------------------------
        Card {
            Layout.alignment: Qt.AlignTop
            title: qsTr("Actuation point")

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
}
