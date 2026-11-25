// SPDX-License-Identifier: GPL-3.0-or-later
#include "advanced.h"

#include "travel.h"

#include <algorithm>

namespace akko {

std::array<std::uint8_t, kDksStages> dksStages(DksStroke s)
{
    std::array<std::uint8_t, kDksStages> out{};
    if (s.from < 0 || s.from >= int(kDksStages) || s.to < s.from || s.to >= int(kDksStages))
        return out;
    if (s.from == s.to) {
        out[s.from] = kStageDown | kStageUp;  // 0a: fire once
        return out;
    }
    // Held: down and held at the first stage, held through the middle ones, and the stage
    // before the last one says "up at the next stage" (0e for a one-stage hold, 06 04 0c 00
    // for a hold across all four, akko-capture10.txt).
    out[s.from] = kStageDown | kStageHold;
    for (int i = s.from + 1; i < s.to; ++i)
        out[i] = kStageHold;
    out[s.to - 1] |= kStageUp;
    return out;
}

std::optional<DksStroke> dksStroke(const std::array<std::uint8_t, kDksStages> &stages)
{
    if (std::all_of(stages.begin(), stages.end(), [](auto b) { return b == 0; }))
        return DksStroke{};
    for (int from = 0; from < int(kDksStages); ++from)
        for (int to = from; to < int(kDksStages); ++to)
            if (dksStages({from, to}) == stages)
                return DksStroke{from, to};
    return std::nullopt;
}

const std::vector<DksPattern> &dksPatterns()
{
    static const std::vector<DksPattern> all = [] {
        // Every set of disjoint strokes starting at stage `first` or later.
        std::vector<DksPattern> rest[kDksStages + 1];
        rest[kDksStages] = {{}};
        for (int first = int(kDksStages) - 1; first >= 0; --first) {
            rest[first] = rest[first + 1];
            for (int to = first; to < int(kDksStages); ++to)
                for (const auto &tail : rest[to + 1]) {
                    DksPattern p{{first, to}};
                    p.insert(p.end(), tail.begin(), tail.end());
                    rest[first].push_back(p);
                }
        }
        // Not used, then single strokes in dksStroke()'s order (once at 0..3, then held a → b),
        std::vector<DksPattern> out{{}};
        for (int i = 0; i < int(kDksStages); ++i)
            out.push_back({{i, i}});
        for (int a = 0; a < int(kDksStages); ++a)
            for (int b = a + 1; b < int(kDksStages); ++b)
                out.push_back({{a, b}});
        // then the rest by number of strokes, each group in stage order.
        const auto key = [](const DksPattern &p) {
            std::vector<int> k;
            for (const auto &st : p)
                k.insert(k.end(), {st.from, st.to});
            return k;
        };
        for (std::size_t n = 2; n <= kDksStages; ++n) {
            std::vector<DksPattern> group;
            for (const auto &p : rest[0])
                if (p.size() == n)
                    group.push_back(p);
            std::sort(group.begin(), group.end(), [&](const auto &a, const auto &b) { return key(a) < key(b); });
            out.insert(out.end(), group.begin(), group.end());
        }
        return out;
    }();
    return all;
}

std::array<std::uint8_t, kDksStages> dksPatternStages(const DksPattern &p)
{
    std::array<std::uint8_t, kDksStages> out{};
    int free = 0;
    for (const auto &s : p) {
        if (s.from < free || s.to < s.from || s.to >= int(kDksStages))
            return {};  // overlapping or out of range: nothing
        const auto one = dksStages(s);
        for (std::size_t i = 0; i < kDksStages; ++i)
            out[i] |= one[i];
        free = s.to + 1;
    }
    return out;
}

std::optional<std::size_t> dksPatternIndex(const std::array<std::uint8_t, kDksStages> &stages)
{
    const auto &all = dksPatterns();
    for (std::size_t i = 0; i < all.size(); ++i)
        if (dksPatternStages(all[i]) == stages)
            return i;
    return std::nullopt;
}

AdvancedKey makeAdvanced(AdvancedType type, std::uint16_t key)
{
    AdvancedKey a;
    a.type = type;
    a.key = key;
    return a;
}

std::vector<std::uint16_t> advancedKeyIds(const AdvancedKey &a)
{
    if (a.type == AdvancedType::Socd)
        return {a.key, a.key2};
    return {a.key};
}

namespace {

void put16(std::vector<std::uint8_t> &out, std::uint16_t v)
{
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

void putAction(std::vector<std::uint8_t> &out, const Action &a)
{
    out.insert(out.end(), a.begin(), a.end());
}

// Reads the payload front to back; ok() turns false on the first read past the end.
struct Reader {
    std::span<const std::uint8_t> p;
    std::size_t at = 0;
    bool short_ = false;

    std::uint8_t u8()
    {
        if (at + 1 > p.size()) {
            short_ = true;
            return 0;
        }
        return p[at++];
    }
    std::uint16_t u16()
    {
        const std::uint16_t hi = u8();
        return static_cast<std::uint16_t>(hi << 8 | u8());
    }
    Action action()
    {
        Action a;
        for (auto &b : a)
            b = u8();
        return a;
    }
    bool ok() const { return !short_; }
};

bool isKey(std::uint16_t id)
{
    const auto keys = layout();
    return std::any_of(keys.begin(), keys.end(), [&](const KeyPosition &k) { return k.id == id; });
}

bool depthOk(std::uint16_t d)
{
    return d >= kMinActuation && d <= kMaxActuation;
}

bool pointDepthOk(std::uint16_t d)
{
    return d >= kMinActuation && d <= kMaxPointDepth;
}

}  // namespace

std::vector<std::uint8_t> encodeAdvanced(const AdvancedKey &a)
{
    std::vector<std::uint8_t> out;
    switch (a.type) {
    case AdvancedType::None:
        put16(out, a.key);
        break;
    case AdvancedType::Toggle:
        put16(out, a.key);
        putAction(out, a.action);
        put16(out, a.holdMs);
        break;
    case AdvancedType::ModTap:
        put16(out, a.key);
        putAction(out, a.action);
        putAction(out, a.tap);
        put16(out, a.holdMs);
        break;
    case AdvancedType::Dks:
        put16(out, a.key);
        for (auto d : a.depths)
            put16(out, d);
        for (const auto &r : a.rows) {
            putAction(out, r.action);
            out.insert(out.end(), r.stages.begin(), r.stages.end());
        }
        break;
    case AdvancedType::Socd:
        out.push_back(a.socdLead);
        put16(out, a.key);
        put16(out, a.key2);
        out.push_back(static_cast<std::uint8_t>(a.mode));
        break;
    case AdvancedType::MultiPoint:
        put16(out, a.key);
        out.push_back(static_cast<std::uint8_t>(a.points.size()));
        for (const auto &pt : a.points) {
            putAction(out, pt.action);
            put16(out, pt.depth);
        }
        break;
    case AdvancedType::Release:
        put16(out, a.key);
        putAction(out, a.action);
        break;
    }
    return out;
}

std::optional<AdvancedKey> decodeAdvanced(std::uint8_t type, std::span<const std::uint8_t> payload)
{
    Reader r{payload};
    AdvancedKey a;
    a.type = static_cast<AdvancedType>(type);
    switch (a.type) {
    case AdvancedType::None:
        a.key = r.u16();
        break;
    case AdvancedType::Toggle:
        a.key = r.u16();
        a.action = r.action();
        a.holdMs = r.u16();
        break;
    case AdvancedType::ModTap:
        a.key = r.u16();
        a.action = r.action();
        a.tap = r.action();
        a.holdMs = r.u16();
        break;
    case AdvancedType::Dks:
        a.key = r.u16();
        for (auto &d : a.depths)
            d = r.u16();
        for (auto &row : a.rows) {
            row.action = r.action();
            for (auto &s : row.stages)
                s = r.u8();
        }
        break;
    case AdvancedType::Socd: {
        a.socdLead = r.u8();
        a.key = r.u16();
        a.key2 = r.u16();
        a.mode = static_cast<SocdMode>(r.u8());
        break;
    }
    case AdvancedType::MultiPoint: {
        a.key = r.u16();
        const std::uint8_t n = r.u8();
        for (std::uint8_t i = 0; i < n && r.ok(); ++i) {
            PointSlot pt;
            pt.action = r.action();
            pt.depth = r.u16();
            a.points.push_back(pt);
        }
        break;
    }
    case AdvancedType::Release:
        a.key = r.u16();
        a.action = r.action();
        break;
    default:
        return std::nullopt;
    }
    if (!r.ok())
        return std::nullopt;
    return a;
}

std::string advancedProblem(const AdvancedKey &a)
{
    const std::string key = "key " + std::to_string(a.key) + ": ";
    if (!isKey(a.key))
        return key + "no such key";
    const auto holdOk = a.holdMs >= kMinHoldMs && a.holdMs <= kMaxHoldMs;
    switch (a.type) {
    case AdvancedType::None:
        return key + "no advanced key type";
    case AdvancedType::Toggle:
        if (a.action == kNoAction)
            return key + "Toggle needs an action";
        if (!holdOk)
            return key + "hold time out of range";
        break;
    case AdvancedType::ModTap:
        if (a.action == kNoAction || a.tap == kNoAction)
            return key + "Mod-Tap needs a hold and a tap action";
        if (!holdOk)
            return key + "hold time out of range";
        break;
    case AdvancedType::Release:
        if (a.action == kNoAction)
            return key + "Release needs an action";
        break;
    case AdvancedType::Socd:
        if (!isKey(a.key2) || a.key2 == a.key)
            return key + "SOCD needs two different keys";
        if (a.mode != SocdMode::Neutral && a.mode != SocdMode::LastInput && a.mode != SocdMode::Key1 &&
            a.mode != SocdMode::Key2)
            return key + "unknown SOCD mode";
        break;
    case AdvancedType::MultiPoint:
        if (a.points.size() != kMultiPointSlots)
            return key + "Multi-Point needs three slots";
        if (std::all_of(a.points.begin(), a.points.end(), [](const PointSlot &p) { return p.action == kNoAction; }))
            return key + "Multi-Point needs at least one action";
        for (const auto &p : a.points)
            if (!pointDepthOk(p.depth))
                return key + "depth out of range";
        break;
    case AdvancedType::Dks: {
        if (!std::all_of(a.depths.begin(), a.depths.end(), depthOk))
            return key + "depth out of range";
        if (a.depths[0] >= a.depths[1] || a.depths[3] >= a.depths[2])
            return key + "a DKS start depth must be above its bottom depth";
        bool any = false;
        for (const auto &row : a.rows) {
            const bool used = std::any_of(row.stages.begin(), row.stages.end(), [](auto b) { return b != 0; });
            if (used != (row.action != kNoAction))
                return key + "every DKS row needs both an action and a stage";
            if (std::any_of(row.stages.begin(), row.stages.end(),
                            [](auto b) { return (b & ~(kStageDown | kStageHold | kStageUp)) != 0; }))
                return key + "unknown DKS stage bits";
            any = any || used;
        }
        if (!any)
            return key + "DKS needs at least one action";
        break;
    }
    default:
        return key + "unknown advanced key type";
    }
    return {};
}

}  // namespace akko
