// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend.h"

#include "keymap.h"
#include "lights.h"

#include <QMetaObject>

#include <algorithm>

Backend::Backend(SessionFactory sessions, const QString &stateFile, QObject *parent)
    : QObject(parent),
      m_stateFile(stateFile.isEmpty() ? akko::defaultStateFile() : stateFile.toStdString())
{
    // While the keyboard is unreachable, try again every few seconds, so plugging it in or
    // closing the vendor web app is enough.
    m_retry.setInterval(3000);
    connect(&m_retry, &QTimer::timeout, this, &Backend::refresh);

    // The worker calls back on its own thread; hop to this one before touching state.
    m_worker = std::make_unique<DeviceWorker>(
        std::move(sessions),
        [this](const Snapshot &s) { QMetaObject::invokeMethod(this, [this, s] { applyState(s); }, Qt::QueuedConnection); },
        [this](const std::string &e) {
            const QString message = QString::fromStdString(e);
            QMetaObject::invokeMethod(this, [this, message] { applyError(message); }, Qt::QueuedConnection);
        });
    refresh();
}

Backend::~Backend()
{
    m_worker.reset();  // joins the thread before the members its callbacks use go away
}

QString Backend::color() const
{
    const auto &l = m_state.lighting;
    return QString::asprintf("#%02x%02x%02x", l[akko::Lighting::Red], l[akko::Lighting::Green],
                             l[akko::Lighting::Blue]);
}

QString Backend::firmware() const
{
    return m_state.firmware ? QString::fromStdString(akko::formatFirmware(m_state.firmware)) : QString();
}

void Backend::post(const std::string &key, DeviceWorker::Action action)
{
    m_worker->post(key, std::move(action));
}

void Backend::refresh()
{
    post("refresh", [](akko::Keyboard &, Snapshot &) {});  // every batch ends with a full read
}

void Backend::reload()
{
    m_worker->invalidateKeymap();
    refresh();
}

QVariantList Backend::layout() const
{
    QVariantList keys;
    for (const auto &k : akko::layout())
        keys.append(QVariantMap{{"id", k.id}, {"x", k.x}, {"y", k.y}, {"w", k.w}});
    return keys;
}

namespace {

QString code(const akko::Action &a)
{
    return QString::fromStdString(akko::actionHex(a));
}

QVariantMap info(const akko::Action &a)
{
    static const char *const kinds[] = {"key",    "mouse",   "consumer", "macro",   "system",
                                        "lighting", "profile", "layer",    "unbound", "unknown"};
    QVariantMap m{{"code", code(a)}, {"kind", kinds[int(akko::actionKind(a))]}, {"modifiers", 0}, {"value", 0}};
    switch (akko::actionKind(a)) {
    case akko::ActionKind::Key: {
        const auto c = *akko::keyCombo(a);
        QVariantList keys;
        for (auto k : c.keys)
            keys.append(int(k));
        m["modifiers"] = int(c.modifiers);
        m["keys"] = keys;
        break;
    }
    case akko::ActionKind::Mouse: m["value"] = int(a[1]); break;
    case akko::ActionKind::Consumer: m["value"] = int(a[2] << 8 | a[3]); break;
    case akko::ActionKind::Layer: m["value"] = int(a[1]); break;
    case akko::ActionKind::Macro:
    case akko::ActionKind::System:
    case akko::ActionKind::Profile: m["value"] = int(a[3]); break;
    case akko::ActionKind::Lighting: m["value"] = int(a[1] << 8 | a[2]); break;
    default: break;
    }
    return m;
}

bool knownKey(int key)
{
    const auto l = akko::layout();
    return std::any_of(l.begin(), l.end(), [&](const akko::KeyPosition &k) { return k.id == key; });
}

}  // namespace

QVariantMap Backend::actionInfo(const QString &c) const
{
    const auto a = akko::parseAction(c.toStdString());
    return a ? info(*a) : QVariantMap{};
}

QVariantMap Backend::factoryAction(int layer, int key) const
{
    if (layer < 0 || layer >= akko::kLayerCount || !knownKey(key))
        return {};
    return info(akko::factoryAction(layer, static_cast<std::uint16_t>(key)));
}

QString Backend::keyCode(int usage) const
{
    return usage > 0 && usage <= 0xff ? code(akko::keyAction(static_cast<std::uint8_t>(usage))) : QString();
}

QString Backend::comboCode(int modifiers, const QVariantList &keys) const
{
    std::vector<std::uint8_t> k;
    for (const auto &v : keys) {
        const int u = v.toInt();
        if (u <= 0 || u > 0xff)
            return {};
        k.push_back(static_cast<std::uint8_t>(u));
    }
    if (modifiers < 0 || modifiers > 0xff)
        return {};
    try {
        return code(akko::comboAction(static_cast<std::uint8_t>(modifiers), k));
    } catch (const akko::Error &) {
        return {};
    }
}

QString Backend::mouseCode(int button) const
{
    return button >= 1 && button <= 3 ? code(akko::mouseAction(static_cast<std::uint8_t>(button))) : QString();
}

QString Backend::consumerCode(int usage) const
{
    return usage > 0 && usage <= 0xffff ? code(akko::consumerAction(static_cast<std::uint16_t>(usage))) : QString();
}

QString Backend::layerCode(int layer) const
{
    return layer == 0 || layer == 1 ? code(akko::layerAction(layer)) : QString();
}

QString Backend::unboundCode() const
{
    return code(akko::kUnbound);
}

QString Backend::brightnessCycleCode() const
{
    return code(akko::kBrightnessCycle);
}

bool Backend::setKeyAction(int layer, int key, const QString &c)
{
    const auto a = akko::parseAction(c.toStdString());
    if (!a || layer < 0 || layer >= akko::kLayerCount || !knownKey(key))
        return false;
    const auto id = static_cast<std::uint16_t>(key);
    const akko::Action action = *a;
    // Keyed per key: clicking through the picker only sends the last choice.
    post("key " + std::to_string(layer) + " " + std::to_string(key), [layer, id, action](akko::Keyboard &kb, Snapshot &s) {
        const akko::Action back = kb.setKey(layer, id, action);
        for (auto &[k, v] : s.keymap[layer])
            if (k == id)
                v = back;
    });
    return true;
}

bool Backend::restoreKeyDefault(int layer, int key)
{
    if (layer < 0 || layer >= akko::kLayerCount || !knownKey(key))
        return false;
    return setKeyAction(layer, key, code(akko::factoryAction(layer, static_cast<std::uint16_t>(key))));
}

void Backend::setProfile(int profile)
{
    if (profile < 1 || (m_state.profileCount && profile > m_state.profileCount))
        return;
    post("profile", [profile](akko::Keyboard &kb, Snapshot &) { kb.setProfile(profile - 1); });
}

void Backend::setEffect(int effect)
{
    if (effect < 0 || effect > 0x13)
        return;
    post("effect", [effect](akko::Keyboard &kb, Snapshot &) { kb.setEffect(effect); });
}

bool Backend::setColor(const QString &color)
{
    std::array<std::uint8_t, 3> rgb;
    try {
        rgb = akko::parseColor(color.toStdString());
    } catch (const akko::Error &) {
        return false;
    }
    post("color", [rgb](akko::Keyboard &kb, Snapshot &) { akko::setColor(kb, rgb); });
    return true;
}

void Backend::setRainbow(bool on)
{
    post("rainbow", [on](akko::Keyboard &kb, Snapshot &) { akko::setRainbow(kb, on); });
}

void Backend::setBrightness(int value)
{
    const std::string file = m_stateFile;
    post("brightness", [value, file](akko::Keyboard &kb, Snapshot &) { akko::setBrightness(kb, value, file); });
}

void Backend::setSpeed(int value)
{
    post("speed", [value](akko::Keyboard &kb, Snapshot &) { akko::setSpeed(kb, value); });
}

void Backend::setLightsOn(bool on)
{
    const std::string file = m_stateFile;
    const auto action = on ? akko::LightsAction::On : akko::LightsAction::Off;
    post("lights", [action, file](akko::Keyboard &kb, Snapshot &) { akko::switchLights(kb, action, file); });
}

void Backend::toggleLights()
{
    // Not keyed: two quick toggles must both happen, not merge into one.
    const std::string file = m_stateFile;
    post({}, [file](akko::Keyboard &kb, Snapshot &) { akko::switchLights(kb, akko::LightsAction::Toggle, file); });
}

QString Backend::classifyError(const QString &message)
{
    if (message.startsWith("keyboard not found"))
        return "notFound";
    if (message.startsWith("no permission"))
        return "permission";
    if (message.startsWith("keyboard busy"))
        return "busy";
    if (message.startsWith("no response") || message.startsWith("keyboard disconnected"))
        return "noResponse";
    return "other";
}

void Backend::applyState(const Snapshot &s)
{
    m_state = s;
    m_keymap.clear();
    if (s.keymapProfile >= 0)
        for (const auto &layer : s.keymap) {
            QVariantMap keys;
            for (const auto &[id, action] : layer)
                keys.insert(QString::number(id), info(action));
            m_keymap.append(keys);
        }
    m_ready = true;
    m_retry.stop();
    if (!m_connected || !m_errorKind.isEmpty()) {
        m_connected = true;
        m_errorKind.clear();
        m_errorDetail.clear();
        emit connectionChanged();
    }
    emit stateChanged();
}

void Backend::applyError(const QString &message)
{
    m_connected = false;
    m_errorKind = classifyError(message);
    m_errorDetail = message;
    m_retry.start();
    emit connectionChanged();
}
