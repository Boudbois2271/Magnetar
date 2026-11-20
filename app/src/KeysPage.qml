// SPDX-License-Identifier: GPL-3.0-or-later
// Keymap: pick a layer, click a key, choose what it does. The drawing shows what the keyboard
// read back; a dot marks keys that differ from the factory keymap.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: page

    property int activeLayer: 0
    property int selectedKey: -1
    property int category: 0
    property string search: ""
    property string pendingCode: ""  // waiting for the "last Fn key" confirmation

    KeyNames { id: names }

    readonly property var layerMap: backend.keymapReady && backend.keymap.length > page.activeLayer
                                    ? backend.keymap[page.activeLayer] : ({})
    readonly property var current: page.selectedKey >= 0 ? page.layerMap[String(page.selectedKey)] : undefined
    readonly property var factory: page.selectedKey >= 0 ? backend.factoryAction(page.activeLayer, page.selectedKey) : undefined

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
    readonly property var symbols: page.range(0x1e, 0x27).concat(page.range(0x2d, 0x31), page.range(0x33, 0x38))
    readonly property var functionKeys: page.range(0x3a, 0x45).concat(page.range(0x68, 0x73))

    // Every action the picker offers, by category. "shortcut" is the builder, not a list.
    readonly property var categories: [
        { name: qsTr("Letters"), items: page.keyItems(page.range(0x04, 0x1d)) },
        { name: qsTr("Numbers and symbols"), items: page.keyItems(page.symbols) },
        { name: qsTr("Function keys"), items: page.keyItems(page.functionKeys) },
        { name: qsTr("Editing and navigation"), items: page.keyItems(page.navigation) },
        { name: qsTr("Numpad"), items: page.keyItems(page.range(0x53, 0x63)) },
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
    ]

    readonly property var shownItems: {
        const q = page.search.trim().toLowerCase()
        if (q === "")
            return page.categories[page.category].items
        let found = []
        for (const c of page.categories)
            for (const i of c.items)
                if (i.label.toLowerCase().indexOf(q) >= 0)
                    found.push(i)
        return found
    }

    // Keys a shortcut can use: everything that is a plain key in the lists above.
    readonly property var shortcutKeys: page.keyItems(page.range(0x04, 0x1d).concat(
        page.symbols, page.functionKeys, page.navigation, page.range(0x53, 0x63)))

    function labelFor(id) {
        return names.capName(page.layerMap[String(id)])
    }
    function differsFromFactory(id) {
        const now = page.layerMap[String(id)]
        return now !== undefined && now.code !== backend.factoryAction(page.activeLayer, id).code
    }

    // Changing the last Fn or Fn1 key of the Default layer leaves that layer unreachable.
    function lastLayerKey() {
        if (page.activeLayer !== 0 || !page.current || page.current.kind !== "layer")
            return false
        let count = 0
        for (const k in page.layerMap)
            if (page.layerMap[k].code === page.current.code)
                ++count
        return count === 1
    }
    function assign(code) {
        if (page.selectedKey < 0 || code === "" || (page.current && code === page.current.code))
            return
        if (page.lastLayerKey()) {
            page.pendingCode = code
            confirmDialog.open()
            return
        }
        backend.setKeyAction(page.activeLayer, page.selectedKey, code)
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            // Nothing to show until the keymap has been read: no made-up values.
            visible: backend.ready && backend.keymapReady
            enabled: backend.connected
            spacing: 20

            Item { implicitHeight: 12 }

            Label {
                Layout.leftMargin: 32
                text: qsTr("Keys")
                font.pixelSize: 26
                font.bold: true
            }

            TabBar {
                id: layerTabs
                objectName: "layerTabs"
                Layout.leftMargin: 32
                currentIndex: page.activeLayer
                onCurrentIndexChanged: page.activeLayer = currentIndex
                TabButton { text: qsTr("Default layer"); width: implicitWidth }
                TabButton { text: qsTr("Fn layer"); width: implicitWidth }
                TabButton { text: qsTr("Fn1 layer"); width: implicitWidth }
            }

            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: page.activeLayer === 0 ? qsTr("What each key does")
                                        : qsTr("What each key does while %1 is held").arg(page.activeLayer === 1 ? "Fn" : "Fn1")

                KeyboardView {
                    objectName: "keyboardView"
                    Layout.fillWidth: true
                    labelFor: page.labelFor
                    markedFor: page.differsFromFactory
                    selectedKey: page.selectedKey
                    onKeyClicked: (id) => page.selectedKey = id
                }
            }

            // ---- selected key --------------------------------------------------
            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Selected key")

                Label {
                    visible: page.selectedKey < 0
                    text: qsTr("Click a key on the drawing to change what it does.")
                    opacity: 0.7
                }

                RowLayout {
                    visible: page.selectedKey >= 0
                    spacing: 16

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            objectName: "currentAction"
                            text: page.current ? names.name(page.current) : ""
                            font.pixelSize: 18
                            font.bold: true
                        }
                        Label {
                            text: page.factory && page.current && page.factory.code !== page.current.code
                                  ? qsTr("Factory setting: %1").arg(names.name(page.factory))
                                  : qsTr("This is the factory setting.")
                            opacity: 0.6
                        }
                    }
                    Button {
                        objectName: "restoreButton"
                        text: qsTr("Restore default")
                        enabled: page.factory !== undefined && page.current !== undefined
                                 && page.factory.code !== page.current.code
                        onClicked: page.assign(page.factory.code)
                    }
                }
            }

            // ---- picker --------------------------------------------------------
            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                title: qsTr("Change it to")
                visible: page.selectedKey >= 0

                TextField {
                    objectName: "searchField"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search, e.g. volume or F5")
                    text: page.search
                    onTextEdited: page.search = text
                }

                Flow {
                    Layout.fillWidth: true
                    visible: page.search.trim() === ""
                    spacing: 6
                    Repeater {
                        model: page.categories
                        delegate: Chip {
                            required property int index
                            required property var modelData
                            on: page.category === index
                            text: modelData.name
                            onClicked: page.category = index
                        }
                    }
                }

                // A list of ready-made actions…
                Flow {
                    Layout.fillWidth: true
                    visible: !(page.search.trim() === "" && page.categories[page.category].shortcut)
                    spacing: 6
                    Repeater {
                        model: page.shownItems
                        delegate: Chip {
                            required property var modelData
                            // The key's current action is filled in.
                            on: page.current !== undefined && page.current.code === modelData.code
                            text: modelData.label
                            onClicked: page.assign(modelData.code)
                        }
                    }
                }
                Label {
                    visible: page.search.trim() !== "" && page.shownItems.length === 0
                    text: qsTr("Nothing matches.")
                    opacity: 0.6
                }

                // …or the shortcut builder: one to four modifiers and one or two keys.
                ColumnLayout {
                    id: builder
                    Layout.fillWidth: true
                    visible: page.search.trim() === "" && page.categories[page.category].shortcut === true
                    spacing: 10

                    property int modifiers: 0
                    readonly property string code: firstKey.currentIndex < 0 ? "" : backend.comboCode(
                        builder.modifiers,
                        secondKey.currentIndex > 0
                            ? [page.shortcutKeys[firstKey.currentIndex].usage, page.shortcutKeys[secondKey.currentIndex - 1].usage]
                            : [page.shortcutKeys[firstKey.currentIndex].usage])

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
                                model: page.shortcutKeys.map(k => k.label)
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
                                model: [qsTr("None")].concat(page.shortcutKeys.map(k => k.label))
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
                            onClicked: page.assign(builder.code)
                        }
                        Label {
                            text: builder.modifiers !== 0 && builder.code !== "" ? names.name(backend.actionInfo(builder.code)) : ""
                            opacity: 0.7
                        }
                    }
                }
            }

            Item { implicitHeight: 12 }
        }
    }

    Dialog {
        id: confirmDialog
        objectName: "confirmLayerKey"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        // The heading sits in the body: a long wrapped Material title makes a sizing loop.
        ColumnLayout {
            spacing: 12
            Label {
                Layout.preferredWidth: 400
                text: qsTr("Change the last %1 key?").arg(page.current ? names.name(page.current) : "")
                wrapMode: Text.WordWrap
                font.pixelSize: 18
                font.bold: true
            }
            Label {
                Layout.preferredWidth: 400
                wrapMode: Text.WordWrap
                text: qsTr("This is the only %1 key on the Default layer. Without it, the %1 layer can't be reached from the keyboard until you set one again here.")
                      .arg(page.current ? names.name(page.current) : "")
            }
        }
        onAccepted: backend.setKeyAction(page.activeLayer, page.selectedKey, page.pendingCode)
    }
}
