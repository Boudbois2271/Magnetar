// SPDX-License-Identifier: GPL-3.0-or-later
// Keys: one drawing, one selection, and tabs for what to change on the selected keys (action,
// travel, advanced function). The active tab decides what each key on the drawing says. Any
// selection can be edited in every tab: the user decides what makes sense.
// Layout: header (title, tabs, the tab's page actions) → the keyboard (on Advanced, the keys that
// have an advanced function beside it, as tall as it) → the selection bar under its plate → the
// tab's controls, from the left edge.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: page

    property var selection: []  // key IDs, in the order they were picked
    property int tab: 0
    readonly property var tabs: [actionTab, travelTab, advancedTab]
    readonly property var current: page.tabs[page.tab]

    // ---- selection --------------------------------------------------------
    // A click selects only that key, or clears it when it is the only one selected; Ctrl or
    // Shift adds it, or takes it out.
    function click(id, modifiers) {
        if (modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) {
            const i = page.selection.indexOf(id)
            let s = page.selection.slice()
            if (i >= 0)
                s.splice(i, 1)
            else
                s.push(id)
            page.selection = s
        } else if (page.selection.length === 1 && page.selection[0] === id) {
            page.selection = []
        } else {
            page.selection = [id]
        }
    }
    // A box selects the keys in it; with Ctrl or Shift it adds them.
    function box(ids, modifiers) {
        if (modifiers & (Qt.ControlModifier | Qt.ShiftModifier))
            page.selection = page.selection.concat(ids.filter(id => page.selection.indexOf(id) < 0))
        else
            page.selection = ids.slice()
    }
    function select(ids) {
        page.selection = ids.slice()
    }
    function allKeys() {
        return backend.layout.map(k => k.id)
    }
    function invert() {
        page.selection = page.allKeys().filter(id => page.selection.indexOf(id) < 0)
    }

    // The drawing is as wide as the page allows, up to its largest keys, and leaves the controls
    // below it the same height whatever the selection: keys never move under the pointer. The
    // Advanced tab's editor is taller (Dynamic keystroke has eight rows) and its key list sits
    // beside the drawing, so there the drawing is smaller.
    readonly property bool sidePanel: page.tab === 2
    // In a narrow window the list goes under the keyboard instead, as a short box.
    readonly property bool sideBelow: frame.width < 400 * ui.unit + page.sideMinWidth + 16 * ui.unit
    readonly property real sideMinWidth: 220 * ui.unit
    readonly property real sideMaxWidth: 320 * ui.unit
    // (In a small window it gives way first: the keyboard keeps a usable size.)
    readonly property real controlsHeight: page.sidePanel ? Math.max(265 * ui.unit, Math.min(380 * ui.unit, 0.45 * frame.height))
                                                          : 265 * ui.unit
    readonly property real drawingWidth: {
        // Height for the plate and the bar under it; both grow with the width (the bar by the
        // plate's bezel).
        const room = frame.height - header.height - 2 * frame.spacing - page.controlsHeight - 34 * ui.unit
                     - (page.sidePanel && page.sideBelow ? 126 * ui.unit : 0)
        const perWidth = 1 / drawing.aspect + drawing.bezel / (drawing.columns + 2 * drawing.bezel)
        const across = frame.width - (page.sidePanel && !page.sideBelow ? page.sideMinWidth + keyboardRow.columnSpacing : 0)
        return Math.max(400 * ui.unit, Math.min(across, drawing.implicitWidth, room / perWidth))
    }

    // A page action: an icon and a word, at the right of the header.
    component IconButton: AppButton {
        id: iconButton
        property string iconName
        implicitWidth: iconRow.implicitWidth + 24 * ui.unit
        contentItem: Item {
            implicitWidth: iconRow.implicitWidth
            implicitHeight: iconRow.implicitHeight
            Row {
                id: iconRow
                anchors.centerIn: parent
                spacing: 6 * ui.unit
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: iconButton.iconName
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: iconButton.text
                }
            }
        }
    }

    PageLayout {
        id: frame
        // Nothing to show until the keyboard has been read: no made-up values.
        visible: backend.ready && backend.keymapReady && backend.travelReady && backend.advancedReady
        enabled: backend.connected

        RowLayout {
            id: header
            Layout.fillWidth: true
            Layout.minimumHeight: frame.headerHeight
            spacing: 24 * ui.unit
            PageTitle { text: qsTr("Keys") }
            TabBar {
                id: tabBar
                objectName: "keysTabs"
                Layout.alignment: Qt.AlignBottom
                currentIndex: page.tab
                onCurrentIndexChanged: page.tab = currentIndex
                TabButton { objectName: "keysTab_0"; text: qsTr("Action"); width: implicitWidth; FocusRing {} }
                TabButton { objectName: "keysTab_1"; text: qsTr("Travel"); width: implicitWidth; FocusRing {} }
                TabButton { objectName: "keysTab_2"; text: qsTr("Advanced"); width: implicitWidth; FocusRing {} }
            }
            Item { Layout.fillWidth: true }
            IconButton {
                objectName: "undoTravel"
                visible: page.tab === 1
                iconName: "undo-2"
                text: qsTr("Undo last change")
                enabled: backend.canUndoTravel
                onClicked: backend.undoTravel()
            }
        }

        // ---- the keyboard and its selection bar ----------------------------
        // The box takes in the drawing's drag margin, so a drag that starts there reaches it.
        GridLayout {
            id: keyboardRow
            Layout.fillWidth: true
            columns: page.sideBelow ? 1 : 3
            columnSpacing: 16 * ui.unit
            rowSpacing: 16 * ui.unit

            Item {
                id: keyboardBox
                readonly property real margin: drawing.dragMargin
                Layout.fillWidth: false
                Layout.preferredWidth: page.drawingWidth + 2 * margin
                Layout.preferredHeight: drawing.height + selectionBar.height + 2 * margin
                Layout.leftMargin: -margin
                Layout.rightMargin: -margin
                Layout.topMargin: -margin
                Layout.bottomMargin: -margin

                KeyboardView {
                    id: drawing
                    objectName: "keysKeyboard"
                    x: parent.margin
                    y: parent.margin
                    width: page.drawingWidth
                    height: implicitHeight
                    maxKeySize: 68 * ui.unit
                    squareBottom: true
                    labelFor: page.current.labelFor
                    markedFor: page.current.markedFor
                    colorFor: page.current.colorFor || function (id) { return "" }
                    depthFor: page.current.depthFor || function (id) { return -1 }
                    badgeFor: page.current.badgeFor || function (id) { return "" }
                    selectedKeys: page.selection
                    onKeyClicked: (id, modifiers) => page.click(id, modifiers)
                    onBoxSelected: (ids, modifiers) => page.box(ids, modifiers)
                }

                // Attached under the plate, on the same surface.
                Rectangle {
                    id: selectionBar
                    x: drawing.x
                    y: drawing.y + drawing.height
                    width: drawing.width
                    height: bar.implicitHeight + drawing.inset
                    bottomLeftRadius: theme.radius
                    bottomRightRadius: theme.radius
                    color: theme.plate

                    BarRow {
                        id: bar
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: drawing.inset
                        anchors.rightMargin: drawing.inset

                        AppButton { text: qsTr("All keys"); onClicked: page.select(page.allKeys()) }
                        AppButton { text: qsTr("Invert"); onClicked: page.invert() }
                        AppButton {
                            visible: page.selection.length > 0
                            text: qsTr("None")
                            onClicked: page.select([])
                        }
                        Label {
                            objectName: "selectedCount"
                            visible: page.selection.length > 0
                            Layout.leftMargin: 10 * ui.unit
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: page.selection.length === 1 ? qsTr("1 key selected")
                                                              : qsTr("%1 keys selected").arg(page.selection.length)
                            elide: Text.ElideRight
                            color: theme.secondary
                            font.features: { "tnum": 1 }
                        }

                        // The keymap has layers; the drawing shows the one being edited.
                        trailing: Segmented {
                            objectName: "layerSwitch"
                            visible: page.tab === 0
                            model: [qsTr("Default layer"), qsTr("Fn layer"), qsTr("Fn1 layer")]
                            currentIndex: actionTab.activeLayer
                            onActivated: (index) => actionTab.activeLayer = index
                        }
                    }
                }
            }

            // ---- Advanced: every key that has one, beside the plate, as tall as it ----
            // A click selects it. The plate's surface, so it reads as part of the keyboard.
            Rectangle {
                objectName: "advancedSide"
                visible: page.sidePanel
                Layout.alignment: Qt.AlignTop
                Layout.fillWidth: true
                Layout.maximumWidth: page.sideBelow ? Number.POSITIVE_INFINITY : page.sideMaxWidth
                Layout.preferredHeight: page.sideBelow ? 110 * ui.unit : drawing.height + selectionBar.height
                radius: theme.radius
                color: theme.plate

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: drawing.inset
                    spacing: 10 * ui.unit

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Keys with an advanced function")
                        wrapMode: Text.WordWrap
                        font.pixelSize: ui.section
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: backend.advancedKeys.length === 0
                        text: qsTr("None yet. Select a key, then choose a function under the keyboard.")
                        wrapMode: Text.WordWrap
                        color: theme.secondary
                    }
                    ScrollView {
                        objectName: "advancedListBox"
                        visible: backend.advancedKeys.length > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: availableWidth

                        Flow {
                            id: advancedList
                            objectName: "advancedList"
                            width: parent.width
                            spacing: 6 * ui.unit
                            Repeater {
                                model: backend.advancedKeys
                                delegate: Chip {
                                    required property var modelData
                                    on: page.selection.indexOf(modelData.key) >= 0
                                    text: advancedTab.keyName(modelData.key)
                                          + (modelData.type === "socd" ? " + " + advancedTab.keyName(modelData.key2) : "")
                                          + " · " + advancedTab.typeName(modelData.type)
                                    onClicked: page.select(modelData.type === "socd" ? [modelData.key, modelData.key2] : [modelData.key])
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true; visible: !(backend.advancedKeys.length > 0) }
                }
            }
            Item { Layout.fillWidth: true; visible: !page.sidePanel }
        }

        // ---- the tab's controls ----------------------------------------------
        // Hidden tabs keep their state (an unsaved draft). They fit the height left: only the
        // lists in them scroll, in their own boxes.
        Item {
            objectName: "keysPanel"
            Layout.fillWidth: true
            Layout.fillHeight: true

            ActionTab {
                id: actionTab
                objectName: "actionTab"
                anchors.fill: parent
                visible: page.tab === 0
                selection: page.selection
            }
            TravelTab {
                id: travelTab
                objectName: "travelTab"
                anchors.fill: parent
                visible: page.tab === 1
                selection: page.selection
            }
            AdvancedTab {
                id: advancedTab
                objectName: "advancedTab"
                anchors.fill: parent
                visible: page.tab === 2
                selection: page.selection
                onSelectKeys: (ids) => page.select(ids)
            }
        }
    }
}
