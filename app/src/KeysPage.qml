// SPDX-License-Identifier: GPL-3.0-or-later
// Keys: one drawing, one selection, and tabs for what to change on the selected keys (action,
// travel, advanced function). The active tab decides what each key on the drawing says. Any
// selection can be edited in every tab: the user decides what makes sense.
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
    function select(ids) {
        page.selection = ids.slice()
    }
    function allKeys() {
        return backend.layout.map(k => k.id)
    }
    function invert() {
        page.selection = page.allKeys().filter(id => page.selection.indexOf(id) < 0)
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            // Nothing to show until the keyboard has been read: no made-up values.
            visible: backend.ready && backend.keymapReady && backend.travelReady && backend.advancedReady
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
                id: tabBar
                objectName: "keysTabs"
                Layout.leftMargin: 32
                currentIndex: page.tab
                onCurrentIndexChanged: page.tab = currentIndex
                TabButton { text: qsTr("Action"); width: implicitWidth }
                TabButton { text: qsTr("Travel"); width: implicitWidth }
                TabButton { text: qsTr("Advanced"); width: implicitWidth }
            }

            Card {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
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

                Label {
                    Layout.fillWidth: true
                    text: page.current.drawingHelp
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                }

                KeyboardView {
                    objectName: "keysKeyboard"
                    Layout.fillWidth: true
                    Layout.maximumWidth: implicitWidth
                    Layout.alignment: Qt.AlignHCenter
                    labelFor: page.current.labelFor
                    markedFor: page.current.markedFor
                    selectedKeys: page.selection
                    onKeyClicked: (id, modifiers) => page.click(id, modifiers)
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Chip { text: qsTr("All keys"); onClicked: page.select(page.allKeys()) }
                    Chip { text: qsTr("Invert"); onClicked: page.invert() }
                    Chip { text: qsTr("None"); onClicked: page.select([]) }
                    Label {
                        objectName: "selectedCount"
                        height: 34
                        leftPadding: 10
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("Selected: %1").arg(page.selection.length)
                        opacity: 0.7
                    }
                }
            }

            // Hidden tabs take no room in the layout, and keep their state (an unsaved draft).
            ActionTab {
                id: actionTab
                objectName: "actionTab"
                Layout.fillWidth: true
                visible: page.tab === 0
                selection: page.selection
            }
            TravelTab {
                id: travelTab
                objectName: "travelTab"
                Layout.fillWidth: true
                visible: page.tab === 1
                selection: page.selection
            }
            AdvancedTab {
                id: advancedTab
                objectName: "advancedTab"
                Layout.fillWidth: true
                visible: page.tab === 2
                selection: page.selection
                onSelectKeys: (ids) => page.select(ids)
            }

            Item { implicitHeight: 12 }
        }
    }
}
