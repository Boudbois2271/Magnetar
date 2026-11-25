// SPDX-License-Identifier: GPL-3.0-or-later
// Keys page, Travel tab: the actuation point, Rapid Trigger and dead zones of the selected keys.
// Every value shown is what the keyboard read back; a change is sent when a slider is let go (or
// a value typed), and the last one can be undone (the page's Undo).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: page
    spacing: 0

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

    readonly property var labelFor: page.capLabel
    // The actuation point as a share of the deepest one the board takes: 0.30 mm stands out.
    readonly property var depthFor: function (id) {
        const t = page.travel[String(id)]
        return t === undefined ? -1 : t.actuation / page.limits.maxActuation
    }
    readonly property var markedFor: page.hasExtras

    function hasExtras(id) {
        const t = page.travel[String(id)]
        return t !== undefined && (t.rapidTrigger || t.deadZones)
    }

    function change(fields) {
        if (page.selection.length > 0)
            backend.changeTravel(page.selection, fields)
    }

    // ---- layout: one column under the drawing, as on the Lighting page ----------
    // A section is a title row (its switch on the right of the title), then one row per value.
    // Labels share one column, sliders start at one line and are all the same width.
    readonly property real rowHeight: 34 * ui.unit  // the app's control height
    readonly property real sliderWidth: 200 * ui.unit
    readonly property real labelWidth: Math.ceil(Math.max(apMetrics.width, rtMetrics.width, dzMetrics.width,
                                                          bodyMetrics.width)) + 12 * ui.unit
    // From the end of the label column to where the sliders' tracks start.
    readonly property real trackOffset: actuation.trackLeft - page.labelWidth
    TextMetrics { id: apMetrics; font.pixelSize: ui.section; font.weight: Font.DemiBold; text: qsTr("Actuation point") }
    TextMetrics { id: rtMetrics; font.pixelSize: ui.section; font.weight: Font.DemiBold; text: qsTr("Rapid Trigger") }
    TextMetrics { id: dzMetrics; font.pixelSize: ui.section; font.weight: Font.DemiBold; text: qsTr("Dead zones") }
    TextMetrics {
        id: bodyMetrics
        font.pixelSize: ui.body
        // The longest value label.
        text: [qsTr("Sensitivity"), qsTr("Press (types again after)"), qsTr("Release (lets go after)"),
               qsTr("Top"), qsTr("Bottom")].reduce((a, b) => a.length >= b.length ? a : b)
    }

    // A section title carrying its switch, the switch's track where the sliders' tracks start.
    // With only some of the selected keys on, it says how many. (An inline component: it sees
    // none of the page's ids, so it is given what it needs.)
    component SectionSwitch: RowLayout {
        id: section
        property string title
        property alias switchName: toggle.objectName
        property int onCount: 0
        property int total: 0
        property real labelWidth
        property real trackOffset
        property real rowHeight: 34 * ui.unit
        readonly property bool on: section.onCount === section.total
        signal toggled(bool checked)
        default property alias extra: tail.data
        Layout.minimumHeight: section.rowHeight
        Layout.maximumHeight: section.rowHeight
        spacing: 0
        Label {
            Layout.preferredWidth: section.labelWidth
            text: section.title
            elide: Text.ElideRight
            font.pixelSize: ui.section
            font.weight: Font.DemiBold
        }
        AppSwitch {
            id: toggle
            Layout.maximumHeight: section.rowHeight
            Layout.leftMargin: section.trackOffset - indicator.x
            checked: section.on
            onToggled: section.toggled(checked)
        }
        Label {
            visible: section.onCount > 0 && !section.on
            Layout.leftMargin: 8 * ui.unit
            text: qsTr("On for %1 of %2 keys").arg(section.onCount).arg(section.total)
            color: theme.secondary
            font.features: { "tnum": 1 }
        }
        RowLayout {
            id: tail
            Layout.leftMargin: page.width < 560 * ui.unit ? 12 * ui.unit : 32 * ui.unit  // closer in a narrow window
            spacing: 0
        }
    }

    ColumnLayout {
        id: controls
        objectName: "travelControls"
        visible: page.selection.length > 0
        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
        // Seven rows at most (Rapid Trigger's press and release apart): they fit the room the Keys
        // page leaves its tabs.
        spacing: 2 * ui.unit

        // ---- actuation: a section of one row --------------------------------
        MmSlider {
            id: actuation
            objectName: "actuationSlider"
            Layout.fillWidth: false
            sliderWidth: page.sliderWidth
            heading: true
            labelWidth: page.labelWidth
            label: qsTr("Actuation point")
            shown: page.common("actuation")
            mixed: page.mixed("actuation")
            from: page.limits.minActuation
            to: page.limits.maxActuation
            step: 0.01
            decimals: 2
            french: page.french
            onCommit: (v) => page.change({ actuation: v })
        }

        // ---- Rapid Trigger --------------------------------------------------
        SectionSwitch {
            id: rt
            Layout.topMargin: 6 * ui.unit
            title: qsTr("Rapid Trigger")
            switchName: "rapidTriggerSwitch"
            onCount: page.count("rapidTrigger")
            total: page.selection.length
            labelWidth: page.labelWidth
            trackOffset: page.trackOffset
            onToggled: (checked) => page.change({ rapidTrigger: checked })

            property bool separate: false
            readonly property bool sameValues: !page.mixed("press") && !page.mixed("release")
                                               && page.common("press") === page.common("release")
            readonly property bool linked: !separate && sameValues

            Label {
                text: qsTr("Separate press and release")
                opacity: rt.on ? 1 : 0.4
            }
            AppSwitch {
                objectName: "separateSwitch"
                Layout.maximumHeight: page.rowHeight
                Layout.leftMargin: 8 * ui.unit
                enabled: rt.on
                checked: !rt.linked
                onToggled: {
                    rt.separate = checked
                    // Linking again gives release the press value, so one slider is true.
                    if (!checked && !rt.sameValues) {
                        const v = page.common("press")
                        page.change({ press: v, release: v })
                    }
                }
            }
        }
        MmSlider {
            objectName: "sensitivitySlider"
            Layout.fillWidth: false
            sliderWidth: page.sliderWidth
            labelWidth: page.labelWidth
            visible: rt.linked
            enabled: rt.on
            label: qsTr("Sensitivity")
            shown: page.common("press")
            mixed: page.mixed("press") || page.mixed("release")
            from: page.limits.minRapidTrigger
            to: page.limits.maxRapidTrigger
            step: page.limits.step
            french: page.french
            onCommit: (v) => page.change({ press: v, release: v })
        }
        MmSlider {
            objectName: "pressSlider"
            Layout.fillWidth: false
            sliderWidth: page.sliderWidth
            labelWidth: page.labelWidth
            visible: !rt.linked
            enabled: rt.on
            label: qsTr("Press (types again after)")
            shown: page.common("press")
            mixed: page.mixed("press")
            from: page.limits.minRapidTrigger
            to: page.limits.maxRapidTrigger
            step: page.limits.step
            french: page.french
            onCommit: (v) => page.change({ press: v })
        }
        MmSlider {
            objectName: "releaseSlider"
            Layout.fillWidth: false
            sliderWidth: page.sliderWidth
            labelWidth: page.labelWidth
            visible: !rt.linked
            enabled: rt.on
            label: qsTr("Release (lets go after)")
            shown: page.common("release")
            mixed: page.mixed("release")
            from: page.limits.minRapidTrigger
            to: page.limits.maxRapidTrigger
            step: page.limits.step
            french: page.french
            onCommit: (v) => page.change({ release: v })
        }

        // ---- dead zones -----------------------------------------------------
        SectionSwitch {
            id: dz
            Layout.topMargin: 6 * ui.unit
            title: qsTr("Dead zones")
            switchName: "deadZoneSwitch"
            onCount: page.count("deadZones")
            total: page.selection.length
            labelWidth: page.labelWidth
            trackOffset: page.trackOffset
            onToggled: (checked) => page.change({ deadZones: checked })
        }
        MmSlider {
            objectName: "deadTopSlider"
            Layout.fillWidth: false
            sliderWidth: page.sliderWidth
            labelWidth: page.labelWidth
            enabled: dz.on
            label: qsTr("Top")
            shown: page.common("deadTop")
            mixed: page.mixed("deadTop")
            from: 0
            to: page.limits.maxDeadZone
            step: page.limits.step
            french: page.french
            onCommit: (v) => page.change({ deadTop: v })
        }
        MmSlider {
            objectName: "deadBottomSlider"
            Layout.fillWidth: false
            sliderWidth: page.sliderWidth
            labelWidth: page.labelWidth
            enabled: dz.on
            label: qsTr("Bottom")
            shown: page.common("deadBottom")
            mixed: page.mixed("deadBottom")
            from: 0
            to: page.limits.maxDeadZone
            step: page.limits.step
            french: page.french
            onCommit: (v) => page.change({ deadBottom: v })
        }
    }

    Item { Layout.fillHeight: true }
}
