// SPDX-License-Identifier: GPL-3.0-or-later
// The window: sidebar (profile picker, pages), connection banner, the current page, shortcuts.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    // The smallest size where every page fits: the Keys page puts its panel beside the drawing.
    width: Math.round(1440 * ui.unit)
    height: Math.round(920 * ui.unit)
    minimumWidth: Math.round(900 * ui.unit)
    minimumHeight: Math.round(640 * ui.unit)
    visible: true
    title: "Magnetar"
    color: theme.background
    // The desktop's interface font at its text size; every control inherits it.
    font: ui.font

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.background: theme.background
    Material.foreground: theme.foreground
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property int page: 0

    // On-board Fn shortcuts can change the lighting behind our back: re-read on focus.
    onActiveChanged: if (active) backend.refresh()

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; enabled: win.helpVisible; onActivated: win.helpVisible = false }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    // Space presses the control reached with Tab; otherwise it switches the lights.
    readonly property bool keyboardFocus: !!activeFocusItem && (activeFocusItem.visualFocus === true
        || (activeFocusItem.visualFocus === undefined && activeFocusItem.activeFocusOnTab === true))
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: backend.connected && !win.keyboardFocus; onActivated: backend.toggleLights() }
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
                Layout.preferredWidth: 200 * ui.unit
                color: theme.sidebar

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18 * ui.unit
                    spacing: 4 * ui.unit

                    Label {
                        text: "Magnetar"
                        font.pixelSize: ui.title
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: "Akko TAC75 HE"
                        color: theme.secondary
                        font.pixelSize: ui.small
                    }
                    // Whether the keyboard answers; the banner over the page says why not.
                    RowLayout {
                        spacing: 6 * ui.unit
                        Rectangle {
                            implicitWidth: 7 * ui.unit
                            implicitHeight: implicitWidth
                            radius: width / 2
                            color: backend.connected ? theme.success : theme.error
                        }
                        Label {
                            objectName: "connectionStatus"
                            text: backend.connected ? qsTr("Connected")
                                  : backend.errorKind === "notFound" ? qsTr("Not found")
                                  : qsTr("Not connected")
                            color: theme.secondary
                            font.pixelSize: ui.small
                        }
                    }

                    Item { implicitHeight: 18 * ui.unit }

                    Label {
                        text: qsTr("Profile")
                        color: theme.secondary
                        font.pixelSize: ui.small
                    }
                    Segmented {
                        objectName: "profilePicker"
                        // A profile switch would wait behind the calibration session.
                        enabled: backend.connected && backend.ready && !backend.calibrating
                        model: Array.from({ length: Math.max(backend.profileCount, 1) }, (_, i) => String(i + 1))
                        currentIndex: backend.ready ? backend.profile - 1 : -1
                        onActivated: (index) => backend.setProfile(index + 1)
                    }

                    Item { implicitHeight: 18 * ui.unit }

                    Repeater {
                        model: [
                            { label: qsTr("Keys"), icon: "keyboard" },
                            { label: qsTr("Lighting"), icon: "lightbulb" },
                            { label: qsTr("Mural"), icon: "audio-lines" },
                            { label: qsTr("Macros"), icon: "clapperboard" },
                            { label: qsTr("Settings"), icon: "settings" }
                        ]
                        delegate: AbstractButton {
                            id: navItem
                            required property int index
                            required property var modelData
                            readonly property bool current: win.page === index
                            readonly property color tint: current ? theme.accent : theme.foreground

                            objectName: "nav_" + index
                            Layout.fillWidth: true
                            implicitHeight: 34 * ui.unit
                            text: modelData.label
                            hoverEnabled: true
                            onClicked: win.page = index

                            background: Rectangle {
                                radius: theme.radius
                                color: navItem.down || (navItem.hovered && !navItem.current) ? theme.chip : "transparent"
                                border.width: navItem.visualFocus ? 2 : 0
                                border.color: theme.accent
                                // The selected page's bar, on the leading edge.
                                Rectangle {
                                    visible: navItem.current
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 3 * ui.unit
                                    color: theme.accent
                                }
                            }
                            contentItem: RowLayout {
                                spacing: 10 * ui.unit
                                Item { implicitWidth: 3 * ui.unit }
                                Icon {
                                    name: navItem.modelData.icon
                                    color: navItem.tint
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: navItem.text
                                    color: navItem.tint
                                    font.weight: navItem.current ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }
                            }

                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    AppButton {
                        id: shortcutsButton
                        objectName: "shortcutsButton"
                        Layout.leftMargin: -12 * ui.unit
                        kind: "quiet"
                        text: qsTr("Shortcuts")
                        implicitWidth: shortcutsRow.implicitWidth + 24 * ui.unit
                        contentItem: Item {
                            RowLayout {
                                id: shortcutsRow
                                anchors.centerIn: parent
                                spacing: 8 * ui.unit
                                Icon { name: "keycap"; color: theme.secondary }
                                Label { text: shortcutsButton.text; color: theme.secondary }
                            }
                        }
                        onClicked: win.helpVisible = true
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
                    LightingPage { objectName: "lightingPage" }
                    MuralPage { objectName: "muralPage" }
                    MacrosPage { objectName: "macrosPage" }
                    SettingsPage { objectName: "settingsPage" }
                }
            }
        }

        // ---- shortcut overlay --------------------------------------------------
        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: Qt.rgba(0, 0, 0, theme.dark ? 0.8 : 0.45)

            MouseArea {
                anchors.fill: parent
                onClicked: win.helpVisible = false
            }

            Rectangle {
                anchors.centerIn: parent
                width: helpGrid.implicitWidth + 64 * ui.unit
                height: helpGrid.implicitHeight + helpTitle.implicitHeight + 72 * ui.unit
                radius: theme.radius
                color: theme.overlay

                Label {
                    id: helpTitle
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 32 * ui.unit
                    text: qsTr("Shortcuts")
                    font.pixelSize: ui.title
                    font.weight: Font.DemiBold
                }

                GridLayout {
                    id: helpGrid
                    anchors.top: helpTitle.bottom
                    anchors.left: parent.left
                    anchors.margins: 32 * ui.unit
                    anchors.topMargin: 16 * ui.unit
                    columns: 2
                    columnSpacing: 28 * ui.unit
                    rowSpacing: 10 * ui.unit

                    // Keys, then what they do. "+" between caps is a separator, not a cap.
                    readonly property var shortcuts: [
                        { keys: [qsTr("Space")], label: qsTr("Lights on or off") },
                        { keys: ["Ctrl", "+", "1", "2", "3"], label: qsTr("Switch profile") },
                        { keys: ["Ctrl", "+", "R"], label: qsTr("Read the keyboard again") },
                        { keys: ["?"], label: qsTr("Show or hide shortcuts") },
                        { keys: ["Q"], label: qsTr("Quit") }
                    ]

                    Repeater {
                        model: helpGrid.shortcuts
                        delegate: Row {
                            id: helpRow
                            required property int index
                            required property var modelData
                            Layout.row: index
                            Layout.column: 0
                            spacing: 4 * ui.unit

                            Repeater {
                                model: helpRow.modelData.keys
                                delegate: Item {
                                    id: cap
                                    required property string modelData
                                    readonly property bool separator: modelData === "+"
                                    readonly property real side: Math.max(1, height * 0.08)
                                    implicitHeight: 28 * ui.unit
                                    implicitWidth: separator ? legend.implicitWidth
                                                   : Math.max(implicitHeight, legend.implicitWidth + 14 * ui.unit)

                                    Rectangle {  // skirt
                                        visible: !cap.separator
                                        anchors.fill: parent
                                        radius: Math.min(theme.radius, cap.height * 0.2)
                                        color: theme.capSkirt
                                    }
                                    Rectangle {  // top face
                                        id: face
                                        visible: !cap.separator
                                        anchors.fill: parent
                                        anchors.leftMargin: cap.side
                                        anchors.rightMargin: cap.side
                                        anchors.topMargin: cap.side * 0.5
                                        anchors.bottomMargin: cap.side * 1.8
                                        radius: Math.max(0, Math.min(theme.radius, cap.height * 0.2) - cap.side)
                                        color: theme.capTop
                                    }
                                    Label {
                                        id: legend
                                        anchors.centerIn: cap.separator ? parent : face
                                        text: cap.modelData
                                        color: cap.separator ? theme.secondary : theme.foreground
                                        font.pixelSize: ui.small
                                    }
                                }
                            }
                        }
                    }
                    Repeater {
                        model: helpGrid.shortcuts
                        delegate: Label {
                            required property int index
                            required property var modelData
                            Layout.row: index
                            Layout.column: 1
                            Layout.alignment: Qt.AlignVCenter
                            text: modelData.label
                        }
                    }
                }
            }
        }
    }
}
