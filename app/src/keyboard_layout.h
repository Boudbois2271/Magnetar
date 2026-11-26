// SPDX-License-Identifier: GPL-3.0-or-later
// What the typing keys print in the desktop's keyboard layout (AZERTY: the Q position types "A"),
// for the keycap labels and action names. Display only: the HID usages stored and sent never change.
#pragma once

#include <QObject>
#include <QStringList>

class KeyboardLayout : public QObject {
    Q_OBJECT
    // Index = HID keyboard usage (0–255): the character it types, "" where the US name stays.
    Q_PROPERTY(QStringList labels READ labels NOTIFY changed)
    // XKB layout name of the first group ("French", "English (US)"), "" when none was read.
    Q_PROPERTY(QString name READ name NOTIFY changed)

public:
    explicit KeyboardLayout(QObject *parent = nullptr);

    QStringList labels() const { return m_labels; }
    QString name() const { return m_name; }
    QString label(int usage) const { return m_labels.value(usage); }

    // The keymap the Wayland compositor gives this app's seat, else one built from the
    // XKB_DEFAULT_* variables; false (US names) when neither is there.
    bool loadSystem();
    // A keymap in XKB text format, as the compositor sends it.
    bool loadKeymap(const QByteArray &text);
    // A keymap built from XKB rule names ("fr", "us"…), so tests don't depend on the machine.
    bool loadNames(const QString &layout, const QString &variant = {});

signals:
    void changed();

private:
    bool load(struct xkb_keymap *keymap);

    QStringList m_labels;
    QString m_name;
};
