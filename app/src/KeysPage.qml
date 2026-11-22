// SPDX-License-Identifier: GPL-3.0-or-later
// Keys: one drawing, one selection, and tabs for what to change on the selected keys (action,
// travel, advanced function, colour). The active tab decides what each key on the drawing says. Any
// selection can be edited in every tab: the user decides what makes sense.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: page

    property var selection: []  // key IDs, in the order they were picked
    property int tab: 0
    readonly property var tabs: [actionTab, travelTab, advancedTab, colorTab]
    readonly property var current: page.tabs[page.tab]

    // ---- selection --------------------------------------------------------
    // A click selects only that key; Ctrl or Shift adds it, or takes it out.
    function click(id, modifiers) {
        if (modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) {
            const i = page.selection.indexOf(id)
            let s = page.selection.slice()
            if (i >= 0)
                s.splice(i, 1)
            else
                s.push(id)
            page.selection = s
        } else if (page.selection.length !== 1 || page.selection[0] !== id) {
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

    // Wide windows put the tab's panel beside the drawing, narrow ones below it. The drawing
    // leaves the panel room: at least panelWidth beside it, or most of the height below it.
    readonly property real panelWidth: 560 * ui.unit
    readonly property bool wide: width >= 1220 * ui.unit
    readonly property real drawingMaxWidth: page.wide ? page.width - 64 * ui.unit - page.panelWidth - 56 * ui.unit
                                                      : page.height * 0.42 * 66 / 26

    PageLayout {
        // Nothing to show until the keyboard has been read: no made-up values.
        visible: backend.ready && backend.keymapReady && backend.travelReady && backend.advancedReady
                 && backend.colorsReady
        enabled: backend.connected

        RowLayout {
            spacing: 24 * ui.unit
            Label {
                text: qsTr("Keys")
                font.pixelSize: Math.round(26 * ui.unit)
                font.bold: true
            }
            TabBar {
                id: tabBar
                objectName: "keysTabs"
                Layout.alignment: Qt.AlignBottom
                currentIndex: page.tab
                onCurrentIndexChanged: page.tab = currentIndex
                TabButton { text: qsTr("Action"); width: implicitWidth }
                TabButton { text: qsTr("Travel"); width: implicitWidth }
                TabButton { text: qsTr("Advanced"); width: implicitWidth }
                TabButton { text: qsTr("Colour"); width: implicitWidth }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: page.wide ? 2 : 1
            columnSpacing: 16 * ui.unit
            rowSpacing: 16 * ui.unit

            // The drawing, and under it on wide windows what a tab puts aside.
            ColumnLayout {
                Layout.fillWidth: !page.wide
                Layout.preferredWidth: page.wide ? drawing.Layout.maximumWidth + 40 * ui.unit : -1
                Layout.alignment: Qt.AlignTop
                spacing: 16 * ui.unit

                Card {
                    Layout.fillWidth: !page.wide
                    Layout.preferredWidth: page.wide ? drawing.Layout.maximumWidth + 40 * ui.unit : -1
                    Layout.alignment: Qt.AlignTop
                    title: page.current.drawingTitle

                    // The keymap has layers; the drawing shows the one being edited.
                    TabBar {
                        objectName: "layerTabs"
                        visible: page.tab === 0
                        currentIndex: actionTab.activeLayer
                        onCurrentIndexChanged: actionTab.activeLayer = currentIndex
                        TabButton { text: qsTr("Default layer"); width: implicitWidth }
                        TabButton { text: qsTr("Fn layer"); width: implicitWidth }
                        TabButton { text: qsTr("Fn1 layer"); width: implicitWidth }
                    }

                    KeyboardView {
                        id: drawing
                        objectName: "keysKeyboard"
                        Layout.fillWidth: true
                        Layout.maximumWidth: Math.min(implicitWidth, page.drawingMaxWidth)
                        Layout.alignment: Qt.AlignHCenter
                        labelFor: page.current.labelFor
                        markedFor: page.current.markedFor
                        colorFor: page.current.colorFor || function (id) { return "" }
                        selectedKeys: page.selection
                        onKeyClicked: (id, modifiers) => page.click(id, modifiers)
                        onBoxSelected: (ids, modifiers) => page.box(ids, modifiers)
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 6 * ui.unit
                        Chip { text: qsTr("All keys"); onClicked: page.select(page.allKeys()) }
                        Chip { text: qsTr("Invert"); onClicked: page.invert() }
                        Chip { text: qsTr("None"); onClicked: page.select([]) }
                        Label {
                            objectName: "selectedCount"
                            height: 34 * ui.unit
                            leftPadding: 10 * ui.unit
                            verticalAlignment: Text.AlignVCenter
                            text: qsTr("Selected: %1").arg(page.selection.length)
                            opacity: 0.7
                        }
                    }
                }

                ColumnLayout {
                    id: aside
                    Layout.fillWidth: true
                    spacing: 16 * ui.unit
                }
            }

            // The tab's panel takes the rest of the page; the long lists in it scroll in their
            // own boxes. Hidden tabs keep their state (an unsaved draft).
            ScrollView {
                id: panel
                objectName: "keysPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                // At least the panel's height, so the picker can fill it. Delayed: the tab's
                // height and the layout inside it settle one after the other.
                Binding on contentHeight {
                    value: Math.max(panel.availableHeight, page.current.implicitHeight - (page.current.slack || 0))
                    delayed: true
                }

                Item {
                    width: panel.availableWidth
                    height: panel.contentHeight

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
                        aside: page.wide && page.tab === 2 ? aside : null
                        onSelectKeys: (ids) => page.select(ids)
                    }
                    ColorTab {
                        id: colorTab
                        objectName: "colorTab"
                        anchors.fill: parent
                        visible: page.tab === 3
                        selection: page.selection
                    }
                }
            }
        }
    }
}
