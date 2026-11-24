// SPDX-License-Identifier: GPL-3.0-or-later
#include "color_palette.h"

#include <QColor>
#include <QRegularExpression>
#include <algorithm>

namespace {
const char *kSavedKey = "colors/saved";
}

ColorPalette::ColorPalette(const QString &file, QObject *parent)
    : QObject(parent), m_settings(file, QSettings::IniFormat)
{
    const QStringList stored = m_settings.value(kSavedKey).toStringList();
    for (int i = 0; i < kSlotCount; ++i)
        m_saved << (i < stored.size() ? normalized(stored[i]) : QString());
}

bool ColorPalette::save(int index, const QString &color)
{
    const QString c = normalized(color);
    if (index < 0 || index >= kSlotCount || c.isEmpty())
        return false;
    if (m_saved[index] != c) {
        m_saved[index] = c;
        store();
    }
    return true;
}

void ColorPalette::remove(int index)
{
    if (index < 0 || index >= kSlotCount || m_saved[index].isEmpty())
        return;
    m_saved[index].clear();
    store();
}

void ColorPalette::store()
{
    m_settings.setValue(kSavedKey, m_saved);
    m_settings.sync();
    emit savedChanged();
}

QString ColorPalette::hsvToHex(double h, double s, double v)
{
    const auto clamp = [](double x) { return std::clamp(x, 0.0, 1.0); };
    // Hue 1 is hue 0 again (red); QColor wants it below 1.
    const double hue = clamp(h) >= 1 ? 0 : clamp(h);
    return QColor::fromHsvF(float(hue), float(clamp(s)), float(clamp(v))).name();
}

QVariantList ColorPalette::hexToHsv(const QString &color, double hue, double saturation)
{
    const QString c = normalized(color);
    if (c.isEmpty())
        return {};
    float h, s, v;
    QColor::fromString(c).getHsvF(&h, &s, &v);
    if (v == 0)
        s = float(saturation);
    if (h < 0 || s == 0 || v == 0)  // QColor gives -1 for a grey
        h = float(hue);
    return {double(h), double(s), double(v)};
}

QString ColorPalette::normalized(const QString &color)
{
    static const QRegularExpression hex("^#?([0-9a-fA-F]{3}|[0-9a-fA-F]{6})$");
    const auto m = hex.match(color.trimmed());
    if (!m.hasMatch())
        return {};
    QString digits = m.captured(1).toLower();
    if (digits.size() == 3)
        digits = QString(digits[0]) + digits[0] + digits[1] + digits[1] + digits[2] + digits[2];
    return "#" + digits;
}
