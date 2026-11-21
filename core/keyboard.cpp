// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyboard.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace akko {

Frame Keyboard::transact(std::uint8_t cmd, std::uint8_t sub, std::span<const std::uint8_t> payload,
                         int timeoutMs, int retries)
{
    return transactPacket(cmd, sub, 1, 0, payload, timeoutMs, retries);
}

Frame Keyboard::transactPacket(std::uint8_t cmd, std::uint8_t sub, std::uint8_t packetCount,
                               std::uint8_t packetIndex, std::span<const std::uint8_t> payload, int timeoutMs,
                               int retries, std::uint8_t arg)
{
    using clock = std::chrono::steady_clock;
    const Frame frame = buildFrame(cmd, sub, payload, packetCount, packetIndex, arg);
    std::array<std::uint8_t, kReportLen> report{};
    report[0] = kReportId;
    std::copy(frame.begin(), frame.end(), report.begin() + 1);

    for (int attempt = 0; attempt <= retries; ++attempt) {
        t_.drain();
        t_.write(report);
        const auto deadline = clock::now() + std::chrono::milliseconds(timeoutMs);
        for (;;) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now());
            if (left.count() <= 0)
                break;
            const auto data = t_.read(static_cast<int>(left.count()));
            if (!data)
                break;  // timed out: send again
            if (isReplyTo(*data, cmd, sub, packetIndex)) {
                Frame answer{};
                std::copy(data->begin() + 1, data->end(), answer.begin());
                return answer;
            }
            // Anything else (key events, another command's answer) is skipped.
        }
    }
    throw Error("no response to command " + commandName(cmd, sub));
}

int Keyboard::profile()
{
    return transact(0x90, 0x00)[kPayloadOffset];
}

int Keyboard::profileCount()
{
    int n = 0;
    try {
        n = transact(0x82, 0x0F)[kPayloadOffset + 3];
    } catch (const Error &) {
        n = 0;
    }
    return (n >= 1 && n <= 8) ? n : 3;
}

void Keyboard::setProfile(int index)
{
    const std::uint8_t p[] = {static_cast<std::uint8_t>(index)};
    transact(0x10, 0x00, p);
}

Lighting Keyboard::lighting()
{
    const Frame r = transact(0x84, 0x01);
    Lighting l;
    std::copy_n(r.begin() + kPayloadOffset, l.bytes.size(), l.bytes.begin());
    return l;
}

void Keyboard::setLighting(const Lighting &l)
{
    transact(0x04, 0x01, l.bytes);
}

void Keyboard::setEffect(int effect)
{
    const std::uint8_t p[] = {static_cast<std::uint8_t>(effect)};
    transact(0x04, 0x02, p);
}

std::uint16_t Keyboard::firmwareVersion()
{
    // The two bytes read 08 01 for v1.08: low byte first, unlike the rest of the protocol.
    const Frame r = transact(0x82, 0x02);
    return static_cast<std::uint16_t>(r[kPayloadOffset] | (r[kPayloadOffset + 1] << 8));
}

namespace {

void checkLayer(int layer)
{
    if (layer < 0 || layer >= kLayerCount)
        throw Error("no such layer: " + std::to_string(layer));
}

std::uint16_t be16(const Frame &f, std::size_t at)
{
    return static_cast<std::uint16_t>(f[at] << 8 | f[at + 1]);
}

}  // namespace

std::vector<std::pair<std::uint16_t, Action>> Keyboard::readLayer(int layer)
{
    checkLayer(layer);
    constexpr std::size_t kPerPacket = 9, kEntry = 6;  // key u16 + action
    const auto order = keyReadOrder();
    const auto packets = static_cast<std::uint8_t>((order.size() + kPerPacket - 1) / kPerPacket);
    std::vector<std::pair<std::uint16_t, Action>> keys;
    for (std::uint8_t p = 0; p < packets; ++p) {
        const auto chunk = order.subspan(p * kPerPacket, std::min(kPerPacket, order.size() - p * kPerPacket));
        std::vector<std::uint8_t> request;
        for (auto id : chunk) {
            request.push_back(static_cast<std::uint8_t>(id >> 8));
            request.push_back(static_cast<std::uint8_t>(id & 0xFF));
        }
        const Frame r = transactPacket(0x83, static_cast<std::uint8_t>(layer), packets, p, request);
        const std::size_t len = std::min<std::size_t>(r[5], kMaxPayload);
        for (std::size_t at = kPayloadOffset; at + kEntry <= kPayloadOffset + len; at += kEntry) {
            const std::uint16_t id = be16(r, at);
            if (std::find(chunk.begin(), chunk.end(), id) == chunk.end())
                continue;  // not a key we asked for in this packet
            Action a;
            std::copy_n(r.begin() + at + 2, 4, a.begin());
            keys.emplace_back(id, a);
        }
    }
    if (keys.size() != order.size())
        throw Error("incomplete keymap read on layer " + std::to_string(layer));
    return keys;
}

Action Keyboard::readKey(int layer, std::uint16_t key)
{
    checkLayer(layer);
    const std::uint8_t p[] = {static_cast<std::uint8_t>(key >> 8), static_cast<std::uint8_t>(key & 0xFF)};
    const Frame r = transact(0x83, static_cast<std::uint8_t>(layer), p);
    if (be16(r, kPayloadOffset) != key)
        throw Error("keymap read answered for another key");
    Action a;
    std::copy_n(r.begin() + kPayloadOffset + 2, 4, a.begin());
    return a;
}

Action Keyboard::setKey(int layer, std::uint16_t key, const Action &action)
{
    checkLayer(layer);
    const std::uint8_t p[] = {static_cast<std::uint8_t>(key >> 8), static_cast<std::uint8_t>(key & 0xFF),
                              action[0], action[1], action[2], action[3]};
    transact(0x03, static_cast<std::uint8_t>(layer), p);
    const Action back = readKey(layer, key);
    if (back != action)
        throw Error("the keyboard kept " + actionHex(back) + " instead of " + actionHex(action));
    return back;
}

// ---- per-key travel -------------------------------------------------------

namespace {

void put16(std::vector<std::uint8_t> &out, std::uint16_t v)
{
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

constexpr std::size_t kActuationPerPacket = 11, kActuationEntry = 5;  // key, actuation, tail
constexpr std::size_t kWidePerPacket = 7, kWideEntry = 8;             // RT and dead zone entries

}  // namespace

void Keyboard::perKey(std::uint8_t cmd, std::size_t perPacket, std::size_t entryLen,
                      std::span<const std::uint16_t> keys,
                      const std::function<void(std::vector<std::uint8_t> &, std::uint16_t)> &encode,
                      const std::function<void(const Frame &, std::size_t)> &onEntry)
{
    const auto packets = static_cast<std::uint8_t>((keys.size() + perPacket - 1) / perPacket);
    for (std::uint8_t p = 0; p < packets; ++p) {
        const auto chunk = keys.subspan(p * perPacket, std::min(perPacket, keys.size() - p * perPacket));
        std::vector<std::uint8_t> request;
        for (auto id : chunk)
            encode(request, id);
        const Frame r = transactPacket(cmd, 0x00, packets, p, request);
        const std::size_t len = std::min<std::size_t>(r[5], kMaxPayload);
        for (std::size_t at = kPayloadOffset; at + entryLen <= kPayloadOffset + len; at += entryLen)
            if (std::find(chunk.begin(), chunk.end(), be16(r, at)) != chunk.end())
                onEntry(r, at);
    }
}

std::vector<KeyTravel> Keyboard::readTravel(std::span<const std::uint16_t> keys)
{
    std::vector<KeyTravel> out(keys.size());
    std::vector<int> seen(keys.size(), 0);  // one bit per table
    auto slot = [&](std::uint16_t id) -> std::size_t {
        return static_cast<std::size_t>(std::find(keys.begin(), keys.end(), id) - keys.begin());
    };
    const auto ids = [](std::vector<std::uint8_t> &req, std::uint16_t id) { put16(req, id); };

    perKey(0x93, kActuationPerPacket, kActuationEntry, keys, ids, [&](const Frame &f, std::size_t at) {
        const auto i = slot(be16(f, at));
        out[i].actuation = be16(f, at + 2);
        out[i].actuationTail = f[at + 4];
        seen[i] |= 1;
    });
    perKey(0x96, kWidePerPacket, kWideEntry, keys, ids, [&](const Frame &f, std::size_t at) {
        const auto i = slot(be16(f, at));
        out[i].deadTop = be16(f, at + 2);
        out[i].deadBottom = be16(f, at + 4);
        out[i].deadZones = be16(f, at + 6) != 0;
        seen[i] |= 2;
    });
    perKey(0x99, kWidePerPacket, kWideEntry, keys, ids, [&](const Frame &f, std::size_t at) {
        const auto i = slot(be16(f, at));
        out[i].rapidTrigger = f[at + 2] != 0;
        out[i].press = be16(f, at + 3);
        out[i].release = be16(f, at + 5);
        out[i].rapidTail = f[at + 7];
        seen[i] |= 4;
    });
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (seen[i] != 7)
            throw Error("incomplete travel read");
        out[i].key = keys[i];
    }
    return out;
}

std::vector<KeyTravel> Keyboard::setTravel(TravelPart part, std::span<const KeyTravel> keys)
{
    std::vector<std::uint16_t> ids;
    for (const auto &t : keys) {
        if (const auto problem = travelProblem(t, part); !problem.empty())
            throw Error(problem);
        ids.push_back(t.key);
    }
    auto entry = [&](std::uint16_t id) -> const KeyTravel & {
        return *std::find_if(keys.begin(), keys.end(), [&](const KeyTravel &t) { return t.key == id; });
    };
    const auto ignore = [](const Frame &, std::size_t) {};

    switch (part) {
    case TravelPart::Actuation:
        perKey(0x13, kActuationPerPacket, kActuationEntry, ids,
               [&](std::vector<std::uint8_t> &out, std::uint16_t id) {
                   const auto &t = entry(id);
                   put16(out, id);
                   put16(out, t.actuation);
                   out.push_back(t.actuationTail);
               },
               ignore);
        break;
    case TravelPart::RapidTrigger:
        perKey(0x19, kWidePerPacket, kWideEntry, ids,
               [&](std::vector<std::uint8_t> &out, std::uint16_t id) {
                   const auto &t = entry(id);
                   put16(out, id);
                   out.push_back(t.rapidTrigger ? 1 : 0);
                   put16(out, t.press);
                   put16(out, t.release);
                   out.push_back(t.rapidTail);
               },
               ignore);
        break;
    case TravelPart::DeadZones:
        for (std::size_t i = 0; i < ids.size(); ++i)
            perKey(0x16, kWidePerPacket, kWideEntry, std::span(ids).subspan(i, 1),
                   [&](std::vector<std::uint8_t> &out, std::uint16_t id) {
                       const auto &t = entry(id);
                       put16(out, id);
                       put16(out, t.deadTop);
                       put16(out, t.deadBottom);
                       put16(out, t.deadZones ? 1 : 0);
                   },
                   ignore);
        break;
    }

    auto back = readTravel(ids);
    for (std::size_t i = 0; i < back.size(); ++i)
        if (!samePart(back[i], keys[i], part))
            throw Error("the keyboard kept other travel values on key " + std::to_string(back[i].key));
    return back;
}

std::string formatFirmware(std::uint16_t version)
{
    char buf[16];
    std::snprintf(buf, sizeof buf, "v%x.%02x", version >> 8, version & 0xFF);
    return buf;
}

// ---- advanced keys --------------------------------------------------------

namespace {

bool covers(const AdvancedKey &a, std::uint16_t key)
{
    const auto ids = advancedKeyIds(a);
    return std::find(ids.begin(), ids.end(), key) != ids.end();
}

bool onBoard(std::uint16_t id)
{
    const auto keys = layout();
    return std::any_of(keys.begin(), keys.end(), [&](const KeyPosition &k) { return k.id == id; });
}

}  // namespace

std::vector<std::uint16_t> Keyboard::advancedKeyList()
{
    std::vector<std::uint16_t> ids;
    auto collect = [&](const Frame &r) {
        const std::size_t len = std::min<std::size_t>(r[5], kMaxPayload);
        for (std::size_t at = kPayloadOffset; at + 2 <= kPayloadOffset + len; at += 2) {
            const std::uint16_t id = be16(r, at);
            // 0 and IDs that aren't keys are skipped: the oldest capture answered this read
            // with the layout table, padded with zeros.
            if (onBoard(id) && std::find(ids.begin(), ids.end(), id) == ids.end())
                ids.push_back(id);
        }
    };
    const Frame first = transact(0x92, 0x00);
    collect(first);
    // More packets than one: ask for the rest, as the vendor app does (92 00 00 nn ii).
    for (std::uint8_t p = 1; p < first[3]; ++p)
        collect(transactPacket(0x92, 0x00, first[3], p));
    return ids;
}

AdvancedKey Keyboard::readAdvanced(std::uint16_t key)
{
    const std::uint8_t p[] = {static_cast<std::uint8_t>(key >> 8), static_cast<std::uint8_t>(key & 0xFF)};
    const Frame r = transact(0x92, 0x00, p);
    const std::uint8_t type = r[2];
    if (type == 0)
        return makeAdvanced(AdvancedType::None, key);
    const std::size_t len = std::min<std::size_t>(r[5], kMaxPayload);
    const auto a = decodeAdvanced(type, std::span(r).subspan(kPayloadOffset, len));
    if (!a)
        throw Error("unreadable advanced key on key " + std::to_string(key));
    if (!covers(*a, key))
        throw Error("advanced key read answered for another key");
    return *a;
}

std::vector<AdvancedKey> Keyboard::readAdvancedKeys()
{
    std::vector<AdvancedKey> out;
    for (auto id : advancedKeyList()) {
        if (std::any_of(out.begin(), out.end(), [&](const AdvancedKey &a) { return covers(a, id); }))
            continue;  // the other key of a SOCD pair already read
        AdvancedKey a = readAdvanced(id);
        if (a.type != AdvancedType::None)
            out.push_back(std::move(a));
    }
    return out;
}

void Keyboard::clearKeys(std::span<const std::uint16_t> keys, const std::vector<AdvancedKey> &current)
{
    std::vector<std::uint16_t> cleared;
    for (auto key : keys) {
        const auto held = std::find_if(current.begin(), current.end(), [&](const AdvancedKey &a) { return covers(a, key); });
        if (held == current.end())
            continue;
        for (auto id : advancedKeyIds(*held)) {
            if (std::find(cleared.begin(), cleared.end(), id) != cleared.end())
                continue;
            const auto payload = encodeAdvanced(makeAdvanced(AdvancedType::None, id));
            transactPacket(0x12, 0x00, 1, 0, payload, 1000, 2, 0x00);
            cleared.push_back(id);
        }
    }
}

std::vector<AdvancedKey> Keyboard::setAdvanced(const AdvancedKey &a)
{
    if (const auto problem = advancedProblem(a); !problem.empty())
        throw Error(problem);
    const auto ids = advancedKeyIds(a);
    clearKeys(ids, readAdvancedKeys());
    transactPacket(0x12, 0x00, 1, 0, encodeAdvanced(a), 1000, 2, static_cast<std::uint8_t>(a.type));
    auto after = readAdvancedKeys();
    const auto back = std::find_if(after.begin(), after.end(), [&](const AdvancedKey &b) { return covers(b, a.key); });
    if (back == after.end() || *back != a)
        throw Error("the keyboard kept another advanced key on key " + std::to_string(a.key));
    return after;
}

std::vector<AdvancedKey> Keyboard::clearAdvanced(std::uint16_t key)
{
    const std::uint16_t keys[] = {key};
    clearKeys(keys, readAdvancedKeys());
    auto after = readAdvancedKeys();
    if (std::any_of(after.begin(), after.end(), [&](const AdvancedKey &b) { return covers(b, key); }))
        throw Error("the keyboard kept the advanced key on key " + std::to_string(key));
    return after;
}

}  // namespace akko
