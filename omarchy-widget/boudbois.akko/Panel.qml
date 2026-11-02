// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Ui

// Akko TAC75 HE: onboard profile picker, lighting on/off, brightness and
// colour. All device I/O goes through ~/.local/bin/akkoctl, which prints the
// keyboard state as JSON after every command.
Panel {
  id: root
  moduleName: "boudbois.akko"
  ipcTarget: "boudbois.akko"

  property var kb: ({ connected: false })
  property var queue: []
  property bool cursorActive: false
  property int profileIndex: 0

  readonly property bool connected: kb.connected === true
  readonly property bool lightsOn: connected && kb.on === true
  readonly property int profileCount: connected ? (kb.profileCount || 3) : 0
  readonly property bool showColor: connected && kb.profile === 1
  readonly property string akkoctl: Quickshell.env("HOME") + "/.local/bin/akkoctl"
  readonly property string glyph: "󰌌"

  readonly property var swatches: [
    "#ff0000", "#ff6a00", "#ffd000", "#00ff00", "#00ffd0",
    "#00a0ff", "#0000ff", "#8000ff", "#ff00c0", "#ffffff"
  ]

  readonly property string statusText: {
    if (!connected) return "NOT CONNECTED"
    return "PROFILE " + kb.profile + " · " + (lightsOn ? "LIGHTS ON" : "LIGHTS OFF")
  }

  // ---------- akkoctl plumbing ----------
  // One process at a time; a newer brightness request replaces a queued one
  // so dragging the slider never builds a backlog.
  function run(args) {
    var q = queue.slice()
    if (args[0] === "brightness") q = q.filter(function(a) { return a[0] !== "brightness" })
    q.push(args)
    queue = q
    pump()
  }

  function pump() {
    if (proc.running || queue.length === 0) return
    var next = queue[0]
    queue = queue.slice(1)
    proc.command = [root.akkoctl].concat(next)
    proc.running = true
  }

  function refresh() { if (queue.length === 0) run(["status"]) }

  function apply(text) {
    try {
      var parsed = JSON.parse(String(text || "").trim())
      if (parsed && typeof parsed === "object") kb = parsed
    } catch (e) {
      kb = { connected: false, error: "akkoctl output unreadable" }
    }
  }

  function setProfile(n) { if (connected && n !== kb.profile) run(["profile", String(n)]) }
  function setBrightness(v) { run(["brightness", String(Math.round(v))]) }
  function setColor(c) { run(["color", c]) }
  function toggleLights() { if (connected) run(["toggle"]) }

  function sameColor(a, b) { return String(a).toLowerCase() === String(b).toLowerCase() }

  Process {
    id: proc
    stdout: StdioCollector { waitForEnd: true; onStreamFinished: root.apply(text) }
    onExited: Qt.callLater(root.pump)
  }

  Component.onCompleted: refresh()

  onOpenedChanged: {
    if (opened) {
      refresh()
      cursorActive = false
      profileIndex = Math.max(0, (kb.profile || 1) - 1)
    }
  }

  // Fast poll while open picks up Fn-key changes; slow poll keeps the bar icon honest.
  Timer { interval: root.opened ? 3000 : 30000; running: true; repeat: true; onTriggered: root.refresh() }

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: root.glyph
    opacity: root.lightsOn ? 1.0 : 0.5
    tooltipText: root.connected ? "" : "Akko keyboard not connected"
    onPressed: function(b) {
      if (b === Qt.RightButton) root.toggleLights()
      else root.toggle()
    }
  }

  KeyboardPanel {
    id: panel
    anchorItem: button
    owner: root
    bar: root.bar
    open: root.opened
    focusTarget: keyCatcher
    contentWidth: panel.fittedContentWidth(Style.space(360))
    contentHeight: panel.fittedContentHeight(column.implicitHeight)

    PanelKeyCatcher {
      id: keyCatcher
      anchors.fill: parent
      onMoveRequested: function(dx, dy) {
        if (!root.connected) return
        if (!root.cursorActive) { root.cursorActive = true; return }
        var d = dx !== 0 ? dx : dy
        root.profileIndex = Math.max(0, Math.min(root.profileCount - 1, root.profileIndex + d))
      }
      onActivateRequested: if (root.cursorActive) root.setProfile(root.profileIndex + 1)
      onCloseRequested: root.close()
      onTabRequested: function(direction) { root.switchPanel(direction) }
      onTextKey: function(t) {
        if (t === "l" || t === "L") root.toggleLights()
        else if (t >= "1" && t <= "9" && Number(t) <= root.profileCount) root.setProfile(Number(t))
      }

      Column {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Style.space(14)

        // ---------- Hero: icon · name/status · lights switch ----------
        Item {
          width: parent.width
          implicitHeight: Math.max(heroIcon.implicitHeight, heroLabels.implicitHeight, lightSwitch.implicitHeight)

          Text {
            id: heroIcon
            textFormat: Text.PlainText
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: root.glyph
            color: root.lightsOn && !root.kb.rainbow ? root.kb.color : root.bar.foreground
            font.family: root.bar.fontFamily
            font.pixelSize: Style.font.display
            opacity: root.lightsOn ? 1.0 : 0.5
            Behavior on color { ColorAnimation { duration: 200 } }
          }

          ToggleSwitch {
            id: lightSwitch
            visible: root.connected
            checked: root.lightsOn
            busy: proc.running
            foreground: root.bar.foreground
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            onToggled: root.toggleLights()

            PanelToolTip {
              visible: lightSwitch.containsMouse
              text: root.lightsOn ? "Turn lighting off" : "Turn lighting on"
              fontFamily: root.bar.fontFamily
            }
          }

          Column {
            id: heroLabels
            anchors.left: heroIcon.right
            anchors.leftMargin: Style.space(14)
            anchors.right: parent.right
            anchors.rightMargin: lightSwitch.visible ? lightSwitch.width + Style.space(12) : 0
            anchors.verticalCenter: parent.verticalCenter
            spacing: Style.space(2)

            Text {
              text: "Akko TAC75 HE"
              color: root.bar.foreground
              font.family: root.bar.fontFamily
              font.pixelSize: Style.font.title
              font.bold: true
              elide: Text.ElideRight
              width: parent.width
            }

            Text {
              textFormat: Text.PlainText
              text: root.statusText
              color: Qt.darker(root.bar.foreground, 1.4)
              font.family: root.bar.fontFamily
              font.pixelSize: Style.font.caption
              font.bold: true
              font.letterSpacing: 1.2
              elide: Text.ElideRight
              width: parent.width
            }
          }
        }

        Text {
          visible: !root.connected
          width: parent.width
          wrapMode: Text.WordWrap
          textFormat: Text.PlainText
          text: root.kb.error ? String(root.kb.error) : "Waiting for keyboard…"
          color: root.bar.foreground
          opacity: 0.6
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.bodySmall
        }

        // ---------- Profiles ----------
        PanelSeparator { visible: root.connected; foreground: root.bar.foreground }

        Column {
          visible: root.connected
          width: parent.width
          spacing: Style.space(10)

          PanelSectionHeader {
            text: "PROFILE"
            foreground: root.bar.foreground
            fontFamily: root.bar.fontFamily
          }

          Row {
            id: profileRow
            width: parent.width
            spacing: Style.space(6)
            readonly property real cellWidth: root.profileCount > 0
              ? (width - spacing * (root.profileCount - 1)) / root.profileCount : 0

            Repeater {
              model: root.profileCount
              Button {
                required property int index
                width: profileRow.cellWidth
                text: String(index + 1)
                fontSize: Style.font.body
                foreground: root.bar.foreground
                fontFamily: root.bar.fontFamily
                verticalPadding: Style.spacing.controlPaddingY + Style.space(2)
                bordered: true
                active: root.kb.profile === index + 1
                hasCursor: root.cursorActive && root.profileIndex === index
                onClicked: root.setProfile(index + 1)
                onHovered: function(h) { if (h) { root.cursorActive = true; root.profileIndex = index } }
              }
            }
          }
        }

        // ---------- Brightness ----------
        PanelSeparator { visible: root.connected; foreground: root.bar.foreground }

        Column {
          visible: root.connected
          width: parent.width
          spacing: Style.space(10)

          Item {
            width: parent.width
            implicitHeight: brightnessHeader.implicitHeight

            PanelSectionHeader {
              id: brightnessHeader
              text: "BRIGHTNESS"
              foreground: root.bar.foreground
              fontFamily: root.bar.fontFamily
            }

            Text {
              anchors.right: parent.right
              anchors.verticalCenter: parent.verticalCenter
              textFormat: Text.PlainText
              text: Math.round(brightnessSlider.liveValue / (root.kb.maxBrightness || 20) * 100) + "%"
              color: root.bar.foreground
              opacity: 0.6
              font.family: root.bar.fontFamily
              font.pixelSize: Style.font.bodySmall
            }
          }

          PanelSlider {
            id: brightnessSlider
            bar: root.bar
            width: parent.width
            minimum: 0
            maximum: root.kb.maxBrightness || 20
            step: 1
            integer: true
            value: root.kb.brightness || 0
            opacity: root.lightsOn ? 1.0 : 0.5
            onMoved: function(v) { root.setBrightness(v) }
            onRightClicked: root.toggleLights()
          }
        }

        // ---------- Colour (profile 1 only: the other profiles keep their own lighting) ----------
        PanelSeparator { visible: root.showColor; foreground: root.bar.foreground }

        Column {
          visible: root.showColor
          width: parent.width
          spacing: Style.space(10)

          PanelSectionHeader {
            text: "COLOR"
            foreground: root.bar.foreground
            fontFamily: root.bar.fontFamily
          }

          Grid {
            id: swatchGrid
            width: parent.width
            columns: 6
            spacing: Style.space(8)
            readonly property real cell: (width - spacing * (columns - 1)) / columns

            Repeater {
              model: root.swatches.concat(["rainbow"])
              Item {
                required property string modelData
                readonly property bool isRainbow: modelData === "rainbow"
                readonly property bool selected: isRainbow ? root.kb.rainbow === true
                                                           : (!root.kb.rainbow && root.sameColor(root.kb.color, modelData))
                width: swatchGrid.cell
                height: Style.space(26)

                Rectangle {
                  anchors.fill: parent
                  radius: Style.cornerRadius > 0 ? height / 2 : 0
                  color: isRainbow ? "transparent" : modelData
                  border.width: selected ? 2 : (swatchMouse.containsMouse ? 1 : 0)
                  border.color: root.bar.foreground
                  gradient: isRainbow ? rainbowGradient : null
                }

                Text {
                  visible: isRainbow
                  anchors.centerIn: parent
                  text: "󰸌"
                  color: "white"
                  font.family: root.bar.fontFamily
                  font.pixelSize: Style.font.bodySmall
                  style: Text.Outline
                  styleColor: "#40000000"
                }

                MouseArea {
                  id: swatchMouse
                  anchors.fill: parent
                  hoverEnabled: true
                  cursorShape: Qt.PointingHandCursor
                  onClicked: root.setColor(modelData)
                }

                PanelToolTip {
                  visible: swatchMouse.containsMouse
                  text: isRainbow ? "Rainbow" : modelData
                  fontFamily: root.bar.fontFamily
                }
              }
            }
          }

          Row {
            width: parent.width
            spacing: Style.space(8)

            Rectangle {
              width: Style.space(26)
              height: hexField.height
              radius: Style.cornerRadius > 0 ? Style.space(4) : 0
              color: /^#?[0-9a-fA-F]{6}$/.test(hexField.text) ? (hexField.text[0] === "#" ? hexField.text : "#" + hexField.text) : root.kb.color || "transparent"
              border.width: 1
              border.color: Util.alpha(root.bar.foreground, 0.3)
            }

            TextField {
              id: hexField
              width: parent.width - Style.space(26) - parent.spacing * 2 - applyButton.width
              placeholderText: root.kb.color || "#rrggbb"
              foreground: root.bar.foreground
              font.family: root.bar.fontFamily
              maximumLength: 7
              onAccepted: applyButton.clicked()
            }

            Button {
              id: applyButton
              text: "Set"
              enabled: /^#?[0-9a-fA-F]{6}$/.test(hexField.text)
              opacity: enabled ? 1.0 : 0.4
              bordered: true
              foreground: root.bar.foreground
              fontFamily: root.bar.fontFamily
              fontSize: Style.font.bodySmall
              onClicked: {
                if (!enabled) return
                var c = hexField.text[0] === "#" ? hexField.text : "#" + hexField.text
                root.setColor(c.toLowerCase())
                hexField.text = ""
              }
            }
          }
        }
      }
    }
  }

  Gradient {
    id: rainbowGradient
    orientation: Gradient.Horizontal
    GradientStop { position: 0.0; color: "#ff0000" }
    GradientStop { position: 0.2; color: "#ffd000" }
    GradientStop { position: 0.4; color: "#00ff00" }
    GradientStop { position: 0.6; color: "#00c0ff" }
    GradientStop { position: 0.8; color: "#4000ff" }
    GradientStop { position: 1.0; color: "#ff00c0" }
  }
}
