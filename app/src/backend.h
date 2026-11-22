// SPDX-License-Identifier: GPL-3.0-or-later
// The keyboard as QML sees it: its state as properties, changes as invokable actions.
// Every action runs on the DeviceWorker; the properties only ever show what was read back
// from the board.
#pragma once

#include "device_worker.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(QString errorKind READ errorKind NOTIFY connectionChanged)
    Q_PROPERTY(QString errorDetail READ errorDetail NOTIFY connectionChanged)
    Q_PROPERTY(int profile READ profile NOTIFY stateChanged)
    Q_PROPERTY(int profileCount READ profileCount NOTIFY stateChanged)
    Q_PROPERTY(int effect READ effect NOTIFY stateChanged)
    Q_PROPERTY(bool rainbow READ rainbow NOTIFY stateChanged)
    Q_PROPERTY(QString color READ color NOTIFY stateChanged)
    Q_PROPERTY(int brightness READ brightness NOTIFY stateChanged)
    Q_PROPERTY(int maxBrightness READ maxBrightness CONSTANT)
    Q_PROPERTY(int speed READ speed NOTIFY stateChanged)
    Q_PROPERTY(bool lightsOn READ lightsOn NOTIFY stateChanged)
    Q_PROPERTY(QString firmware READ firmware NOTIFY stateChanged)
    Q_PROPERTY(QString devicePath READ devicePath NOTIFY stateChanged)
    // Keys page. layout: [{id, x, y, w}] in quarter key widths. keymap: one object per layer,
    // key ID (as a string) → action info (see actionInfo).
    Q_PROPERTY(QVariantList layout READ layout CONSTANT)
    Q_PROPERTY(bool keymapReady READ keymapReady NOTIFY stateChanged)
    Q_PROPERTY(QVariantList keymap READ keymap NOTIFY stateChanged)
    Q_PROPERTY(bool travelReady READ travelReady NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap travel READ travel NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap travelLimits READ travelLimits CONSTANT)
    Q_PROPERTY(bool canUndoTravel READ canUndoTravel NOTIFY stateChanged)
    // Advanced keys page: one record per advanced key (see advancedKeys()), and the editors' limits.
    Q_PROPERTY(bool advancedReady READ advancedReady NOTIFY stateChanged)
    Q_PROPERTY(QVariantList advancedKeys READ advancedKeys NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap advancedLimits READ advancedLimits CONSTANT)
    // Colour tab: key ID (string) → "#rrggbb", the colours the Custom effect shows.
    Q_PROPERTY(bool colorsReady READ colorsReady NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap keyColors READ keyColors NOTIFY stateChanged)
    Q_PROPERTY(int customEffect READ customEffect CONSTANT)
    // Macros page: the active profile's macros, in blob order (see macros()), and the storage.
    Q_PROPERTY(bool macrosReady READ macrosReady NOTIFY stateChanged)
    Q_PROPERTY(QVariantList macros READ macros NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap macroStorage READ macroStorage NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap macroLimits READ macroLimits CONSTANT)

public:
    explicit Backend(SessionFactory sessions = hidrawSessions(), const QString &stateFile = {},
                     QObject *parent = nullptr);
    ~Backend() override;

    bool connected() const { return m_connected; }
    bool ready() const { return m_ready; }
    // "notFound", "permission", "busy", "noResponse" or "other", so QML can say it in the
    // user's language; errorDetail is the core's own English message.
    QString errorKind() const { return m_errorKind; }
    QString errorDetail() const { return m_errorDetail; }
    int profile() const { return m_state.profile + 1; }
    int profileCount() const { return m_state.profileCount; }
    int effect() const { return m_state.lighting[akko::Lighting::Effect]; }
    bool rainbow() const { return m_state.lighting.rainbow(); }
    QString color() const;
    int brightness() const { return m_state.lighting[akko::Lighting::Brightness]; }
    int maxBrightness() const { return akko::kMaxBrightness; }
    int speed() const { return m_state.lighting[akko::Lighting::Speed]; }
    bool lightsOn() const { return brightness() > 0; }
    QString firmware() const;
    QString devicePath() const { return QString::fromStdString(m_state.path); }

    QVariantList layout() const;
    bool keymapReady() const { return m_state.keymapProfile >= 0; }
    QVariantList keymap() const { return m_keymap; }

    // Travel of the active profile, in mm: key id (string) → actuation, rapidTrigger, press,
    // release, deadZones, deadTop, deadBottom.
    bool travelReady() const { return m_state.travelProfile >= 0; }
    QVariantMap travel() const { return m_travel; }
    QVariantMap travelLimits() const;  // min/max in mm for the page's sliders, and the step
    bool canUndoTravel() const { return m_state.travelUndo.has_value(); }

    // Advanced keys of the active profile, as read from the board. Each record: {type: "toggle" |
    // "modTap" | "dks" | "socd" | "multiPoint" | "release", key, and by type: action (Toggle,
    // Release, Mod-Tap hold), tap, holdMs; depths [4 × mm] and rows [4 × {action, from, to,
    // stages}] (DKS; from/to are stage indexes, -1 when the row is unused or its stages aren't a
    // single press or hold, stages = the bytes as hex); key2 and mode ("neutral" | "lastInput" |
    // "key1" | "key2") for SOCD; points [3 × {action, depth}] for Multi-Point. Actions are codes
    // as in actionInfo, "" for an empty slot.
    bool advancedReady() const { return m_state.advancedProfile >= 0; }
    QVariantList advancedKeys() const { return m_advanced; }
    bool colorsReady() const { return m_state.colorsProfile >= 0; }
    QVariantMap keyColors() const { return m_colors; }
    int customEffect() const { return akko::kEffectCustom; }
    QVariantMap advancedLimits() const;  // holdMs and depth ranges, step, the vendor app's defaults

    // Macros of the active profile, as read from the board. Each record: {name, size (bytes in
    // the blob), steps: [{press, usage, delay}]}; usage = HID keyboard usage, delay in ms after
    // the step. The board keeps 1 ms on the last step.
    bool macrosReady() const { return m_state.macrosProfile >= 0; }
    QVariantList macros() const { return m_macros; }
    QVariantMap macroStorage() const;  // {used, max} in bytes
    QVariantMap macroLimits() const;   // name length, standard delay, max delay, max repeat

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void reload();  // refresh, keymap included (Ctrl+R)

    // What an action is, for QML to name it: {code: "00 01 19 00", kind: "key" | "mouse" |
    // "consumer" | "macro" | "system" | "lighting" | "profile" | "layer" | "unbound" | "unknown",
    // modifiers, keys: [usages], value}. value = mouse button, consumer usage, macro index,
    // layer, profile (0-based) or the last byte for system/lighting. A macro also has loop
    // ("times" | "untilKeyPressed" | "whileHeld", "" if unknown) and count.
    Q_INVOKABLE QVariantMap actionInfo(const QString &code) const;
    Q_INVOKABLE QVariantMap factoryAction(int layer, int key) const;
    // Action codes for the picker; an empty string when the input can't be encoded.
    Q_INVOKABLE QString keyCode(int usage) const;
    Q_INVOKABLE QString comboCode(int modifiers, const QVariantList &keys) const;
    Q_INVOKABLE QString mouseCode(int button) const;
    Q_INVOKABLE QString consumerCode(int usage) const;
    Q_INVOKABLE QString layerCode(int layer) const;
    Q_INVOKABLE QString unboundCode() const;
    Q_INVOKABLE QString brightnessCycleCode() const;
    // Writes the action, the drawing then shows the board's read-back. False (nothing sent) for
    // a bad layer, key or code.
    Q_INVOKABLE bool setKeyAction(int layer, int key, const QString &code);
    // The same action on each of these keys. False (nothing sent) if any of them is bad.
    Q_INVOKABLE bool setKeyActions(int layer, const QVariantList &keys, const QString &code);
    Q_INVOKABLE bool restoreKeyDefault(int layer, int key);

    // Changes one part of the travel on these keys and keeps the rest of each key as it is.
    // change holds fields of one part only, in mm: {actuation}, {rapidTrigger, press, release}
    // or {deadZones, deadTop, deadBottom}. Turning Rapid Trigger on for a key at 0 / 0 gives it
    // 0.005 / 0.005, as the vendor app does. False, and nothing sent, if it isn't valid.
    Q_INVOKABLE bool changeTravel(const QVariantList &keys, const QVariantMap &change);
    Q_INVOKABLE void undoTravel();  // puts back what the last change replaced

    // True when the record (same shape as advancedKeys) can be saved.
    Q_INVOKABLE bool advancedValid(const QVariantMap &record) const;
    // Saves these records in one go: whatever their keys hold now is cleared first (both keys of
    // a SOCD pair), then the page shows the board's read-back. False, and nothing sent, if one
    // isn't valid or two share a key.
    Q_INVOKABLE bool saveAdvanced(const QVariantList &records);
    // Removes these keys' advanced keys (a SOCD pair whole, from either key).
    Q_INVOKABLE bool removeAdvanced(const QVariantList &keys);

    // Gives these keys one colour for the Custom effect (kept whatever effect is on), then the
    // page shows the board's read-back. False, and nothing sent, for a bad key or colour.
    Q_INVOKABLE bool setKeyColors(const QVariantList &keys, const QString &color);

    // True when the record ({name, steps}, as in macros) can be stored on its own.
    Q_INVOKABLE bool macroValid(const QVariantMap &record) const;
    // Replaces macro `index`, or adds the record after the others for -1, then the page shows
    // the board's read-back. Keys bound to macros keep their index. False, and nothing sent, if
    // the record isn't valid, the index doesn't exist, or the macros wouldn't fit the storage.
    Q_INVOKABLE bool saveMacro(int index, const QVariantMap &record);
    // Removes macro `index`: keys bound to it are unbound, keys bound to later macros follow
    // their macro, and the keymap shows the read-back. False, and nothing sent, for a bad index.
    Q_INVOKABLE bool deleteMacro(int index);
    // Action code that runs macro `index`: loop "times" (count 1–255), "untilKeyPressed" or
    // "whileHeld". Empty when it can't be encoded.
    Q_INVOKABLE QString macroCode(int index, const QString &loop, int count = 1) const;
    // HID usage of the physical key behind a key event's nativeScanCode (Linux), 0 if none, so
    // recording doesn't depend on the keyboard layout.
    Q_INVOKABLE int usageForScanCode(int nativeScanCode) const;

    Q_INVOKABLE void setProfile(int profile);  // 1-based, like the UI
    Q_INVOKABLE void setEffect(int effect);
    Q_INVOKABLE bool setColor(const QString &color);  // false (and nothing sent) if it isn't a colour
    Q_INVOKABLE void setRainbow(bool on);
    Q_INVOKABLE void setBrightness(int value);
    Q_INVOKABLE void setSpeed(int value);
    Q_INVOKABLE void setLightsOn(bool on);
    Q_INVOKABLE void toggleLights();

    static QString classifyError(const QString &message);

signals:
    void connectionChanged();
    void stateChanged();

private:
    void post(const std::string &key, DeviceWorker::Action action);
    void applyState(const Snapshot &s);
    void applyError(const QString &message);

    std::string m_stateFile;
    Snapshot m_state;
    QVariantList m_keymap;
    QVariantMap m_travel;
    QVariantList m_advanced;
    QVariantMap m_colors;
    QVariantList m_macros;
    bool m_connected = false;
    bool m_ready = false;
    QString m_errorKind;
    QString m_errorDetail;
    QTimer m_retry;
    std::unique_ptr<DeviceWorker> m_worker;  // destroyed first, in ~Backend
};
