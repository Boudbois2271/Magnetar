// SPDX-License-Identifier: GPL-3.0-or-later
// The colour picker's helpers, for QML as `colorPalette`: hue/saturation/value ↔ "#rrggbb", and
// the user's saved colours. Saved colours are an app preference, not board data: they live in
// an INI file (QSettings), one list of slots, "" for an empty one.
#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariantList>

class ColorPalette : public QObject {
    Q_OBJECT
    // Every slot in order, kSlotCount of them: "#rrggbb" or "" when empty.
    Q_PROPERTY(QStringList saved READ saved NOTIFY savedChanged)
    Q_PROPERTY(int slotCount READ slotCount CONSTANT)

public:
    static constexpr int kSlotCount = 20;  // two rows of ten

    // `file`: the settings file to keep the saved colours in.
    explicit ColorPalette(const QString &file, QObject *parent = nullptr);

    QStringList saved() const { return m_saved; }
    int slotCount() const { return kSlotCount; }

    // Stores `color` in slot `index` (replacing what was there). False, and nothing stored, for
    // an index out of range or something that isn't a colour.
    Q_INVOKABLE bool save(int index, const QString &color);
    Q_INVOKABLE void remove(int index);

    // h, s, v in 0…1 → "#rrggbb".
    Q_INVOKABLE static QString hsvToHex(double h, double s, double v);
    // "#rrggbb" → [h, s, v], each 0…1. A grey has no hue and black no saturation either: those
    // keep `hue` and `saturation`, so the picker's marker and hue don't jump. Empty for
    // something that isn't a colour.
    Q_INVOKABLE static QVariantList hexToHsv(const QString &color, double hue = 0, double saturation = 0);
    // "#rrggbb" in lower case, or "" for something that isn't a colour ("#abc", "ff0000" and
    // "#FF0000" are colours).
    Q_INVOKABLE static QString normalized(const QString &color);

signals:
    void savedChanged();

private:
    void store();

    QSettings m_settings;
    QStringList m_saved;
};
