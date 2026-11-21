// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend.h"

#include "keymap.h"
#include "lights.h"

#include <QMetaObject>

#include <algorithm>
#include <cmath>
#include <optional>

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

// ---- travel ----------------------------------------------------------------

namespace {

// Units to mm as the page shows them: 57 → 0.285, not 0.28500000000000003.
double mm(std::uint16_t units)
{
    return std::round(units * akko::kMmPerUnit * 1000) / 1000;
}

struct TravelChange {
    akko::TravelPart part = akko::TravelPart::Actuation;
    std::optional<std::uint16_t> actuation, press, release, deadTop, deadBottom;
    std::optional<bool> rapidTrigger, deadZones;

    void apply(akko::KeyTravel &t) const
    {
        if (actuation)
            t.actuation = *actuation;
        if (rapidTrigger) {
            t.rapidTrigger = *rapidTrigger;
            if (t.rapidTrigger && !press && t.press == 0)
                t.press = akko::kMinRapidTrigger;
            if (t.rapidTrigger && !release && t.release == 0)
                t.release = akko::kMinRapidTrigger;
        }
        if (press)
            t.press = *press;
        if (release)
            t.release = *release;
        if (deadZones)
            t.deadZones = *deadZones;
        if (deadTop)
            t.deadTop = *deadTop;
        if (deadBottom)
            t.deadBottom = *deadBottom;
    }
};

// Reads {field: value} into a change; nullopt if a field is unknown, fields of two parts are
// mixed, or a distance isn't a number.
std::optional<TravelChange> parseChange(const QVariantMap &m)
{
    using P = akko::TravelPart;
    TravelChange c;
    std::optional<P> part;
    for (auto it = m.begin(); it != m.end(); ++it) {
        const QString &f = it.key();
        const P p = f == "actuation"                                           ? P::Actuation
                    : (f == "rapidTrigger" || f == "press" || f == "release") ? P::RapidTrigger
                    : (f == "deadZones" || f == "deadTop" || f == "deadBottom") ? P::DeadZones
                                                                                 : P(-1);
        if (int(p) < 0 || (part && *part != p))
            return std::nullopt;
        part = p;
        if (f == "rapidTrigger" || f == "deadZones") {
            (f == "rapidTrigger" ? c.rapidTrigger : c.deadZones) = it.value().toBool();
            continue;
        }
        bool ok = false;
        const double v = it.value().toDouble(&ok);
        if (!ok || v < 0 || v > 10)
            return std::nullopt;
        const auto units = static_cast<std::uint16_t>(std::lround(v / akko::kMmPerUnit));
        (f == "actuation" ? c.actuation
         : f == "press"   ? c.press
         : f == "release" ? c.release
         : f == "deadTop" ? c.deadTop
                          : c.deadBottom) = units;
    }
    if (!part)
        return std::nullopt;
    c.part = *part;
    return c;
}

}  // namespace

QVariantMap Backend::travelLimits() const
{
    return {{"minActuation", mm(akko::kMinActuation)}, {"maxActuation", mm(akko::kMaxActuation)},
            {"minRapidTrigger", mm(akko::kMinRapidTrigger)}, {"maxRapidTrigger", mm(akko::kMaxRapidTrigger)},
            {"maxDeadZone", mm(akko::kMaxDeadZone)}, {"step", akko::kMmPerUnit}};
}

bool Backend::changeTravel(const QVariantList &keys, const QVariantMap &change)
{
    const auto c = parseChange(change);
    if (!c || keys.isEmpty() || !travelReady())
        return false;
    std::vector<std::uint16_t> ids;
    for (const auto &k : keys) {
        const int id = k.toInt();
        if (!knownKey(id))
            return false;
        if (std::find(ids.begin(), ids.end(), id) == ids.end())
            ids.push_back(static_cast<std::uint16_t>(id));
    }
    // Check against what the page shows; the worker checks again against the board's values.
    for (auto t : m_state.travel)
        if (std::find(ids.begin(), ids.end(), t.key) != ids.end()) {
            c->apply(t);
            if (!akko::travelProblem(t, c->part).empty())
                return false;
        }
    // Not keyed: each change builds on the one before, so none may be merged away.
    post({}, [ids, c = *c](akko::Keyboard &kb, Snapshot &s) {
        const int profile = kb.profile();
        if (s.travelProfile != profile) {
            s.travel = kb.readTravel();
            s.travelProfile = profile;
        }
        std::vector<akko::KeyTravel> before, after;
        for (const auto &t : s.travel)  // in keyReadOrder(), like the vendor app
            if (std::find(ids.begin(), ids.end(), t.key) != ids.end()) {
                before.push_back(t);
                after.push_back(t);
                c.apply(after.back());
            }
        // Kept before writing: if the write fails halfway, Undo can still put the old values back.
        s.travelUndo = Snapshot::TravelUndo{profile, c.part, before};
        const auto back = kb.setTravel(c.part, after);
        for (auto &t : s.travel)
            for (const auto &b : back)
                if (t.key == b.key)
                    t = b;
    });
    return true;
}

void Backend::undoTravel()
{
    post({}, [](akko::Keyboard &kb, Snapshot &s) {
        if (!s.travelUndo || s.travelUndo->profile != kb.profile())
            return;
        const auto undo = *s.travelUndo;
        s.travelUndo.reset();
        const auto back = kb.setTravel(undo.part, undo.before);
        for (auto &t : s.travel)
            for (const auto &b : back)
                if (t.key == b.key)
                    t = b;
    });
}

// ---- advanced keys ---------------------------------------------------------

namespace {

using akko::AdvancedType;

const std::pair<AdvancedType, const char *> kTypeNames[] = {
    {AdvancedType::Toggle, "toggle"}, {AdvancedType::ModTap, "modTap"},         {AdvancedType::Dks, "dks"},
    {AdvancedType::Socd, "socd"},     {AdvancedType::MultiPoint, "multiPoint"}, {AdvancedType::Release, "release"},
};
const std::pair<akko::SocdMode, const char *> kModeNames[] = {
    {akko::SocdMode::Neutral, "neutral"}, {akko::SocdMode::LastInput, "lastInput"},
    {akko::SocdMode::Key1, "key1"},       {akko::SocdMode::Key2, "key2"},
};

QString slotCode(const akko::Action &a)
{
    return a == akko::kNoAction ? QString() : code(a);
}

QVariantMap toVariant(const akko::AdvancedKey &a)
{
    QVariantMap m{{"key", a.key}};
    for (const auto &[t, name] : kTypeNames)
        if (t == a.type)
            m["type"] = name;
    switch (a.type) {
    case AdvancedType::Toggle:
        m["action"] = code(a.action);
        m["holdMs"] = a.holdMs;
        break;
    case AdvancedType::Release:
        m["action"] = code(a.action);
        break;
    case AdvancedType::ModTap:
        m["action"] = code(a.action);
        m["tap"] = code(a.tap);
        m["holdMs"] = a.holdMs;
        break;
    case AdvancedType::Dks: {
        QVariantList depths, rows;
        for (auto d : a.depths)
            depths.append(mm(d));
        for (const auto &r : a.rows) {
            const auto stroke = akko::dksStroke(r.stages);
            const akko::Action stages{r.stages[0], r.stages[1], r.stages[2], r.stages[3]};
            rows.append(QVariantMap{{"action", slotCode(r.action)},
                                    {"from", stroke ? stroke->from : -1},
                                    {"to", stroke ? stroke->to : -1},
                                    {"stages", code(stages)}});
        }
        m["depths"] = depths;
        m["rows"] = rows;
        break;
    }
    case AdvancedType::Socd:
        m["key2"] = a.key2;
        m["mode"] = QString();
        for (const auto &[mode, name] : kModeNames)
            if (mode == a.mode)
                m["mode"] = name;
        break;
    case AdvancedType::MultiPoint: {
        QVariantList points;
        for (const auto &p : a.points)
            points.append(QVariantMap{{"action", slotCode(p.action)}, {"depth", mm(p.depth)}});
        m["points"] = points;
        break;
    }
    default:
        break;
    }
    return m;
}

std::optional<std::uint16_t> units(const QVariant &v)
{
    bool ok = false;
    const double d = v.toDouble(&ok);
    if (!ok || d < 0 || d > 10)
        return std::nullopt;
    return static_cast<std::uint16_t>(std::lround(d / akko::kMmPerUnit));
}

// "" is an empty slot; anything else must be a code.
std::optional<akko::Action> slotAction(const QVariant &v)
{
    const QString c = v.toString();
    if (c.isEmpty())
        return akko::kNoAction;
    return akko::parseAction(c.toStdString());
}

// The record QML sends, or nullopt if a field is missing or malformed. Range checks are
// advancedProblem()'s.
std::optional<akko::AdvancedKey> fromVariant(const QVariantMap &m)
{
    std::optional<AdvancedType> type;
    for (const auto &[t, name] : kTypeNames)
        if (m.value("type").toString() == name)
            type = t;
    const int key = m.value("key", -1).toInt();
    if (!type || key < 0 || key > 0xffff)
        return std::nullopt;
    akko::AdvancedKey a = akko::makeAdvanced(*type, static_cast<std::uint16_t>(key));
    auto action = [&](const char *field, akko::Action &out) {
        const auto act = slotAction(m.value(field));
        if (act)
            out = *act;
        return act.has_value();
    };
    auto hold = [&] {
        const int ms = m.value("holdMs", -1).toInt();
        a.holdMs = static_cast<std::uint16_t>(std::clamp(ms, 0, 0xffff));
        return ms >= 0;
    };
    switch (*type) {
    case AdvancedType::Toggle:
        if (!action("action", a.action) || !hold())
            return std::nullopt;
        break;
    case AdvancedType::Release:
        if (!action("action", a.action))
            return std::nullopt;
        break;
    case AdvancedType::ModTap:
        if (!action("action", a.action) || !action("tap", a.tap) || !hold())
            return std::nullopt;
        break;
    case AdvancedType::Dks: {
        const auto depths = m.value("depths").toList();
        const auto rows = m.value("rows").toList();
        if (depths.size() != 4 || rows.size() != int(akko::kDksRows))
            return std::nullopt;
        for (int i = 0; i < 4; ++i) {
            const auto u = units(depths[i]);
            if (!u)
                return std::nullopt;
            a.depths[i] = *u;
        }
        for (int i = 0; i < rows.size(); ++i) {
            const auto r = rows[i].toMap();
            const auto act = slotAction(r.value("action"));
            if (!act)
                return std::nullopt;
            a.rows[i].action = *act;
            const int from = r.value("from", -1).toInt(), to = r.value("to", -1).toInt();
            if (from >= 0) {
                if (to < from || to >= int(akko::kDksStages))
                    return std::nullopt;
                a.rows[i].stages = akko::dksStages({from, to});
            } else if (*act != akko::kNoAction) {
                // A pattern the editor can't draw: kept as read.
                const auto kept = akko::parseAction(r.value("stages").toString().toStdString());
                if (!kept)
                    return std::nullopt;
                std::copy(kept->begin(), kept->end(), a.rows[i].stages.begin());
            }
        }
        break;
    }
    case AdvancedType::Socd: {
        const int key2 = m.value("key2", -1).toInt();
        std::optional<akko::SocdMode> mode;
        for (const auto &[md, name] : kModeNames)
            if (m.value("mode").toString() == name)
                mode = md;
        if (key2 < 0 || key2 > 0xffff || !mode)
            return std::nullopt;
        a.key2 = static_cast<std::uint16_t>(key2);
        a.mode = *mode;
        break;
    }
    case AdvancedType::MultiPoint:
        for (const auto &v : m.value("points").toList()) {
            const auto p = v.toMap();
            const auto act = slotAction(p.value("action"));
            const auto depth = units(p.value("depth"));
            if (!act || !depth)
                return std::nullopt;
            a.points.push_back({*act, *depth});
        }
        break;
    default:
        return std::nullopt;
    }
    return a;
}

// Puts the board's read-back in the snapshot.
void storeAdvanced(Snapshot &s, std::vector<akko::AdvancedKey> after, int profile)
{
    s.advanced = std::move(after);
    s.advancedProfile = profile;
}

}  // namespace

QVariantMap Backend::advancedLimits() const
{
    return {{"minHoldMs", akko::kMinHoldMs},
            {"maxHoldMs", akko::kMaxHoldMs},
            {"defaultHoldMs", 200},
            {"minDepth", mm(akko::kMinActuation)},
            {"maxDepth", mm(akko::kMaxActuation)},
            {"step", akko::kMmPerUnit},
            {"dksDepths", QVariantList{1.0, 3.0, 3.0, 1.0}},
            {"pointDepths", QVariantList{0.5, 1.0, 1.5}}};
}

bool Backend::advancedValid(const QVariantMap &record) const
{
    const auto a = fromVariant(record);
    return a && akko::advancedProblem(*a).empty();
}

bool Backend::saveAdvanced(const QVariantMap &record)
{
    const auto a = fromVariant(record);
    if (!a || !akko::advancedProblem(*a).empty() || !advancedReady())
        return false;
    // Keyed per key: only the last save of a key waiting in the queue is sent.
    post("advanced " + std::to_string(a->key), [a = *a](akko::Keyboard &kb, Snapshot &s) {
        const int profile = kb.profile();
        storeAdvanced(s, kb.setAdvanced(a), profile);
    });
    return true;
}

bool Backend::removeAdvanced(int key)
{
    if (!knownKey(key) || !advancedReady())
        return false;
    const auto id = static_cast<std::uint16_t>(key);
    post("advanced " + std::to_string(key), [id](akko::Keyboard &kb, Snapshot &s) {
        const int profile = kb.profile();
        storeAdvanced(s, kb.clearAdvanced(id), profile);
    });
    return true;
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
    m_travel.clear();
    if (s.travelProfile >= 0)
        for (const auto &t : s.travel)
            m_travel.insert(QString::number(t.key),
                            QVariantMap{{"actuation", mm(t.actuation)}, {"rapidTrigger", t.rapidTrigger},
                                        {"press", mm(t.press)}, {"release", mm(t.release)},
                                        {"deadZones", t.deadZones}, {"deadTop", mm(t.deadTop)},
                                        {"deadBottom", mm(t.deadBottom)}});
    m_advanced.clear();
    if (s.advancedProfile >= 0)
        for (const auto &a : s.advanced)
            m_advanced.append(toVariant(a));
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
