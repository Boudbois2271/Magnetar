// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend.h"

#include "keymap.h"
#include "lights.h"
#include "macro.h"
#include "report_listener.h"
#include "scancodes.h"

#include <QMetaObject>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

Backend::Backend(SessionFactory sessions, const QString &stateFile, QObject *parent)
    : QObject(parent),
      m_stateFile(stateFile.isEmpty() ? akko::defaultStateFile() : stateFile.toStdString())
{
    // While the keyboard is unreachable, try again every few seconds, so plugging it in or
    // closing the vendor web app is enough (the interval is set in applyError()).
    connect(&m_retry, &QTimer::timeout, this, &Backend::refresh);
    m_restartWait.setSingleShot(true);
    connect(&m_restartWait, &QTimer::timeout, this, &Backend::refresh);

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
    if (m_calibrationRun)
        m_calibrationRun->stop = true;  // Finish, so the keyboard types again
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

const std::pair<akko::MacroLoop, const char *> kLoopNames[] = {
    {akko::MacroLoop::Times, "times"},
    {akko::MacroLoop::UntilKeyPressed, "untilKeyPressed"},
    {akko::MacroLoop::WhileHeld, "whileHeld"},
};

QVariantMap info(const akko::Action &a)
{
    static const char *const kinds[] = {"key",    "mouse",   "consumer", "macro",   "system",
                                        "lighting", "profile", "layer",    "special", "unbound", "unknown"};
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
        m["value"] = int(a[3]);
        m["loop"] = QString();
        m["count"] = 0;
        if (const auto b = akko::macroBinding(a)) {
            for (const auto &[loop, name] : kLoopNames)
                if (loop == b->loop)
                    m["loop"] = name;
            m["count"] = b->count;
        }
        break;
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
    return button >= 1 && button <= 5 ? code(akko::mouseAction(static_cast<std::uint8_t>(button))) : QString();
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

QString Backend::lightingCode(int target, int step) const
{
    if (target < 0 || target > 4 || step < 0 || step > 2)
        return {};
    return code(akko::lightingAction(static_cast<akko::LightingTarget>(target), static_cast<akko::LightingStep>(step)));
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

bool Backend::setKeyActions(int layer, const QVariantList &keys, const QString &c)
{
    if (!akko::parseAction(c.toStdString()) || layer < 0 || layer >= akko::kLayerCount || keys.isEmpty())
        return false;
    for (const auto &k : keys)
        if (!knownKey(k.toInt()))
            return false;
    for (const auto &k : keys)
        setKeyAction(layer, k.toInt(), c);
    return true;
}

bool Backend::restoreKeyDefault(int layer, int key)
{
    if (layer < 0 || layer >= akko::kLayerCount || !knownKey(key))
        return false;
    return setKeyAction(layer, key, code(akko::factoryAction(layer, static_cast<std::uint16_t>(key))));
}

bool Backend::resetLayer(int layer)
{
    if (layer != 0 && layer != 1)
        return false;
    post("reset layer " + std::to_string(layer), [layer](akko::Keyboard &kb, Snapshot &s) {
        s.keymap[layer] = kb.resetLayer(layer);
    });
    return true;
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
            const auto pattern = akko::dksPatternIndex(r.stages);
            const akko::Action stages{r.stages[0], r.stages[1], r.stages[2], r.stages[3]};
            rows.append(QVariantMap{{"action", slotCode(r.action)},
                                    {"pattern", pattern ? int(*pattern) : -1},
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
            const int pattern = r.value("pattern", -1).toInt();
            if (pattern >= 0) {
                if (pattern >= int(akko::dksPatterns().size()))
                    return std::nullopt;
                a.rows[i].stages = akko::dksPatternStages(akko::dksPatterns()[std::size_t(pattern)]);
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

// Keeps what a change to these keys replaces, before writing: if the write fails halfway, Undo
// can still put the old records back. A record that covers one of the keys brings its other key
// in too (a SOCD pair is cleared whole).
void rememberAdvanced(akko::Keyboard &kb, Snapshot &s, std::vector<std::uint16_t> keys)
{
    const int profile = kb.profile();
    if (s.advancedProfile != profile)
        storeAdvanced(s, kb.readAdvancedKeys(), profile);
    std::vector<akko::AdvancedKey> before;
    for (const auto &a : s.advanced) {
        const auto ids = akko::advancedKeyIds(a);
        if (std::none_of(ids.begin(), ids.end(),
                         [&](auto id) { return std::find(keys.begin(), keys.end(), id) != keys.end(); }))
            continue;
        before.push_back(a);
        for (auto id : ids)
            if (std::find(keys.begin(), keys.end(), id) == keys.end())
                keys.push_back(id);
    }
    s.advancedUndo = Snapshot::AdvancedUndo{profile, std::move(keys), std::move(before)};
}

}  // namespace

// Every DKS row pattern, as lists of {from, to} strokes (stage indexes): QML names them.
static QVariantList dksPatternList()
{
    QVariantList out;
    for (const auto &p : akko::dksPatterns()) {
        QVariantList strokes;
        for (const auto &s : p)
            strokes.append(QVariantMap{{"from", s.from}, {"to", s.to}});
        out.append(QVariant(strokes));
    }
    return out;
}

QVariantMap Backend::advancedLimits() const
{
    return {{"minHoldMs", akko::kMinHoldMs},
            {"maxHoldMs", akko::kMaxHoldMs},
            {"defaultHoldMs", 200},
            {"minDepth", mm(akko::kMinActuation)},
            {"maxDepth", mm(akko::kMaxActuation)},
            {"maxPointDepth", mm(akko::kMaxPointDepth)},
            {"step", akko::kMmPerUnit},
            {"dksDepths", QVariantList{1.0, 3.0, 3.0, 1.0}},
            {"pointDepths", QVariantList{0.5, 1.0, 1.5}},
            {"dksPatterns", dksPatternList()}};
}

bool Backend::advancedValid(const QVariantMap &record) const
{
    const auto a = fromVariant(record);
    return a && akko::advancedProblem(*a).empty();
}

bool Backend::saveAdvanced(const QVariantList &records)
{
    if (records.isEmpty() || !advancedReady())
        return false;
    std::vector<akko::AdvancedKey> batch;
    std::vector<std::uint16_t> ids;
    for (const auto &r : records) {
        const auto a = fromVariant(r.toMap());
        if (!a || !akko::advancedProblem(*a).empty())
            return false;
        for (auto id : akko::advancedKeyIds(*a)) {
            if (std::find(ids.begin(), ids.end(), id) != ids.end())
                return false;
            ids.push_back(id);
        }
        batch.push_back(*a);
    }
    // Never replaced in the queue: a later save may be for other keys.
    post("", [batch = std::move(batch), ids = std::move(ids)](akko::Keyboard &kb, Snapshot &s) {
        const int profile = kb.profile();
        rememberAdvanced(kb, s, ids);
        storeAdvanced(s, kb.setAdvanced(batch), profile);
    });
    return true;
}

bool Backend::removeAdvanced(const QVariantList &keys)
{
    if (keys.isEmpty() || !advancedReady())
        return false;
    std::vector<std::uint16_t> ids;
    for (const auto &k : keys) {
        if (!knownKey(k.toInt()))
            return false;
        ids.push_back(static_cast<std::uint16_t>(k.toInt()));
    }
    post("", [ids = std::move(ids)](akko::Keyboard &kb, Snapshot &s) {
        const int profile = kb.profile();
        rememberAdvanced(kb, s, ids);
        storeAdvanced(s, kb.clearAdvanced(ids), profile);
    });
    return true;
}

void Backend::undoAdvanced()
{
    post("", [](akko::Keyboard &kb, Snapshot &s) {
        const int profile = kb.profile();
        if (!s.advancedUndo || s.advancedUndo->profile != profile)
            return;
        const auto undo = *s.advancedUndo;
        s.advancedUndo.reset();
        // Cleared first (12 00 type 00, both keys of a pair), then the old records saved back.
        storeAdvanced(s, kb.clearAdvanced(undo.keys), profile);
        if (!undo.before.empty())
            storeAdvanced(s, kb.setAdvanced(undo.before), profile);
    });
}

// ---- macros ----------------------------------------------------------------

namespace {

QVariantMap toVariant(const akko::Macro &m)
{
    QVariantList steps;
    for (const auto &st : m.steps)
        steps.append(QVariantMap{{"press", st.press}, {"usage", int(st.usage)}, {"delay", int(st.delayMs)},
                                {"kind", int(st.kind)}});
    return {{"name", QString::fromStdString(m.name)}, {"size", int(akko::macroSize(m))}, {"steps", steps}};
}

// The record QML sends, or nullopt if a field is missing or out of range. The rest is
// macroProblem()'s.
std::optional<akko::Macro> macroFromVariant(const QVariantMap &r)
{
    if (!r.contains("name") || !r.value("steps").canConvert<QVariantList>())
        return std::nullopt;
    akko::Macro m;
    m.name = r.value("name").toString().toStdString();
    for (const auto &v : r.value("steps").toList()) {
        const auto st = v.toMap();
        bool usageOk = false, delayOk = false;
        const int usage = st.value("usage").toInt(&usageOk);
        const int delay = st.value("delay", akko::kStandardDelayMs).toInt(&delayOk);
        bool kindOk = false;
        const int kind = st.value("kind", akko::kMacroKey).toInt(&kindOk);
        if (!st.contains("press") || !usageOk || !delayOk || !kindOk || usage < 0 || usage > 0xff || delay < 0 ||
            delay > 0xffff || kind < 0 || kind > 0x7f)
            return std::nullopt;
        m.steps.push_back({st.value("press").toBool(), static_cast<std::uint16_t>(delay),
                           static_cast<std::uint8_t>(usage), static_cast<std::uint8_t>(kind)});
    }
    return m;
}

// Reads the macros again if the snapshot's are from another profile.
void freshMacros(akko::Keyboard &kb, Snapshot &s)
{
    const int profile = kb.profile();
    if (s.macrosProfile != profile) {
        s.macros = kb.readMacros();
        s.macrosProfile = profile;
    }
}

}  // namespace

QVariantMap Backend::macroStorage() const
{
    return {{"used", int(akko::macroStorageUsed(m_state.macros))}, {"max", int(akko::kMacroStorage)}};
}

QVariantMap Backend::macroLimits() const
{
    return {{"minName", int(akko::kMinMacroName)}, {"maxName", int(akko::kMaxMacroName)},
            {"standardDelay", int(akko::kStandardDelayMs)}, {"maxDelay", 0xffff},
            {"maxRepeat", akko::kMaxMacroRepeat}, {"keyStep", int(akko::kMacroKey)},
            {"mouseStep", int(akko::kMacroMouse)}, {"leftButton", int(akko::kMacroMouseLeft)},
            {"mouseButtons", QVariantList{int(akko::kMacroMouseLeft), int(akko::kMacroMouseRight),
                                          int(akko::kMacroMouseMiddle), int(akko::kMacroMouseBack),
                                          int(akko::kMacroMouseForward)}}};
}

bool Backend::macroValid(const QVariantMap &record) const
{
    const auto m = macroFromVariant(record);
    return m && akko::macroProblem(*m).empty();
}

bool Backend::saveMacro(int index, const QVariantMap &record)
{
    const auto m = macroFromVariant(record);
    if (!m || !macrosReady() || index < -1 || index >= int(m_state.macros.size()))
        return false;
    // Check the list the page shows; the worker builds it again from the board's.
    auto list = m_state.macros;
    if (index < 0)
        list.push_back(*m);
    else
        list[index] = *m;
    if (!akko::macrosProblem(list).empty())
        return false;
    post({}, [index, macro = *m](akko::Keyboard &kb, Snapshot &s) {
        freshMacros(kb, s);
        auto macros = s.macros;
        if (index < 0)
            macros.push_back(macro);
        else if (index < int(macros.size()))
            macros[index] = macro;
        else
            throw akko::Error("macro " + std::to_string(index + 1) + " is no longer on the keyboard");
        s.macros = kb.writeMacros(macros);
    });
    return true;
}

bool Backend::deleteMacro(int index)
{
    if (!macrosReady() || index < 0 || index >= int(m_state.macros.size()))
        return false;
    post({}, [index](akko::Keyboard &kb, Snapshot &s) {
        freshMacros(kb, s);
        if (index >= int(s.macros.size()))
            throw akko::Error("macro " + std::to_string(index + 1) + " is no longer on the keyboard");
        auto done = kb.deleteMacro(index);
        s.macros = std::move(done.macros);
        if (s.keymapProfile == s.macrosProfile)
            for (const auto &k : done.keys)
                for (auto &[id, action] : s.keymap[k.layer])
                    if (id == k.key)
                        action = k.action;
    });
    return true;
}

QString Backend::macroCode(int index, const QString &loop, int count) const
{
    for (const auto &[l, name] : kLoopNames)
        if (loop == name) {
            try {
                return code(akko::macroAction({index, l, l == akko::MacroLoop::Times ? count : 1}));
            } catch (const akko::Error &) {
                return {};
            }
        }
    return {};
}

int Backend::usageForScanCode(int nativeScanCode) const
{
    return usageForEvdev(nativeScanCode - kXkbKeycodeOffset);
}

void Backend::setProfile(int profile)
{
    if (profile < 1 || (m_state.profileCount && profile > m_state.profileCount))
        return;
    post("profile", [profile](akko::Keyboard &kb, Snapshot &) { kb.setProfile(profile - 1); });
}

QStringList Backend::profileNames() const
{
    QStringList names;
    for (const auto &n : m_state.profileNames)
        names.append(n ? QString::fromLatin1(n->data(), qsizetype(n->size())) : QString());
    return names;
}

bool Backend::renameProfile(int profile, const QString &name)
{
    const std::string n = name.toStdString();
    if (profile < 1 || profile > int(m_state.profileNames.size()) || !akko::profileNameProblem(n).empty())
        return false;
    post({}, [profile, n](akko::Keyboard &kb, Snapshot &s) {
        const auto back = kb.setProfileName(profile - 1, n);
        if (profile <= int(s.profileNames.size()))
            s.profileNames[profile - 1] = back;
    });
    return true;
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

bool Backend::setKeyColors(const QVariantList &keys, const QString &color)
{
    std::array<std::uint8_t, 3> rgb;
    try {
        rgb = akko::parseColor(color.toStdString());
    } catch (const std::exception &) {
        return false;
    }
    if (keys.isEmpty() || !colorsReady())
        return false;
    std::vector<std::uint16_t> ids;
    for (const auto &k : keys) {
        const int id = k.toInt();
        if (!knownKey(id))
            return false;
        if (std::find(ids.begin(), ids.end(), id) == ids.end())
            ids.push_back(static_cast<std::uint16_t>(id));
    }
    // Not keyed: two colours on different keys must both be sent.
    post({}, [ids, rgb](akko::Keyboard &kb, Snapshot &s) {
        std::vector<akko::KeyColor> wanted;
        for (const auto id : akko::keyReadOrder())  // like the vendor app
            if (std::find(ids.begin(), ids.end(), id) != ids.end())
                wanted.push_back({id, rgb});
        const auto back = kb.setColors(wanted);
        for (auto &c : s.colors)
            for (const auto &b : back)
                if (c.key == b.key)
                    c = b;
    });
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

QVariantList Backend::pollingRates() const
{
    QVariantList rates;
    for (int code = 0; akko::pollingRateHz(code); ++code)
        rates.append(akko::pollingRateHz(code));
    std::sort(rates.begin(), rates.end(), [](const QVariant &a, const QVariant &b) { return a.toInt() < b.toInt(); });
    return rates;
}

void Backend::setWinKeyLock(bool on)
{
    post("winKeyLock", [on](akko::Keyboard &kb, Snapshot &) { kb.setBaseSetting(akko::BaseSetting::WinKeyLock, on); });
}

void Backend::setComboOptimization(bool on)
{
    post("comboOptimization",
         [on](akko::Keyboard &kb, Snapshot &) { kb.setBaseSetting(akko::BaseSetting::ComboOptimization, on); });
}

void Backend::setAdaptiveCalibration(bool on)
{
    post("adaptiveCalibration",
         [on](akko::Keyboard &kb, Snapshot &) { kb.setBaseSetting(akko::BaseSetting::AdaptiveCalibration, on); });
}

bool Backend::setPollingRate(int hz)
{
    const int code = akko::pollingRateCode(hz);
    if (code < 0)
        return false;
    post("pollingRate", [code](akko::Keyboard &kb, Snapshot &s) {
        s.base.pollingRate = kb.setBaseSetting(akko::BaseSetting::PollingRate, static_cast<std::uint8_t>(code));
        s.restarting = true;
    });
    return true;
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
    if (s.restarting) {
        // Keep showing what is on screen, with the new polling rate, until the board is back.
        m_state.base.pollingRate = s.base.pollingRate;
        m_restarting = true;
        m_restartClock.start();
        m_restartWait.start(kRestartWaitMs);
        emit connectionChanged();
        emit stateChanged();
        return;
    }
    const bool wasRestarting = std::exchange(m_restarting, false);
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
    m_colors.clear();
    if (s.colorsProfile >= 0)
        for (const auto &c : s.colors)
            m_colors.insert(QString::number(c.key),
                            QString::asprintf("#%02x%02x%02x", c.rgb[0], c.rgb[1], c.rgb[2]));
    m_macros.clear();
    if (s.macrosProfile >= 0)
        for (const auto &m : s.macros)
            m_macros.append(toVariant(m));
    m_ready = true;
    m_retry.stop();
    if (m_reports)
        m_reports->open();
    if (wasRestarting || !m_connected || !m_errorKind.isEmpty()) {
        m_connected = true;
        m_errorKind.clear();
        m_errorDetail.clear();
        emit connectionChanged();
    }
    emit stateChanged();
}

void Backend::keyboardAdded()
{
    if (!m_connected || m_restarting)
        refresh();
}

void Backend::keyboardRemoved(const QString &node)
{
    // While the board restarts (polling rate) it drops off USB on purpose: that has its own wait.
    if (!m_connected || m_restarting || node != QString::fromStdString(m_state.path))
        return;
    m_worker->invalidateKeymap();  // whatever comes back is read whole
    applyError("keyboard not found (is it plugged in?)");
}

void Backend::setReportListener(ReportListener *listener)
{
    m_reports = listener;
    connect(listener, &ReportListener::brightnessReported, this, &Backend::brightnessReported);
    connect(listener, &ReportListener::lost, this, [this] {
        if (m_connected && !m_restarting)
            refresh();  // a read: fails if it was unplugged, reopens the listener if it's back
    });
    listener->open();
}

void Backend::brightnessReported(int value)
{
    auto &b = m_state.lighting[akko::Lighting::Brightness];
    if (!m_ready || b == value)
        return;
    b = static_cast<std::uint8_t>(value);
    emit stateChanged();
}

int Backend::calibrationFull() const
{
    return int(std::count_if(m_calibrationDepth.begin(), m_calibrationDepth.end(),
                             [](const QVariant &v) { return v.toInt() >= kCalibrationFull; }));
}

void Backend::startCalibration()
{
    if (m_calibrationRun)
        return;
    auto run = std::make_shared<CalibrationRun>();
    m_calibrationRun = run;
    m_calibrating = true;
    m_calibrationDepth.clear();
    emit calibrationChanged();
    post({}, [this, run](akko::Keyboard &kb, Snapshot &) {
        const auto before = kb.readCalibration();
        QMetaObject::invokeMethod(this, [this, before] {
            m_calibrationBefore.clear();
            for (const auto &k : before)
                m_calibrationBefore[k.key] = k;
        }, Qt::QueuedConnection);
        kb.calibrate([run] { return !run->stop; }, [this](std::span<const akko::CalibrationSample> s) {
            std::vector<akko::CalibrationSample> samples(s.begin(), s.end());
            QMetaObject::invokeMethod(this, [this, samples] {
                bool changed = false;
                for (const auto &x : samples) {
                    const auto it = m_calibrationBefore.find(x.key);
                    if (it == m_calibrationBefore.end() || it->second.rest <= it->second.bottom)
                        continue;
                    const double range = it->second.rest - it->second.bottom;
                    const int depth = int(std::clamp((it->second.rest - double(x.lowest)) / range * 100.0, 0.0, 100.0));
                    const QString id = QString::number(x.key);
                    if (m_calibrationDepth.value(id, -1).toInt() != depth) {
                        m_calibrationDepth[id] = depth;
                        changed = true;
                    }
                }
                if (changed)
                    emit calibrationChanged();
            }, Qt::QueuedConnection);
        });
        const auto after = kb.readCalibration();
        int saved = 0;
        for (std::size_t i = 0; i < after.size() && i < before.size(); ++i)
            saved += after[i].bottom != before[i].bottom;
        QMetaObject::invokeMethod(this, [this, saved] {
            m_calibrationSaved = saved;
            emit calibrationChanged();
        }, Qt::QueuedConnection);
    });
}

void Backend::finishCalibration()
{
    if (!m_calibrationRun)
        return;
    m_calibrationRun->stop = true;
    m_calibrationRun.reset();
    m_calibrating = false;
    emit calibrationChanged();
}

void Backend::applyError(const QString &message)
{
    if (m_calibrationRun)
        finishCalibration();  // the session ended with the batch (Finish was sent)
    if (m_restarting && m_restartClock.elapsed() < kRestartGiveUpMs) {
        m_restartWait.start(300);  // not back yet: try again soon, without the error banner
        return;
    }
    if (std::exchange(m_restarting, false))
        emit connectionChanged();
    m_connected = false;
    m_errorKind = classifyError(message);
    m_errorDetail = message;
    // Unplugged with a udev watch running: the watch says when it's back; retry rarely, in case
    // an event is missed. Busy, permission and other errors get no udev event: retry often.
    m_retry.start(m_hotplugWatched && m_errorKind == "notFound" ? kHotplugFallbackMs : kRetryMs);
    emit connectionChanged();
}
