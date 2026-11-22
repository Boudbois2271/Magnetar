// SPDX-License-Identifier: GPL-3.0-or-later
// Keys page, Action tab: what the selected keys do on the chosen layer. One key or many: the
// picker sets the same action on every selected key. The drawing shows what the keyboard read
// back; a dot marks keys that differ from the factory keymap.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: tab
    spacing: 20

    property var selection: []
    property int activeLayer: 0
    property string pendingCode: ""  // waiting for the "last Fn key" confirmation
    property string pendingLayer: ""  // the layer key's name, for the dialog

    KeyNames { id: names }

    readonly property string drawingTitle: tab.activeLayer === 0 ? qsTr("What each key does")
        : qsTr("What each key does while %1 is held").arg(tab.activeLayer === 1 ? "Fn" : "Fn1")

    readonly property var layerMap: backend.keymapReady && backend.keymap.length > tab.activeLayer
                                    ? backend.keymap[tab.activeLayer] : ({})
    readonly property var currents: tab.selection.map(id => tab.layerMap[String(id)]).filter(a => a !== undefined)
    // The action every selected key has, or undefined when they differ.
    readonly property var current: tab.currents.length > 0 && tab.currents.every(a => a.code === tab.currents[0].code)
                                   ? tab.currents[0] : undefined
    readonly property var notFactory: tab.selection.filter(id => tab.differsFromFactory(id))

    function labelFor(id) {
        return names.capName(tab.layerMap[String(id)])
    }
    function differsFromFactory(id) {
        const now = tab.layerMap[String(id)]
        return now !== undefined && now.code !== backend.factoryAction(tab.activeLayer, id).code
    }
    readonly property var markedFor: tab.differsFromFactory

    // Changing every Fn (or Fn1) key of the Default layer leaves that layer unreachable. Returns
    // the layer key that would be lost, or undefined.
    function lostLayerKey(keys, code) {
        if (tab.activeLayer !== 0)
            return undefined
        for (const id of keys) {
            const a = tab.layerMap[String(id)]
            if (!a || a.kind !== "layer" || a.code === code)
                continue
            let others = false
            for (const k in tab.layerMap)
                if (tab.layerMap[k].code === a.code && keys.indexOf(Number(k)) < 0)
                    others = true
            if (!others)
                return a
        }
        return undefined
    }
    function assign(code) {
        if (code === "")
            return
        const keys = tab.selection.filter(id => tab.layerMap[String(id)] === undefined || tab.layerMap[String(id)].code !== code)
        if (keys.length === 0)
            return
        const lost = tab.lostLayerKey(keys, code)
        if (lost !== undefined) {
            tab.pendingCode = code
            tab.pendingLayer = names.name(lost)
            confirmDialog.open()
            return
        }
        backend.setKeyActions(tab.activeLayer, keys, code)
    }
    function send(code) {
        const keys = tab.selection.filter(id => tab.layerMap[String(id)] === undefined || tab.layerMap[String(id)].code !== code)
        if (keys.length > 0)
            backend.setKeyActions(tab.activeLayer, keys, code)
    }
    function restoreDefaults() {
        for (const id of tab.notFactory)
            backend.restoreKeyDefault(tab.activeLayer, id)
    }

    // ---- selected keys -----------------------------------------------------
    Card {
        Layout.leftMargin: 32
        Layout.rightMargin: 32
        title: tab.selection.length > 1 ? qsTr("%1 keys selected").arg(tab.selection.length) : qsTr("Selected key")

        RowLayout {
            visible: tab.selection.length > 0
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Label {
                    objectName: "currentAction"
                    text: tab.current ? names.name(tab.current) : qsTr("Different actions")
                    font.pixelSize: 18
                    font.bold: true
                }
                Label {
                    readonly property var factory: tab.selection.length === 1
                        ? backend.factoryAction(tab.activeLayer, tab.selection[0]) : undefined
                    text: tab.notFactory.length === 0 ? (tab.selection.length > 1 ? qsTr("These are the factory settings.")
                                                                                 : qsTr("This is the factory setting."))
                          : factory !== undefined ? qsTr("Factory setting: %1").arg(names.name(factory))
                          : qsTr("%1 of them differ from the factory setting.").arg(tab.notFactory.length)
                    opacity: 0.6
                }
            }
            Button {
                objectName: "restoreButton"
                text: qsTr("Restore default")
                enabled: tab.notFactory.length > 0
                onClicked: tab.restoreDefaults()
            }
        }
    }

    // ---- picker --------------------------------------------------------------
    Card {
        Layout.leftMargin: 32
        Layout.rightMargin: 32
        title: qsTr("Change it to")
        visible: tab.selection.length > 0

        ActionPicker {
            Layout.fillWidth: true
            currentCode: tab.current !== undefined ? tab.current.code : ""
            onPicked: (code) => tab.assign(code)
        }
    }

    Dialog {
        id: confirmDialog
        objectName: "confirmLayerKey"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        // The heading sits in the body: a long wrapped Material title makes a sizing loop.
        ColumnLayout {
            spacing: 12
            Label {
                Layout.preferredWidth: 400
                text: qsTr("Change the last %1 key?").arg(tab.pendingLayer)
                wrapMode: Text.WordWrap
                font.pixelSize: 18
                font.bold: true
            }
            Label {
                Layout.preferredWidth: 400
                wrapMode: Text.WordWrap
                text: qsTr("This is the only %1 key on the Default layer. Without it, the %1 layer can't be reached from the keyboard until you set one again here.")
                      .arg(tab.pendingLayer)
            }
        }
        onAccepted: tab.send(tab.pendingCode)
    }
}
