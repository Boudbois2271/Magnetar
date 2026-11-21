// SPDX-License-Identifier: GPL-3.0-or-later
// Choose an action: categories of ready-made actions, a search box, and a shortcut builder.
// Used by the Keys page and the advanced key editors.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: picker

    property string currentCode: ""   // filled in among the choices
    property bool offerNothing: true  // the "Nothing" category (unbound)
    property int category: 0
    property string search: ""
    signal picked(string code)

    spacing: 12

    KeyNames { id: names }

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
        { name: qsTr("Letters"), items: picker.keyItems(picker.range(0x04, 0x1d)) },
        { name: qsTr("Numbers and symbols"), items: picker.keyItems(picker.symbols) },
        { name: qsTr("Function keys"), items: picker.keyItems(picker.functionKeys) },
        { name: qsTr("Editing and navigation"), items: picker.keyItems(picker.navigation) },
        { name: qsTr("Numpad"), items: picker.keyItems(picker.range(0x53, 0x63)) },
        { name: qsTr("Modifiers"), items: [1, 2, 4, 8, 16, 32, 64, 128].map(
              b => ({ label: names.modifierName(b), code: backend.comboCode(b, []) })) },
        { name: qsTr("Shortcut"), shortcut: true, items: [] },
        { name: qsTr("Media"), items: names.media.map(m => ({ label: m.name, code: backend.consumerCode(m.usage) })) },
        { name: qsTr("Mouse"), items: [1, 2, 3].map(b => ({ label: names.mouseName(b), code: backend.mouseCode(b) })) },
        { name: qsTr("Layers and lighting"), items: [
              { label: "Fn", code: backend.layerCode(0) },
              { label: "Fn1", code: backend.layerCode(1) },
              { label: qsTr("Brightness cycle"), code: backend.brightnessCycleCode() }] },
        { name: qsTr("Nothing"), items: [{ label: qsTr("Nothing (the key does nothing)"), code: backend.unboundCode() }] }
    ].filter(c => picker.offerNothing || c.items.length === 0 || c.items[0].code !== backend.unboundCode())

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


    TextField {
        objectName: "searchField"
        Layout.fillWidth: true
        placeholderText: qsTr("Search, e.g. volume or F5")
        text: picker.search
        onTextEdited: picker.search = text
    }

    Flow {
        Layout.fillWidth: true
        visible: picker.search.trim() === ""
        spacing: 6
        Repeater {
            model: picker.categories
            delegate: Chip {
                required property int index
                required property var modelData
                on: picker.category === index
                text: modelData.name
                onClicked: picker.category = index
            }
        }
    }

    // A list of ready-made actions…
    Flow {
        Layout.fillWidth: true
        visible: !(picker.search.trim() === "" && picker.categories[picker.category].shortcut)
        spacing: 6
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
    Label {
        visible: picker.search.trim() !== "" && picker.shownItems.length === 0
        text: qsTr("Nothing matches.")
        opacity: 0.6
    }

    // …or the shortcut builder: one to four modifiers and one or two keys.
    ColumnLayout {
        id: builder
        Layout.fillWidth: true
        visible: picker.search.trim() === "" && picker.categories[picker.category].shortcut === true
        spacing: 10

        property int modifiers: 0
        readonly property string code: firstKey.currentIndex < 0 ? "" : backend.comboCode(
            builder.modifiers,
            secondKey.currentIndex > 0
                ? [picker.shortcutKeys[firstKey.currentIndex].usage, picker.shortcutKeys[secondKey.currentIndex - 1].usage]
                : [picker.shortcutKeys[firstKey.currentIndex].usage])

        Label {
            Layout.fillWidth: true
            text: qsTr("Pick at least one modifier and a key, then apply.")
            wrapMode: Text.WordWrap
            opacity: 0.7
        }
        Flow {
            Layout.fillWidth: true
            spacing: 6
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
            spacing: 16
            Row {
                spacing: 10
                Label { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Key") }
                ComboBox {
                    id: firstKey
                    width: 160
                    model: picker.shortcutKeys.map(k => k.label)
                    currentIndex: -1
                    displayText: currentIndex < 0 ? qsTr("Choose") : currentText
                }
            }
            Row {
                spacing: 10
                Label { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Second key") }
                ComboBox {
                    id: secondKey
                    width: 160
                    model: [qsTr("None")].concat(picker.shortcutKeys.map(k => k.label))
                    currentIndex: 0
                }
            }
        }
        RowLayout {
            spacing: 16
            Button {
                text: qsTr("Apply")
                highlighted: true
                enabled: builder.modifiers !== 0 && builder.code !== ""
                onClicked: picker.picked(builder.code)
            }
            Label {
                text: builder.modifiers !== 0 && builder.code !== "" ? names.name(backend.actionInfo(builder.code)) : ""
                opacity: 0.7
            }
        }
    }
}
