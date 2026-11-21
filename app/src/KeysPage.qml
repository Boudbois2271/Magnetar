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
    property string pendingCode: ""  // waiting for the "last Fn key" confirmation

    KeyNames { id: names }

    readonly property var layerMap: backend.keymapReady && backend.keymap.length > page.activeLayer
                                    ? backend.keymap[page.activeLayer] : ({})
    readonly property var current: page.selectedKey >= 0 ? page.layerMap[String(page.selectedKey)] : undefined
    readonly property var factory: page.selectedKey >= 0 ? backend.factoryAction(page.activeLayer, page.selectedKey) : undefined

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
                    Layout.maximumWidth: implicitWidth
                    Layout.alignment: Qt.AlignHCenter
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

                ActionPicker {
                    Layout.fillWidth: true
                    currentCode: page.current !== undefined ? page.current.code : ""
                    onPicked: (code) => page.assign(code)
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
