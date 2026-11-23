// SPDX-License-Identifier: GPL-3.0-or-later
// A button for picking among many: a secondary button, filled with the accent colour when `on`.
import QtQuick

AppButton {
    property bool on: false
    kind: on ? "primary" : "secondary"
}
