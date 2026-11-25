// SPDX-License-Identifier: GPL-3.0-or-later
// Names for what a key does, from backend.actionInfo(): HID keyboard usages, modifiers,
// media keys, mouse buttons, layers. Names that read the same in every language aren't
// marked for translation.
import QtQuick

QtObject {
    id: names

    // HID keyboard page usage → name, as printed on the keycaps (US layout).
    function usageName(u) {
        if (u >= 0x04 && u <= 0x1d)
            return String.fromCharCode(0x41 + u - 0x04)  // A–Z
        if (u >= 0x1e && u <= 0x26)
            return String(u - 0x1d)  // 1–9
        if (u >= 0x3a && u <= 0x45)
            return "F" + (u - 0x39)
        if (u >= 0x68 && u <= 0x73)
            return "F" + (u - 0x68 + 13)
        if (u >= 0xe0 && u <= 0xe7)
            return modifierName(1 << (u - 0xe0))  // modifier usages, as in macro steps
        if (u >= 0x59 && u <= 0x61)
            return qsTr("Num %1").arg(u - 0x58)
        switch (u) {
        case 0x27: return "0"
        case 0x28: return qsTr("Enter")
        case 0x29: return qsTr("Esc")
        case 0x2a: return qsTr("Backspace")
        case 0x2b: return "Tab"
        case 0x2c: return qsTr("Space")
        case 0x2d: return "-"
        case 0x2e: return "="
        case 0x2f: return "["
        case 0x30: return "]"
        case 0x31: return "\\"
        case 0x33: return ";"
        case 0x34: return "'"
        case 0x35: return "`"
        case 0x36: return ","
        case 0x37: return "."
        case 0x38: return "/"
        case 0x39: return qsTr("Caps Lock")
        case 0x46: return qsTr("Print Screen")
        case 0x47: return qsTr("Scroll Lock")
        case 0x48: return "Pause"
        case 0x49: return qsTr("Insert")
        case 0x4a: return qsTr("Home")
        case 0x4b: return qsTr("Page Up")
        case 0x4c: return qsTr("Delete")
        case 0x4d: return qsTr("End")
        case 0x4e: return qsTr("Page Down")
        case 0x4f: return "→"
        case 0x50: return "←"
        case 0x51: return "↓"
        case 0x52: return "↑"
        case 0x53: return qsTr("Num Lock")
        case 0x54: return qsTr("Num %1").arg("/")
        case 0x55: return qsTr("Num %1").arg("*")
        case 0x56: return qsTr("Num %1").arg("-")
        case 0x57: return qsTr("Num %1").arg("+")
        case 0x58: return qsTr("Num Enter")
        case 0x62: return qsTr("Num %1").arg("0")
        case 0x63: return qsTr("Num %1").arg(".")
        case 0x65: return "Menu"
        }
        return "0x" + u.toString(16)
    }

    // A single modifier key, by its bit (01 left Ctrl … 80 right Win).
    function modifierName(bit) {
        switch (bit) {
        case 0x01: return qsTr("Left Ctrl")
        case 0x02: return qsTr("Left Shift")
        case 0x04: return qsTr("Left Alt")
        case 0x08: return qsTr("Left Win")
        case 0x10: return qsTr("Right Ctrl")
        case 0x20: return qsTr("Right Shift")
        case 0x40: return qsTr("Right Alt")
        case 0x80: return qsTr("Right Win")
        }
        return ""
    }

    // Modifiers inside a shortcut: "Ctrl+Shift", sides only named for the right ones.
    function modifierPrefix(mods) {
        const short = ["Ctrl", qsTr("Shift"), "Alt", "Win"]
        let parts = []
        for (let i = 0; i < 4; ++i) {
            if (mods & (1 << i))
                parts.push(short[i])
            if (mods & (0x10 << i))
                parts.push(qsTr("Right %1").arg(short[i]))
        }
        return parts.join("+")
    }

    // Consumer usages offered and found in the factory Fn layer.
    readonly property var media: [
        { usage: 0xe9, name: qsTr("Volume up") },
        { usage: 0xea, name: qsTr("Volume down") },
        { usage: 0xe2, name: qsTr("Mute") },
        { usage: 0xcd, name: qsTr("Play / pause") },
        { usage: 0xb5, name: qsTr("Next track") },
        { usage: 0xb6, name: qsTr("Previous track") },
        { usage: 0xb7, name: qsTr("Stop playback") },
        { usage: 0x183, name: qsTr("Media player") },
        { usage: 0x18a, name: qsTr("Mail") },
        { usage: 0x192, name: qsTr("Calculator") },
        { usage: 0x194, name: qsTr("File manager") },
        { usage: 0x221, name: qsTr("Search") },
        { usage: 0x223, name: qsTr("Browser home") },
        { usage: 0x22a, name: qsTr("Bookmarks") }
    ]

    function consumerName(u) {
        for (const m of media)
            if (m.usage === u)
                return m.name
        return qsTr("Media key %1").arg("0x" + u.toString(16))
    }

    function mouseName(b) {
        switch (b) {
        case 1: return qsTr("Left click")
        case 2: return qsTr("Right click")
        case 3: return qsTr("Middle click")
        case 4: return qsTr("Back (mouse)")
        case 5: return qsTr("Forward (mouse)")
        }
        return qsTr("Mouse button %1").arg(b)
    }

    // Lighting control keys (08 gg oo 00), by target (gg) then step (oo), as the vendor lists them.
    readonly property var lighting: [
        [qsTr("Effect cycle"), qsTr("Effect +"), qsTr("Effect −")],
        [qsTr("Direction cycle"), qsTr("Direction left"), qsTr("Direction right")],
        [qsTr("Color cycle"), qsTr("Color +"), qsTr("Color −")],
        [qsTr("Brightness cycle"), qsTr("Brightness +"), qsTr("Brightness −")],
        [qsTr("Speed cycle"), qsTr("Speed +"), qsTr("Speed −")]
    ]

    function lightingName(value) {
        const row = lighting[value >> 8]
        return row && row[value & 0xff] !== undefined ? row[value & 0xff] : qsTr("Lighting control")
    }

    // Shorter names for the keycaps of the drawing; anything else uses name().
    function capName(info) {
        if (info && info.kind === "key" && info.keys.length === 1 && info.modifiers === 0) {
            switch (info.keys[0]) {
            case 0x39: return qsTr("Caps")
            case 0x46: return qsTr("PrtSc")
            case 0x47: return qsTr("ScrLk")
            case 0x49: return qsTr("Ins")
            case 0x4a: return qsTr("Home")
            case 0x4b: return qsTr("PgUp")
            case 0x4c: return qsTr("Del")
            case 0x4e: return qsTr("PgDn")
            case 0x53: return qsTr("NumLk")
            }
        }
        if (info && info.kind === "key" && info.keys.length === 0) {
            // A lone modifier: its place on the board already says which side.
            switch (info.modifiers) {
            case 0x01: case 0x10: return "Ctrl"
            case 0x02: case 0x20: return qsTr("Shift")
            case 0x04: return "Alt"
            case 0x40: return qsTr("Alt Gr")
            case 0x08: case 0x80: return "Win"
            }
        }
        if (info && info.kind === "macro") {
            const m = backend.macros[info.value]
            return m ? m.name : qsTr("Macro %1").arg(info.value + 1)
        }
        return name(info)
    }

    // A macro by its index on the active profile: "Macro: name", or its number if it isn't there.
    function macroName(i) {
        const m = backend.macros[i]
        return m ? qsTr("Macro: %1").arg(m.name) : qsTr("Macro %1").arg(i + 1)
    }

    // Full name of an action info object.
    function name(info) {
        if (!info || !info.kind)
            return ""
        switch (info.kind) {
        case "key": {
            const keys = info.keys.map(usageName)
            if (info.modifiers === 0)
                return keys.join("+")
            if (keys.length === 0 && [1, 2, 4, 8, 16, 32, 64, 128].indexOf(info.modifiers) >= 0)
                return modifierName(info.modifiers)
            return [modifierPrefix(info.modifiers)].concat(keys).join("+")
        }
        case "mouse": return mouseName(info.value)
        case "consumer": return consumerName(info.value)
        case "macro": {
            const what = macroName(info.value)
            switch (info.loop) {
            case "times": return info.count > 1 ? qsTr("%1 ×%2").arg(what).arg(info.count) : what
            case "untilKeyPressed": return qsTr("%1, until a key is pressed").arg(what)
            case "whileHeld": return qsTr("%1, while held").arg(what)
            }
            return what
        }
        case "layer": return info.value === 0 ? "Fn" : "Fn" + info.value
        case "profile": return qsTr("Profile %1").arg(info.value + 1)
        case "lighting": return info.code.endsWith(" 00") ? lightingName(info.value) : qsTr("Lighting control")
        case "special": return info.code === "11 00 00 00" ? "www" : info.code
        case "system": return qsTr("System function")
        case "unbound": return qsTr("Nothing")
        }
        return info.code
    }
}
