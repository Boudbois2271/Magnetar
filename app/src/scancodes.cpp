// SPDX-License-Identifier: GPL-3.0-or-later
#include "scancodes.h"

#include <array>

namespace {

// Index = evdev code; the mapping is the inverse of the kernel's HID keyboard table.
constexpr auto kTable = [] {
    std::array<std::uint8_t, 195> t{};
    auto run = [&](int code, std::uint8_t usage, int n) {
        for (int i = 0; i < n; ++i)
            t[code + i] = static_cast<std::uint8_t>(usage + i);
    };
    t[1] = 0x29;              // Esc
    run(2, 0x1e, 9);          // 1–9
    t[11] = 0x27;             // 0
    t[12] = 0x2d;             // -
    t[13] = 0x2e;             // =
    t[14] = 0x2a;             // Backspace
    t[15] = 0x2b;             // Tab
    const std::uint8_t qwerty[] = {0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13};
    for (int i = 0; i < 10; ++i)
        t[16 + i] = qwerty[i];  // Q–P
    t[26] = 0x2f;             // [
    t[27] = 0x30;             // ]
    t[28] = 0x28;             // Enter
    t[29] = 0xe0;             // Left Ctrl
    const std::uint8_t asdf[] = {0x04, 0x16, 0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f};
    for (int i = 0; i < 9; ++i)
        t[30 + i] = asdf[i];  // A–L
    t[39] = 0x33;             // ;
    t[40] = 0x34;             // '
    t[41] = 0x35;             // `
    t[42] = 0xe1;             // Left Shift
    t[43] = 0x31;             // backslash
    const std::uint8_t zxcv[] = {0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11, 0x10};
    for (int i = 0; i < 7; ++i)
        t[44 + i] = zxcv[i];  // Z–M
    t[51] = 0x36;             // ,
    t[52] = 0x37;             // .
    t[53] = 0x38;             // /
    t[54] = 0xe5;             // Right Shift
    t[55] = 0x55;             // Keypad *
    t[56] = 0xe2;             // Left Alt
    t[57] = 0x2c;             // Space
    t[58] = 0x39;             // Caps Lock
    run(59, 0x3a, 10);        // F1–F10
    t[69] = 0x53;             // Num Lock
    t[70] = 0x47;             // Scroll Lock
    run(71, 0x5f, 3);         // Keypad 7–9
    t[74] = 0x56;             // Keypad -
    run(75, 0x5c, 3);         // Keypad 4–6
    t[78] = 0x57;             // Keypad +
    run(79, 0x59, 3);         // Keypad 1–3
    t[82] = 0x62;             // Keypad 0
    t[83] = 0x63;             // Keypad .
    t[85] = 0x94;             // Zenkaku/Hankaku
    t[86] = 0x64;             // ISO \ (102nd key)
    t[87] = 0x44;             // F11
    t[88] = 0x45;             // F12
    t[89] = 0x87;             // Ro
    t[90] = 0x92;             // Katakana
    t[91] = 0x93;             // Hiragana
    t[92] = 0x8a;             // Henkan
    t[93] = 0x88;             // Katakana/Hiragana
    t[94] = 0x8b;             // Muhenkan
    t[95] = 0x8c;             // Keypad JP comma
    t[96] = 0x58;             // Keypad Enter
    t[97] = 0xe4;             // Right Ctrl
    t[98] = 0x54;             // Keypad /
    t[99] = 0x46;             // Print Screen
    t[100] = 0xe6;            // Right Alt
    t[102] = 0x4a;            // Home
    t[103] = 0x52;            // Up
    t[104] = 0x4b;            // Page Up
    t[105] = 0x50;            // Left
    t[106] = 0x4f;            // Right
    t[107] = 0x4d;            // End
    t[108] = 0x51;            // Down
    t[109] = 0x4e;            // Page Down
    t[110] = 0x49;            // Insert
    t[111] = 0x4c;            // Delete
    t[113] = 0x7f;            // Mute
    t[114] = 0x81;            // Volume Down
    t[115] = 0x80;            // Volume Up
    t[116] = 0x66;            // Power
    t[117] = 0x67;            // Keypad =
    t[119] = 0x48;            // Pause
    t[121] = 0x85;            // Keypad ,
    t[122] = 0x90;            // Hangeul
    t[123] = 0x91;            // Hanja
    t[124] = 0x89;            // Yen
    t[125] = 0xe3;            // Left Meta
    t[126] = 0xe7;            // Right Meta
    t[127] = 0x65;            // Menu (Compose)
    run(183, 0x68, 12);       // F13–F24
    return t;
}();

}  // namespace

std::uint8_t usageForEvdev(int code)
{
    return code >= 0 && code < int(kTable.size()) ? kTable[code] : 0;
}
