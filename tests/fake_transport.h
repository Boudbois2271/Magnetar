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
    std::map<std::uint8_t, std::uint8_t> base{{0x15, 0}, {0x17, 0}, {0x18, 1}, {0x19, 1}};  // 04/84 xx, stock
    bool ignoreBaseWrites = false;  // echoes 04 xx but keeps the old value
    bool ignoreKeyWrites = false;                          // echoes 03 LL but keeps the old action
    std::map<std::uint16_t, KeyTravel> travel;  // starts as read in akko-capture2.txt
    bool ignoreTravelWrites = false;            // echoes 13/16/19 but keeps the old values
    std::map<std::uint16_t, std::array<std::uint8_t, 3>> colors;  // Custom effect, all black at first
    bool ignoreColorWrites = false;                               // echoes 06 00 but keeps the old colours
    // Advanced keys: key → (type, payload as written). A SOCD pair is stored on both keys.
    std::map<std::uint16_t, std::pair<std::uint8_t, Bytes>> advanced;
    bool ignoreAdvancedWrites = false;  // echoes 12 00 but keeps the old records
    Bytes macros;                       // the macro blob, empty = no macros (reads ff ff)
    bool ignoreMacroWrites = false;     // echoes 05 00 but keeps the old blob
    Bytes macroIncoming;                // 05 00 packets gathered until the last one
    // Calibration: key → (rest, bottom), every key 10500 / 1800 at first. A 94 00 keep-alive also
    // sends the reports waiting in calibrationReports (94 02, as captured); 94 04 stores the
    // lowest value each key reached in them as its bottom.
    std::map<std::uint16_t, std::pair<std::uint16_t, std::uint16_t>> calibration;
    std::deque<Bytes> calibrationReports;
    std::map<std::uint16_t, std::uint16_t> calibrationLowest;
    int keepAlives = 0, finishes = 0;
    std::vector<Bytes> mural;  // 08 04 packets received (never echoed, like the real board)

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
            calibration[k.id] = {10500, 1800};
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

    // 85 00: offset in sub (high) and arg (low), len = bytes wanted, packet header kept.
    Bytes macroRead(const Bytes &w)
    {
        Bytes r(kReportLen, 0);
        std::copy_n(w.begin(), 6, r.begin());
        const std::size_t offset = static_cast<std::size_t>(w[2] << 8 | w[3]), len = w[6];
        r[6] = static_cast<std::uint8_t>(len);
        if (macros.empty()) {
            r[7] = r[8] = 0xff;
            return r;
        }
        for (std::size_t i = 0; i < len && offset + i < macros.size(); ++i)
            r[7 + i] = macros[offset + i];
        return r;
    }

    // 05 00: packet i holds blob bytes i × 56 …; the blob is replaced on the last packet.
    void macroWrite(const Bytes &w)
    {
        const std::size_t count = w[4], index = w[5], len = w[6];
        if (index == 0)
            macroIncoming.clear();
        macroIncoming.insert(macroIncoming.end(), w.begin() + 7, w.begin() + 7 + static_cast<std::ptrdiff_t>(len));
        if (index + 1 == count && !ignoreMacroWrites)
            macros = macroIncoming;
    }

    std::vector<Bytes> operator()(const Bytes &w)
    {
        const std::uint8_t cmd = w[1], sub = w[2];
        if (cmd == 0x08 && sub == 0x04) {
            mural.push_back(w);
            return {};
        }
        if (cmd == 0x85)
            return {macroRead(w)};
        if (cmd == 0x05 && sub == 0x00)
            macroWrite(w);
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
        if (cmd == 0x94 && sub == 0x05) {
            // Answers each key ID of the request with key, rest, bottom, packet header kept.
            Bytes r(kReportLen, 0);
            std::copy_n(w.begin(), 6, r.begin());
            std::size_t at = 7;
            auto put = [&](std::uint16_t v) {
                r[at++] = static_cast<std::uint8_t>(v >> 8);
                r[at++] = static_cast<std::uint8_t>(v & 0xFF);
            };
            for (std::size_t i = 7; i + 1 < std::size_t(7) + w[6]; i += 2) {
                const std::uint16_t id = static_cast<std::uint16_t>(w[i] << 8 | w[i + 1]);
                put(id);
                put(calibration[id].first);
                put(calibration[id].second);
            }
            r[6] = static_cast<std::uint8_t>(at - 7);
            return {r};
        }
        if (cmd == 0x94 && sub == 0x00) {
            ++keepAlives;
            std::vector<Bytes> out{Bytes(w)};
            while (!calibrationReports.empty()) {
                const Bytes rep = calibrationReports.front();
                calibrationReports.pop_front();
                for (std::size_t at = 7; at + 6 <= std::size_t(7) + rep[6]; at += 6) {
                    const std::uint16_t id = static_cast<std::uint16_t>(rep[at] << 8 | rep[at + 1]);
                    const std::uint16_t low = static_cast<std::uint16_t>(rep[at + 4] << 8 | rep[at + 5]);
                    auto it = calibrationLowest.find(id);
                    if (it == calibrationLowest.end() || low < it->second)
                        calibrationLowest[id] = low;
                }
                out.push_back(rep);
            }
            return out;
        }
        if (cmd == 0x94 && sub == 0x04) {
            ++finishes;
            for (auto [id, low] : calibrationLowest)
                calibration[id].second = low;
            calibrationLowest.clear();
        }
        if (cmd == 0x86 && sub == 0x00) {
            // Answers each key ID of the request with key, R, G, B, packet header kept.
            Bytes r(kReportLen, 0);
            std::copy_n(w.begin(), 6, r.begin());
            std::size_t at = 7;
            for (std::size_t i = 7; i + 1 < std::size_t(7) + w[6]; i += 2) {
                r[at++] = w[i];
                r[at++] = w[i + 1];
                for (auto b : colors[static_cast<std::uint16_t>(w[i] << 8 | w[i + 1])])
                    r[at++] = b;
            }
            r[6] = static_cast<std::uint8_t>(at - 7);
            return {r};
        }
        if (cmd == 0x06 && sub == 0x00 && !ignoreColorWrites)
            for (std::size_t i = 7; i + 5 <= std::size_t(7) + w[6]; i += 5)
                std::copy_n(w.begin() + i + 2, 3, colors[static_cast<std::uint16_t>(w[i] << 8 | w[i + 1])].begin());
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
        if (cmd == 0x84 && base.count(sub))
            return {reply(cmd, sub, {base[sub]})};
        if (cmd == 0x04 && base.count(sub) && !ignoreBaseWrites)
            base[sub] = w[7];
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
