// SPDX-License-Identifier: GPL-3.0-or-later
// The window: sidebar (profile picker, pages), connection banner, the current page, shortcuts.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1000
    height: 680
    minimumWidth: 760
    minimumHeight: 540
    visible: true
    title: "Magnetar"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property int page: 0

    // On-board Fn shortcuts can change the lighting behind our back: re-read on focus.
    onActiveChanged: if (active) backend.refresh()

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; enabled: win.helpVisible; onActivated: win.helpVisible = false }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: backend.connected; onActivated: backend.toggleLights() }
    Shortcut { sequence: "Ctrl+R"; context: Qt.ApplicationShortcut; onActivated: backend.reload() }
    Shortcut { sequence: "Ctrl+1"; context: Qt.ApplicationShortcut; enabled: !backend.calibrating; onActivated: backend.setProfile(1) }
    Shortcut { sequence: "Ctrl+2"; context: Qt.ApplicationShortcut; enabled: !backend.calibrating; onActivated: backend.setProfile(2) }
    Shortcut { sequence: "Ctrl+3"; context: Qt.ApplicationShortcut; enabled: !backend.calibrating; onActivated: backend.setProfile(3) }

    // Right-to-left languages mirror the window; the Translator sets the layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // ---- sidebar -------------------------------------------------------
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 230
                color: "#141417"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 4

                    Label {
                        text: "Magnetar"
                        font.pixelSize: 22
                        font.bold: true
                    }
                    Label {
                        text: "Akko TAC75 HE"
                        opacity: 0.55
                        font.pixelSize: 12
                    }

                    Item { implicitHeight: 18 }

                    Label {
                        text: qsTr("Profile")
                        opacity: 0.55
                        font.pixelSize: 12
                    }
                    ComboBox {
                        id: profileBox
                        objectName: "profileBox"
                        Layout.fillWidth: true
                        // A profile switch would wait behind the calibration session.
                        enabled: backend.connected && backend.ready && !backend.calibrating
                        model: Math.max(backend.profileCount, 1)
                        currentIndex: backend.profile - 1
                        displayText: qsTr("Profile %1").arg(backend.profile)
                        delegate: ItemDelegate {
                            required property int index
                            width: ListView.view ? ListView.view.width : implicitWidth
                            text: qsTr("Profile %1").arg(index + 1)
                            highlighted: profileBox.highlightedIndex === index
                        }
                        onActivated: (index) => backend.setProfile(index + 1)
                    }

                    Item { implicitHeight: 18 }

                    Repeater {
                        model: [qsTr("Keys"), qsTr("Macros"), qsTr("Lighting"), qsTr("Settings")]
                        delegate: ItemDelegate {
                            required property int index
                            required property string modelData
                            Layout.fillWidth: true
                            text: modelData
                            highlighted: win.page === index
                            onClicked: win.page = index
                        }
                    }

                    Item { Layout.fillHeight: true }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Press ? for shortcuts")
                        opacity: 0.45
                        font.pixelSize: 11
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Unofficial tool, not affiliated with Akko.")
                        wrapMode: Text.WordWrap
                        opacity: 0.45
                        font.pixelSize: 11
                    }
                }
            }

            // ---- content -------------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                ConnectionBanner {
                    Layout.fillWidth: true
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: win.page

                    KeysPage { objectName: "keysPage" }
                    MacrosPage { objectName: "macrosPage" }
                    LightingPage {}
                    SettingsPage { objectName: "settingsPage" }
                }
            }
        }

        // ---- shortcut overlay --------------------------------------------------
        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: "#cc000000"

            MouseArea {
                anchors.fill: parent
                onClicked: win.helpVisible = false
            }

            Rectangle {
                anchors.centerIn: parent
                width: helpGrid.implicitWidth + 64
                height: helpGrid.implicitHeight + helpTitle.implicitHeight + 72
                radius: theme.radius
                color: "#1b1b20"

                Label {
                    id: helpTitle
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 32
                    text: qsTr("Shortcuts")
                    font.pixelSize: 18
                    font.bold: true
                }

                GridLayout {
                    id: helpGrid
                    anchors.top: helpTitle.bottom
                    anchors.left: parent.left
                    anchors.margins: 32
                    anchors.topMargin: 16
                    columns: 2
                    columnSpacing: 28
                    rowSpacing: 10

                    // Key, then what it does, filling the two columns row by row.
                    Repeater {
                        model: [
                            "Space", qsTr("Lights on or off"),
                            "Ctrl+1 … 3", qsTr("Switch profile"),
                            "Ctrl+R", qsTr("Read the keyboard again"),
                            "?", qsTr("Show or hide shortcuts"),
                            "Q", qsTr("Quit")
                        ]
                        delegate: Label {
                            required property int index
                            required property string modelData
                            text: modelData
                            font.family: index % 2 === 0 ? "monospace" : Qt.application.font.family
                            color: index % 2 === 0 ? theme.accent : Material.foreground
                        }
                    }
                }
            }
        }
    }
}
