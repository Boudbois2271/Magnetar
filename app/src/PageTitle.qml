// SPDX-License-Identifier: GPL-3.0-or-later
// A page's title: one font for every page, set in PageLayout's header row.
import QtQuick
import QtQuick.Controls

Label {
    font.pixelSize: ui.title
    font.weight: Font.DemiBold
}
