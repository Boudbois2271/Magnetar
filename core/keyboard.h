// SPDX-License-Identifier: GPL-3.0-or-later
// The keyboard's commands, on top of any Transport. Only KNOWN writes live here
// (see the status labels in akko-protocol.md).
#pragma once

#include "advanced.h"
#include "frame.h"
#include "keymap.h"
#include "travel.h"
#include "transport.h"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace akko {

constexpr int kMaxBrightness = 20;
constexpr int kEffectCustom = 0x13;  // per-key colours from 06 00 (effect IDs are hex in akko-protocol.md)

// The 7-byte lighting block of 04 01 / 84 01.
struct Lighting {
    enum : std::size_t { Effect, ColorMode, Red, Green, Blue, Brightness, Speed };
    std::array<std::uint8_t, 7> bytes{};

    std::uint8_t &operator[](std::size_t i) { return bytes[i]; }
    std::uint8_t operator[](std::size_t i) const { return bytes[i]; }
    bool rainbow() const { return bytes[ColorMode] >= 7; }
};

// One key's colour for the Custom effect (kEffectCustom): entries of 06 00 / 86 00.
struct KeyColor {
    std::uint16_t key = 0;
    std::array<std::uint8_t, 3> rgb{};

    bool operator==(const KeyColor &) const = default;
};

// "v1.08" for 0x0108, as the vendor app's Device Info page writes it.
std::string formatFirmware(std::uint16_t version);

class Keyboard {
public:
    explicit Keyboard(Transport &transport) : t_(transport) {}

    // Send one frame and wait for the matching answer. Returns the answer's 63 bytes
    // (report ID stripped), so payload[n] is at index 6 + n. Writes are echoed back.
    Frame transact(std::uint8_t cmd, std::uint8_t sub, std::span<const std::uint8_t> payload = {},
                   int timeoutMs = 1000, int retries = 2);
    // The same for one packet of a multi-packet exchange; the answer must carry the same index.
    Frame transactPacket(std::uint8_t cmd, std::uint8_t sub, std::uint8_t packetCount, std::uint8_t packetIndex,
                         std::span<const std::uint8_t> payload = {}, int timeoutMs = 1000, int retries = 2,
                         std::uint8_t arg = 0);

    int profile();       // 0-based active profile (90 00)
    int profileCount();  // from 82 0f; 3 when the answer is missing or implausible
    void setProfile(int index);

    Lighting lighting();  // 84 01
    void setLighting(const Lighting &l);
    // 04 02: switch effect. The board loads that effect's own stored colour and speed,
    // so read lighting() again afterwards.
    void setEffect(int effect);

    // 82 02: firmware version, e.g. 0x0108 for the v1.08 the vendor app shows.
    std::uint16_t firmwareVersion();

    // Keymap of the active profile. Layer 0 Default, 1 Fn, 2 Fn1.
    // readLayer: 10 × 83 LL, 9 keys per packet, in keyReadOrder(), like the vendor app.
    std::vector<std::pair<std::uint16_t, Action>> readLayer(int layer);
    Action readKey(int layer, std::uint16_t key);  // single-key 83 LL
    // 03 LL, then reads the key back (as the vendor app does) and returns what the board has.
    // Throws if the read-back differs from what was sent.
    Action setKey(int layer, std::uint16_t key, const Action &action);

    // Travel of these keys (all 83 by default, in keyReadOrder() like the vendor app): 93 00 in
    // packets of 11 keys, 96 00 and 99 00 in packets of 7.
    std::vector<KeyTravel> readTravel(std::span<const std::uint16_t> keys = keyReadOrder());
    // Writes one part for these keys, then reads them back and returns what the board has. 13 00
    // goes in packets of 11 and 19 00 in packets of 7, as captured; 16 00 one key per frame, the
    // only form ever captured. Throws before sending anything if a value is out of range, and
    // after writing if the read-back differs.
    std::vector<KeyTravel> setTravel(TravelPart part, std::span<const KeyTravel> keys);

    // Custom-effect colours of these keys (all 83 by default, in keyReadOrder() like the vendor
    // app): 86 00 in packets of 11 keys.
    std::vector<KeyColor> readColors(std::span<const std::uint16_t> keys = keyReadOrder());
    // 06 00 as captured: each frame a single packet of up to 8 keys, followed by a read-back of
    // those keys. Returns what the board has; throws if a read-back differs. The colours show
    // under the Custom effect (kEffectCustom), the only effect they were captured with.
    std::vector<KeyColor> setColors(std::span<const KeyColor> keys);

    // Advanced keys of the active profile. advancedKeyList: 92 00 with no key, the IDs of the
    // keys that have one (both keys of a SOCD pair). readAdvanced: 92 00 with one key; type
    // None when it has none. readAdvancedKeys: both, like the vendor app, one record per
    // advanced key (a SOCD pair once), in list order.
    std::vector<std::uint16_t> advancedKeyList();
    AdvancedKey readAdvanced(std::uint16_t key);
    std::vector<AdvancedKey> readAdvancedKeys();
    // Clears what the record's keys hold now (12 00 type 00, as the vendor app does before every
    // save; a SOCD pair is cleared on both keys), writes it, reads it back and returns the
    // board's advanced keys. Throws before sending anything if advancedProblem() objects, and
    // after writing if the read-back differs.
    std::vector<AdvancedKey> setAdvanced(const AdvancedKey &a);
    // The same for several records at once (one list read before, one after): all the keys they
    // occupy are cleared first, then each record is written. Throws before sending anything if
    // a record is refused or two records share a key.
    std::vector<AdvancedKey> setAdvanced(std::span<const AdvancedKey> records);
    // Removes the key's advanced key (both keys of a SOCD pair). Returns the board's advanced keys.
    std::vector<AdvancedKey> clearAdvanced(std::uint16_t key);
    std::vector<AdvancedKey> clearAdvanced(std::span<const std::uint16_t> keys);

private:
    // Sends 12 00 type 00 for each key that has an advanced key, and for the other key of a
    // SOCD pair it is part of.
    void clearKeys(std::span<const std::uint16_t> keys, const std::vector<AdvancedKey> &current);

    // Sends a per-key request (key IDs, or full entries) in packets of perPacket keys and
    // hands each answer entry for a key of that packet to onEntry(frame, offset).
    void perKey(std::uint8_t cmd, std::size_t perPacket, std::size_t entryLen, std::span<const std::uint16_t> keys,
                const std::function<void(std::vector<std::uint8_t> &, std::uint16_t)> &encode,
                const std::function<void(const Frame &, std::size_t)> &onEntry);

    Transport &t_;
};

}  // namespace akko
