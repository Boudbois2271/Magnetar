// SPDX-License-Identifier: GPL-3.0-or-later
// A pretend keyboard for tests: records every report written and answers from a script.
#pragma once

#include "frame.h"
#include "keymap.h"
#include "travel.h"
#include "transport.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <vector>

namespace akko::test {

using Bytes = std::vector<std::uint8_t>;

// A 64-byte input report answering cmd/sub with the given payload bytes.
inline Bytes reply(std::uint8_t cmd, std::uint8_t sub, std::initializer_list<std::uint8_t> payload = {})
{
    Bytes r(kReportLen, 0);
    r[0] = kReportId;
    r[1] = cmd;
    r[2] = sub;
    r[4] = 0x01;
    r[6] = static_cast<std::uint8_t>(payload.size());
    std::size_t i = 7;
    for (auto b : payload)
        r[i++] = b;
    return r;
}

class FakeTransport : public Transport {
public:
    // Called on every write with the report sent; returns what the keyboard "sends back".
    std::function<std::vector<Bytes>(const Bytes &)> respond;

    std::vector<Bytes> written;  // every report sent, report ID included
    std::deque<Bytes> pending;   // input not read yet
    int drains = 0;

    void drain() override
    {
        ++drains;
        pending.clear();
    }

    void write(std::span<const std::uint8_t> report) override
    {
        written.emplace_back(report.begin(), report.end());
        if (respond)
            for (auto &r : respond(written.back()))
                pending.push_back(std::move(r));
    }

    std::optional<Bytes> read(int) override
    {
        if (pending.empty())
            return std::nullopt;  // a timeout, without the wait
        Bytes r = std::move(pending.front());
        pending.pop_front();
        return r;
    }
};

// A keyboard that echoes writes and answers reads from a small state, like the real one.
struct FakeKeyboard {
    std::uint8_t profile = 0;
    std::uint8_t profileCount = 3;
    bool silentProfileCount = false;  // never answers 82 0f
    std::array<std::uint8_t, 7> lighting{0x02, 0x00, 0x00, 0xff, 0x00, 20, 0x04};
    std::map<std::uint16_t, Action> keymap[kLayerCount];  // starts as the factory keymap
    bool ignoreKeyWrites = false;                          // echoes 03 LL but keeps the old action
    std::map<std::uint16_t, KeyTravel> travel;  // starts as read in akko-capture2.txt
    bool ignoreTravelWrites = false;            // echoes 13/16/19 but keeps the old values
    // Advanced keys: key → (type, payload as written). A SOCD pair is stored on both keys.
    std::map<std::uint16_t, std::pair<std::uint8_t, Bytes>> advanced;
    bool ignoreAdvancedWrites = false;  // echoes 12 00 but keeps the old records

    FakeKeyboard()
    {
        for (int l = 0; l < kLayerCount; ++l)
            for (const auto &k : layout())
                keymap[l][k.id] = factoryAction(l, k.id);
        for (const auto &k : layout()) {
            KeyTravel t;
            t.key = k.id;
            t.actuation = 0x78;  // 0.6 mm
            t.deadTop = 0x1a;
            t.deadBottom = 0x19;
            travel[k.id] = t;
        }
    }

    // 93 / 96 / 99: answers each key ID of the request with its entry, packet header kept.
    Bytes travelRead(const Bytes &w)
    {
        Bytes r(kReportLen, 0);
        std::copy_n(w.begin(), 6, r.begin());
        std::size_t at = 7;
        auto put = [&](std::uint16_t v) {
            r[at++] = static_cast<std::uint8_t>(v >> 8);
            r[at++] = static_cast<std::uint8_t>(v & 0xFF);
        };
        for (std::size_t i = 7; i + 1 < std::size_t(7) + w[6]; i += 2) {
            const std::uint16_t id = static_cast<std::uint16_t>(w[i] << 8 | w[i + 1]);
            const KeyTravel &t = travel[id];
            put(id);
            if (w[1] == 0x93) {
                put(t.actuation);
                r[at++] = t.actuationTail;
            } else if (w[1] == 0x96) {
                put(t.deadTop);
                put(t.deadBottom);
                put(t.deadZones ? 1 : 0);
            } else {
                r[at++] = t.rapidTrigger ? 1 : 0;
                put(t.press);
                put(t.release);
                r[at++] = t.rapidTail;
            }
        }
        r[6] = static_cast<std::uint8_t>(at - 7);
        return r;
    }

    // 13 / 16 / 19: stores every entry of the packet.
    void travelWrite(const Bytes &w)
    {
        const std::size_t entry = w[1] == 0x13 ? 5 : 8;
        auto be = [&](std::size_t i) { return static_cast<std::uint16_t>(w[i] << 8 | w[i + 1]); };
        for (std::size_t i = 7; i + entry <= std::size_t(7) + w[6]; i += entry) {
            KeyTravel &t = travel[be(i)];
            if (w[1] == 0x13) {
                t.actuation = be(i + 2);
                t.actuationTail = w[i + 4];
            } else if (w[1] == 0x16) {
                t.deadTop = be(i + 2);
                t.deadBottom = be(i + 4);
                t.deadZones = be(i + 6) != 0;
            } else {
                t.rapidTrigger = w[i + 2] != 0;
                t.press = be(i + 3);
                t.release = be(i + 5);
                t.rapidTail = w[i + 7];
            }
        }
    }

    // 92 00: with no key, the list of keys that have an advanced key; with one, its record
    // (type in the arg byte, all zero when it has none).
    Bytes advancedRead(const Bytes &w)
    {
        Bytes r(kReportLen, 0);
        std::copy_n(w.begin(), 6, r.begin());
        std::size_t at = 7;
        if (w[6] == 0) {
            for (const auto &[id, rec] : advanced) {
                r[at++] = static_cast<std::uint8_t>(id >> 8);
                r[at++] = static_cast<std::uint8_t>(id & 0xFF);
            }
        } else {
            const std::uint16_t id = static_cast<std::uint16_t>(w[7] << 8 | w[8]);
            const auto found = advanced.find(id);
            if (found == advanced.end()) {
                r[at++] = w[7];
                r[at++] = w[8];
            } else {
                r[3] = found->second.first;
                for (auto b : found->second.second)
                    r[at++] = b;
            }
        }
        r[6] = static_cast<std::uint8_t>(at - 7);
        return r;
    }

    // 12 00: type 00 clears one key; SOCD (04) lands on both of its keys.
    void advancedWrite(const Bytes &w)
    {
        const std::uint8_t type = w[3];
        const Bytes payload(w.begin() + 7, w.begin() + 7 + w[6]);
        auto be = [&](std::size_t i) { return static_cast<std::uint16_t>(payload[i] << 8 | payload[i + 1]); };
        if (type == 0x00)
            advanced.erase(be(0));
        else if (type == 0x04)
            advanced[be(1)] = advanced[be(3)] = {type, payload};
        else
            advanced[be(0)] = {type, payload};
    }

    std::vector<Bytes> operator()(const Bytes &w)
    {
        const std::uint8_t cmd = w[1], sub = w[2];
        if (cmd == 0x92 && sub == 0x00)
            return {advancedRead(w)};
        if (cmd == 0x12 && sub == 0x00 && !ignoreAdvancedWrites)
            advancedWrite(w);
        if (cmd == 0x83 && sub < kLayerCount) {
            // Answers each key ID listed in the request with key + action, packet header kept.
            Bytes r(kReportLen, 0);
            std::copy_n(w.begin(), 6, r.begin());
            std::size_t at = 7;
            for (std::size_t i = 7; i + 1 < std::size_t(7) + w[6]; i += 2) {
                const std::uint16_t id = static_cast<std::uint16_t>(w[i] << 8 | w[i + 1]);
                const Action a = keymap[sub][id];
                r[at++] = w[i];
                r[at++] = w[i + 1];
                for (auto b : a)
                    r[at++] = b;
            }
            r[6] = static_cast<std::uint8_t>(at - 7);
            return {r};
        }
        if ((cmd == 0x93 || cmd == 0x96 || cmd == 0x99) && sub == 0x00)
            return {travelRead(w)};
        if ((cmd == 0x13 || cmd == 0x16 || cmd == 0x19) && sub == 0x00 && !ignoreTravelWrites)
            travelWrite(w);
        if (cmd == 0x03 && sub < kLayerCount && !ignoreKeyWrites)
            std::copy_n(w.begin() + 9, 4, keymap[sub][static_cast<std::uint16_t>(w[7] << 8 | w[8])].begin());
        if (cmd == 0x90 && sub == 0x00)
            return {reply(cmd, sub, {profile})};
        if (cmd == 0x82 && sub == 0x0f) {
            if (silentProfileCount)
                return {};
            return {reply(cmd, sub, {0x00, 0x00, 0x00, profileCount})};
        }
        if (cmd == 0x82 && sub == 0x02)
            return {reply(cmd, sub, {0x08, 0x01})};
        if (cmd == 0x84 && sub == 0x01) {
            const auto &l = lighting;
            return {reply(cmd, sub, {l[0], l[1], l[2], l[3], l[4], l[5], l[6]})};
        }
        if (cmd == 0x10 && sub == 0x00)
            profile = w[7];
        else if (cmd == 0x04 && sub == 0x01)
            std::copy_n(w.begin() + 7, 7, lighting.begin());
        else if (cmd == 0x04 && sub == 0x02)
            lighting[0] = w[7];
        Bytes echo(w);  // writes are echoed back unchanged
        return {echo};
    }
};

}  // namespace akko::test
