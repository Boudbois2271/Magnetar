// SPDX-License-Identifier: GPL-3.0-or-later
#include "color_palette.h"

#include <QColor>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

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

namespace {
double toLinear(double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); }
double toGamma(double v) { return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - 0.055; }

// WCAG relative luminance.
double luminance(const QColor &c)
{
    return 0.2126 * toLinear(c.redF()) + 0.7152 * toLinear(c.greenF()) + 0.0722 * toLinear(c.blueF());
}

// OKLab (Björn Ottosson), from and to linear sRGB.
struct Lab { double l, a, b; };

Lab toOklab(const QColor &c)
{
    const double r = toLinear(c.redF()), g = toLinear(c.greenF()), b = toLinear(c.blueF());
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

// Linear sRGB of an OKLab colour; false when it falls outside sRGB.
bool fromOklab(const Lab &c, double rgb[3])
{
    const double l = std::pow(c.l + 0.3963377774 * c.a + 0.2158037573 * c.b, 3);
    const double m = std::pow(c.l - 0.1055613458 * c.a - 0.0638541728 * c.b, 3);
    const double s = std::pow(c.l - 0.0894841775 * c.a - 1.2914855480 * c.b, 3);
    rgb[0] = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    rgb[1] = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    rgb[2] = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
    constexpr double e = 1e-6;
    return std::all_of(rgb, rgb + 3, [](double v) { return v >= -e && v <= 1 + e; });
}

// Lightness `l` at hue (a, b) scaled by the largest chroma factor ≤ 1 that stays in sRGB.
QColor atLightness(double l, double a, double b)
{
    double rgb[3];
    double lo = 0, hi = 1;
    if (!fromOklab({l, a, b}, rgb)) {
        for (int i = 0; i < 30; ++i) {
            const double k = (lo + hi) / 2;
            (fromOklab({l, a * k, b * k}, rgb) ? lo : hi) = k;
        }
        fromOklab({l, a * lo, b * lo}, rgb);
    }
    const auto ch = [](double v) { return float(std::clamp(toGamma(std::clamp(v, 0.0, 1.0)), 0.0, 1.0)); };
    return QColor::fromRgbF(ch(rgb[0]), ch(rgb[1]), ch(rgb[2]));
}
}  // namespace

double ColorPalette::contrast(const QColor &a, const QColor &b)
{
    const double x = luminance(a), y = luminance(b);
    return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05);
}

QColor ColorPalette::inkOn(const QColor &fill)
{
    const QColor softDark("#0e0e10"), softLight("#f0f0f0");
    const bool dark = contrast(fill, softDark) >= contrast(fill, softLight);
    const QColor soft = dark ? softDark : softLight;
    if (contrast(fill, soft) >= kLegible)
        return soft;
    return QColor(dark ? "#000000" : "#ffffff");
}

QColor ColorPalette::legibleOn(const QColor &led, const QColor &face)
{
    if (contrast(led, face) >= kLegible)
        return led;
    const Lab c = toOklab(led);
    // Away from the face: up on a dark one, down on a light one (the end with more room).
    const bool up = contrast(face, Qt::white) >= contrast(face, Qt::black);
    double near = c.l, far = up ? 1 : 0;
    if (contrast(atLightness(far, c.a, c.b), face) < kLegible)
        return atLightness(far, c.a, c.b);  // can't reach it: as far as the hue goes
    for (int i = 0; i < 30; ++i) {
        const double l = (near + far) / 2;
        (contrast(atLightness(l, c.a, c.b), face) >= kLegible ? far : near) = l;
    }
    return atLightness(far, c.a, c.b);
}

double ColorPalette::oklchHue(const QColor &color)
{
    const Lab c = toOklab(color);
    return std::fmod(std::atan2(c.b, c.a) * 180 / M_PI + 360, 360);
}
