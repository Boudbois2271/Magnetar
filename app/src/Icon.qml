// SPDX-License-Identifier: GPL-3.0-or-later
// A Lucide icon from the qrc (icons/<name>.svg), tinted with `color`. IconImage (the one the
// Controls themselves use) tints on the CPU, so it also works offscreen and in the tests.
import QtQuick
import QtQuick.Controls.impl

Item {
    id: icon
    property string name
    property color color: theme.foreground
    property real size: 16 * ui.unit

    implicitWidth: size
    implicitHeight: size

    IconImage {
        anchors.fill: parent
        color: icon.color
        source: icon.name ? "qrc:/icons/" + icon.name + ".svg" : ""
        sourceSize: Qt.size(icon.size * Screen.devicePixelRatio, icon.size * Screen.devicePixelRatio)
    }
}
