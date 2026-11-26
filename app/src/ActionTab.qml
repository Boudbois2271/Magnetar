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
    spacing: 16 * ui.unit

    property var selection: []
    property int activeLayer: 0
    property string pendingCode: ""  // waiting for the "last Fn key" confirmation
    property string pendingLayer: ""  // the layer key's name, for the dialog

    KeyNames { id: names }

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
    // Some key of the shown layer isn't as it came from the factory.
    readonly property bool layerDiffers: Object.keys(tab.layerMap).some(k => tab.differsFromFactory(Number(k)))

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

    // ---- what they do now, then what to change it to ---------------------------
    // One column from the left edge, under the keyboard: the current action on one line, with
    // the reset for the selection beside it, then the picker.
    ColumnLayout {
        visible: tab.selection.length > 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 12 * ui.unit

        RowLayout {
            Layout.fillWidth: true
            spacing: 12 * ui.unit

            Label {
                text: qsTr("Current action")
                font.pixelSize: ui.section
                font.weight: Font.DemiBold
            }
            Label {
                objectName: "currentAction"
                Layout.maximumWidth: 280 * ui.unit
                text: tab.current ? names.name(tab.current) : qsTr("Different actions")
                elide: Text.ElideRight
                font.pixelSize: ui.section
                color: theme.accent  // like the selected keys on the drawing
            }
            Label {
                objectName: "factoryNote"
                readonly property var factory: tab.selection.length === 1
                    ? backend.factoryAction(tab.activeLayer, tab.selection[0]) : undefined
                text: tab.notFactory.length === 0 ? (tab.selection.length > 1 ? qsTr("These are the factory settings.")
                                                                             : qsTr("This is the factory setting."))
                      : factory !== undefined ? qsTr("Factory setting: %1").arg(names.name(factory))
                      : qsTr("%1 of them differ from the factory setting.").arg(tab.notFactory.length)
                // Gives way first in a narrow window, so Restore default stays in it, and wraps
                // then. Rounded up: the layout rounds widths down, which cut off the last letters.
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: Math.ceil(implicitWidth)
                Layout.maximumWidth: Math.ceil(implicitWidth)
                wrapMode: Text.Wrap
                color: theme.secondary
            }
            // Resets the selected keys on this layer, not the whole layer.
            AppButton {
                id: restoreButton
                objectName: "restoreButton"
                Layout.leftMargin: 4 * ui.unit
                implicitWidth: restoreRow.implicitWidth + 24 * ui.unit
                text: qsTr("Restore default")
                enabled: tab.notFactory.length > 0
                onClicked: tab.restoreDefaults()
                contentItem: Item {
                    implicitWidth: restoreRow.implicitWidth
                    implicitHeight: restoreRow.implicitHeight
                    Row {
                        id: restoreRow
                        anchors.centerIn: parent
                        spacing: 6 * ui.unit
                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: "rotate-ccw"
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: restoreButton.text
                        }
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        Card {
            title: qsTr("Change it to")
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true
            Layout.maximumHeight: implicitHeight

            ActionPicker {
                objectName: "actionPicker"
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentCode: tab.current !== undefined ? tab.current.code : ""
                onPicked: (code) => tab.assign(code)
            }
        }
    }
    // Takes the room the cards don't need, so they stay right under the keyboard.
    Item { Layout.fillHeight: true }

    Dialog {
        id: confirmDialog
        objectName: "confirmLayerKey"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        background: Rectangle {
            radius: theme.radius
            color: theme.overlay
        }
        Overlay.modal: Rectangle { color: Qt.alpha(theme.background, 0.7) }
        footer: Item {
            implicitHeight: confirmDialogButtons.implicitHeight + 24 * ui.unit
            implicitWidth: confirmDialogButtons.implicitWidth + 48 * ui.unit
            RowLayout {
                id: confirmDialogButtons
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.rightMargin: 24 * ui.unit
                spacing: 8 * ui.unit
                AppButton { text: qsTr("Cancel"); onClicked: confirmDialog.reject() }
                AppButton { kind: "primary"; text: qsTr("Change it"); onClicked: confirmDialog.accept() }
            }
        }

        // The heading sits in the body: a long wrapped Material title makes a sizing loop.
        ColumnLayout {
            spacing: 12 * ui.unit
            Label {
                Layout.preferredWidth: 400 * ui.unit
                text: qsTr("Change the last %1 key?").arg(tab.pendingLayer)
                wrapMode: Text.WordWrap
                font.pixelSize: ui.section
                font.weight: Font.DemiBold
            }
            Label {
                Layout.preferredWidth: 400 * ui.unit
                wrapMode: Text.WordWrap
                text: qsTr("This is the only %1 key on the Default layer. Without it, the %1 layer can't be reached from the keyboard until you set one again here.")
                      .arg(tab.pendingLayer)
            }
        }
        onAccepted: tab.send(tab.pendingCode)
    }
}
