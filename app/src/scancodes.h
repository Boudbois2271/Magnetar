// SPDX-License-Identifier: GPL-3.0-or-later
// Linux key codes (evdev, linux/input-event-codes.h) to HID keyboard usages, so a macro is
// recorded by physical key whatever the keyboard layout. Plain C++, no Qt.
#pragma once

#include <cstdint>

// HID usage for an evdev key code (modifiers → e0–e7), 0 for a key with no keyboard usage.
std::uint8_t usageForEvdev(int code);

// Qt's nativeScanCode() on Linux (xcb and Wayland) is the XKB keycode: evdev code + 8.
constexpr int kXkbKeycodeOffset = 8;
