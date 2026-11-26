// SPDX-License-Identifier: GPL-3.0-or-later
// Choose an action: categories of ready-made actions, a search box, and a shortcut builder.
// Given less height than it needs, the list of actions scrolls in its own box.
// Used by the Keys page and the advanced key editors.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: picker

    property string currentCode: ""   // filled in among the choices
    property bool offerNothing: true  // the "Nothing" action (unbound), last of "Modifiers and layers"
    property int category: 0
    property string search: ""
    property string macroLoop: "times"  // how a picked macro plays: times, untilKeyPressed, whileHeld
    property int macroCount: 1
    signal picked(string code)

    spacing: 12 * ui.unit

    // For a narrow column: the categories are a list beside the search box, not a row of chips.
    property bool compact: false

    KeyNames { id: names }

    // The search box and the actions under it are as wide as the rows of categories.
    readonly property real columnWidth: picker.compact ? Number.POSITIVE_INFINITY
                                                       : Math.max(categoryChips.childrenRect.width, 200 * ui.unit)

    function range(from, to) {
        let r = []
        for (let u = from; u <= to; ++u)
            r.push(u)
        return r
    }
    function keyItems(usages) {
        return usages.map(u => ({ label: names.usageName(u), code: backend.keyCode(u), usage: u }))
    }

    readonly property var navigation: [0x29, 0x28, 0x2a, 0x2b, 0x2c, 0x39, 0x49, 0x4c, 0x4a, 0x4d, 0x4b,
                                       0x4e, 0x52, 0x51, 0x50, 0x4f, 0x46, 0x47, 0x48, 0x65]
    readonly property var symbols: picker.range(0x1e, 0x27).concat(picker.range(0x2d, 0x31), picker.range(0x33, 0x38))
    readonly property var functionKeys: picker.range(0x3a, 0x45).concat(picker.range(0x68, 0x73))

    // Every action the picker offers, by category. "shortcut" is the builder, not a list.
    readonly property var categories: [
        { name: qsTr("Typing"), items: picker.keyItems(picker.range(0x04, 0x1d).concat(picker.symbols)) },
        { name: qsTr("Function keys and numpad"), items: picker.keyItems(picker.functionKeys.concat(picker.range(0x53, 0x63))) },
        { name: qsTr("Editing and navigation"), items: picker.keyItems(picker.navigation) },
        // "Nothing" (unbound) ends this list, unless the picker doesn't offer it.
        { name: qsTr("Modifiers and layers"), items: [1, 2, 4, 8, 16, 32, 64, 128].map(
              b => ({ label: names.modifierName(b), code: backend.comboCode(b, []) })).concat(
              [{ label: "Fn", code: backend.layerCode(0) }, { label: "Fn1", code: backend.layerCode(1) }],
              picker.offerNothing ? [{ label: qsTr("Nothing (the key does nothing)"), code: backend.unboundCode() }] : []) },
        { name: qsTr("Shortcut"), shortcut: true, items: [] },
        { name: qsTr("Macros"), macros: true, items: backend.macros.map((m, i) => ({
              label: names.macroName(i), code: backend.macroCode(i, picker.macroLoop, picker.macroCount) })) },
        { name: qsTr("Media and mouse"), items: names.media.map(m => ({ label: m.name, code: backend.consumerCode(m.usage) })).concat(
              [1, 2, 3, 4, 5].map(b => ({ label: names.mouseName(b), code: backend.mouseCode(b) }))) },
        { name: qsTr("Lighting"), items: [].concat(...names.lighting.map((row, t) => row.map(
              (label, s) => ({ label: label, code: backend.lightingCode(t, s) })))) }
    ]
    readonly property bool onMacros: picker.search.trim() === "" && picker.categories[picker.category].macros === true

    // A key that runs a macro opens on Macros, with its loop mode and count.
    function showCurrent() {
        const info = picker.currentCode === "" ? undefined : backend.actionInfo(picker.currentCode)
        if (!info || info.kind !== "macro" || info.loop === "")
            return
        picker.macroLoop = info.loop
        picker.macroCount = info.loop === "times" ? info.count : picker.macroCount
        picker.category = picker.categories.findIndex(c => c.macros === true)
    }
    onCurrentCodeChanged: picker.showCurrent()
    Component.onCompleted: picker.showCurrent()

    readonly property var shownItems: {
        const q = picker.search.trim().toLowerCase()
        if (q === "")
            return picker.categories[picker.category].items
        let found = []
        for (const c of picker.categories)
            for (const i of c.items)
                if (i.label.toLowerCase().indexOf(q) >= 0)
                    found.push(i)
        return found
    }

    // Keys a shortcut can use: everything that is a plain key in the lists above.
    readonly property var shortcutKeys: picker.keyItems(picker.range(0x04, 0x1d).concat(
        picker.symbols, picker.functionKeys, picker.navigation, picker.range(0x53, 0x63)))


    RowLayout {
        Layout.fillWidth: true
        spacing: 8 * ui.unit
        AppTextField {
            objectName: "searchField"
            Layout.fillWidth: true
            // No wider than the rows of categories under it.
            Layout.maximumWidth: picker.columnWidth
            placeholderText: qsTr("Search, e.g. volume or F5")
            text: picker.search
            onTextEdited: picker.search = text
        }
        AppComboBox {
            id: categoryBox
            objectName: "categoryBox"
            visible: picker.compact
            Layout.preferredWidth: 150 * ui.unit
            enabled: picker.search.trim() === ""
            model: picker.categories.map(c => c.name)
            currentIndex: picker.category
            onActivated: (i) => picker.category = i
            // The list is as wide as its longest name, opening leftwards from the box's right edge.
            popup.width: Math.max(width, Math.ceil(Math.max(...picker.categories.map(c => nameWidth.advanceWidth(c.name)))) + 40 * ui.unit)
            popup.x: width - popup.width
            FontMetrics { id: nameWidth; font: categoryBox.font }
        }
    }

    Flow {
        id: categoryChips
        Layout.fillWidth: true
        visible: picker.search.trim() === "" && !picker.compact
        spacing: 6 * ui.unit
        // Categories read as tabs, apart from the filled chips of the actions under them: text
        // only, and the open one on a chip fill, in bold, underlined in the accent colour.
        Repeater {
            model: picker.categories
            delegate: AppButton {
                id: categoryTab
                required property int index
                required property var modelData
                readonly property bool on: picker.category === index
                kind: on ? "secondary" : "quiet"
                text: modelData.name
                onClicked: picker.category = index
                // As wide as in bold, so the row doesn't shift when another category opens.
                implicitWidth: Math.max(implicitHeight, boldText.implicitWidth + 16 * ui.unit)
                Label {
                    id: boldText
                    visible: false
                    text: categoryTab.text
                    font.weight: Font.DemiBold
                }
                contentItem: Label {
                    text: categoryTab.text
                    font.weight: categoryTab.on ? Font.DemiBold : Font.Normal
                    color: categoryTab.on || categoryTab.hovered ? theme.foreground : theme.secondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                Rectangle {
                    visible: categoryTab.on
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: boldText.implicitWidth
                    height: Math.max(2, Math.round(2 * ui.unit))
                    radius: height / 2
                    color: theme.accent
                }
            }
        }
    }

    // How a macro plays, above the list of macros.
    Flow {
        Layout.fillWidth: true
        visible: picker.onMacros && backend.macros.length > 0
        spacing: 6 * ui.unit
        Repeater {
            model: [{ loop: "times", label: qsTr("Play a number of times") },
                    { loop: "untilKeyPressed", label: qsTr("Repeat until a key is pressed") },
                    { loop: "whileHeld", label: qsTr("Repeat while held") }]
            delegate: Chip {
                required property var modelData
                on: picker.macroLoop === modelData.loop
                text: modelData.label
                onClicked: picker.macroLoop = modelData.loop
            }
        }
        AppSpinBox {
            objectName: "macroCount"
            visible: picker.macroLoop === "times"
            from: 1
            to: backend.macroLimits.maxRepeat
            editable: true
            value: picker.macroCount
            textFromValue: (v) => qsTr("%1×").arg(v)
            valueFromText: (t) => parseInt(t) || 1
            onValueModified: picker.macroCount = value
        }
    }
    // A list of ready-made actions, in its own box: it scrolls when the page has no room left.
    ScrollView {
        id: itemsBox
        objectName: "pickerItems"
        Layout.fillWidth: true
        Layout.maximumWidth: picker.columnWidth  // wraps under the categories, not past them
        Layout.fillHeight: true
        Layout.preferredHeight: items.implicitHeight
        Layout.minimumHeight: Math.min(items.implicitHeight, 80 * ui.unit)
        contentWidth: availableWidth
        visible: !(picker.search.trim() === "" && picker.categories[picker.category].shortcut)

        Flow {
            id: items
            width: parent.width
            spacing: 6 * ui.unit
            Repeater {
                model: picker.shownItems
                delegate: Chip {
                    required property var modelData
                    // The key's current action is filled in.
                    on: picker.currentCode === modelData.code
                    text: modelData.label
                    onClicked: picker.picked(modelData.code)
                }
            }
        }
    }
    Label {
        visible: picker.search.trim() !== "" && picker.shownItems.length === 0
        text: qsTr("Nothing matches.")
        color: theme.secondary
    }

    // …or the shortcut builder: one to four modifiers and one or two keys.
    ColumnLayout {
        id: builder
        Layout.fillWidth: true
        visible: picker.search.trim() === "" && picker.categories[picker.category].shortcut === true
        spacing: 10 * ui.unit

        property int modifiers: 0
        readonly property string code: firstKey.currentIndex < 0 ? "" : backend.comboCode(
            builder.modifiers,
            secondKey.currentIndex > 0
                ? [picker.shortcutKeys[firstKey.currentIndex].usage, picker.shortcutKeys[secondKey.currentIndex - 1].usage]
                : [picker.shortcutKeys[firstKey.currentIndex].usage])

        Flow {
            Layout.fillWidth: true
            spacing: 6 * ui.unit
            Repeater {
                model: [{ bit: 1, label: "Ctrl" }, { bit: 2, label: qsTr("Shift") },
                        { bit: 4, label: "Alt" }, { bit: 8, label: "Win" }]
                delegate: Chip {
                    required property var modelData
                    on: (builder.modifiers & modelData.bit) !== 0
                    text: modelData.label
                    onClicked: builder.modifiers = builder.modifiers ^ modelData.bit
                }
            }
        }
        // Wraps at narrow widths: two labelled pickers don't fit side by side.
        Flow {
            Layout.fillWidth: true
            spacing: 16 * ui.unit
            Row {
                spacing: 10 * ui.unit
                Label { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Key") }
                AppComboBox {
                    id: firstKey
                    width: 160 * ui.unit
                    model: picker.shortcutKeys.map(k => k.label)
                    currentIndex: -1
                    displayText: currentIndex < 0 ? qsTr("Choose") : currentText
                }
            }
            Row {
                spacing: 10 * ui.unit
                Label { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Second key") }
                AppComboBox {
                    id: secondKey
                    width: 160 * ui.unit
                    model: [qsTr("None")].concat(picker.shortcutKeys.map(k => k.label))
                    currentIndex: 0
                }
            }
        }
        RowLayout {
            spacing: 16 * ui.unit
            AppButton {
                kind: "primary"
                text: qsTr("Apply")
                enabled: builder.modifiers !== 0 && builder.code !== ""
                onClicked: picker.picked(builder.code)
            }
            Label {
                text: builder.modifiers !== 0 && builder.code !== "" ? names.name(backend.actionInfo(builder.code)) : ""
                color: theme.secondary
            }
        }
    }
}
