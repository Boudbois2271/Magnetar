# Akko TAC75 HE (38ee:0016) — protocol recap

Sources (local capture logs, not published): `akko-capture.txt` (2026-10-03: profile switch, lighting) and `akko-capture2.txt`
(2026-11-02: connect, all effects, custom per-key RGB, Mural, Basic Key, Advanced Key).
Status: **KNOWN** = akkoctl uses it, verified on the board · **SEEN** = captured write with a clear
before/after, never sent by us · **DECODED** = read-only structure, clear · **GUESS** = needs a capture.

## Transport (KNOWN)
- Interface 2, usage page 0xFF60, report ID 9. Write `[0x09] + 63 bytes`, read 64 back.
- Frame: `cmd, sub, arg, pkt_count, pkt_index, len, payload[len]…, checksum@62`.
  `arg` is usually 0; for advanced keys (0x12/0x92) it is the advanced-key type.
- Checksum: `(0x09 + sum(frame[0:62]) + checksum) & 0xFF == 0xFF`.
- cmd bit 7 set = read; writes are echoed back. Per-key reads: the request payload lists the key IDs
  (u16 BE) wanted for that packet.
- One session at a time or the firmware stops sending keystrokes (akkoctl flock).

## Units
- Key IDs: u16 BE, 82 keys, 0x01..0x67 with gaps (layout below).
- Travel: **0.005 mm per unit** (SEEN: 127 = 0.635 mm, 233 = 1.165, 639 = 3.195; DKS 227 = 1.14 shown).
- Time: ms, u16 BE (322, 965 SEEN).
- Action (4 bytes), same in keymap and advanced keys:
  - `00 00 00 uu` plain key, `uu` = HID usage
  - `00 mm 00 00` modifier key alone (mm: 01 LCtrl 02 LShift 04 LAlt 08 LGui 10 RCtrl 20 RShift 40 RAlt)
  - `00 mm uu 00` shortcut, e.g. `00 01 19 00` = Ctrl+V (SEEN; note usage moves to byte 2)
  - `01 bb 01 00` mouse button, bb = 01 left (SEEN; others GUESS 02 right 04 middle)
  - `02 00 hi lo` consumer usage (volume, media, mail, calc…)
  - `07 00 00 nn` system function (Fn+Win = 01 Win lock?, Fn+W = 00 WASD swap?) GUESS
  - `08 aa bb 00` lighting control (`08 09 xx` = effect xx, `08 03 ±` brightness, `08 01 ±` speed) GUESS
  - `09 00 00 nn` switch to profile nn (Fn+F9/F10/F11) GUESS, very likely
  - `0d 00 00 00` Fn key

## Commands

### Profile & device
| cmd/sub | Status | Meaning |
|---|---|---|
| `10 00` / `90 00` | KNOWN | set / read active profile (0-based, payload[0]) |
| `82 0F` | DECODED | `00 01 01 03 00 00 00 04 00 1f` — payload[3] = 3 = profile count probably. akkoctl reads payload[18] (=0) and falls back to 3: harmless but wrong offset |
| `82 01` | GUESS | `11 00 00 00 00 59` — firmware/hardware version? |
| `82 02` | GUESS | `08 01` — version 1.08? |
| `82 00` `82 03` `82 04` `82 06` `82 08` `82 09` `82 0c` `82 0e` | GUESS | settings blocks (polling rate, sleep, debounce, Win lock…). Settings tab capture will tell |
| `85 00` `87 00` `a6 ff` | GUESS | `ff ff`, `ff`, zeros |
| `a1 00` | DECODED | key → LED index: `key u16, 01, led`. LED = column×8 + row (Esc 0x00, F1 0x08, `` ` `` 0x01) |

### Lighting
| cmd/sub | Status | Meaning |
|---|---|---|
| `04 01` / `84 01` | KNOWN | `[effect, colour mode, R, G, B, brightness 0–20, speed 0–4]`. Colour mode 07 = random/rainbow, 00 = fixed colour |
| `04 02` | KNOWN | effect only `[effect]`. Switching effect resets speed to 2 |
| `06 00` / `86 00` | SEEN | custom per-key colour: list of `key u16, R, G, B` (5 B each, up to 11 per packet). Read = 8 packets |
| `84 11` | GUESS | `01` |
| `08 04` | SEEN, not decoded | **Mural**: first a 2-packet header `00 00 53` + 83 LED indices, then 5-packet frames (254 B) streamed repeatedly. Frame encoding unknown (not raw RGB) |

Effects (SEEN): 00 Off · 01 Steady · 02 Breathing · 03 Dream rainbow · 04 Instant trigger (reactive, row) ·
05 Walking in the rain · 06 Rainbow wheel · 07 Ripple (reactive, all) · 08 Starry night ·
09 Snow trail (reactive, fades key) · 0A Endless flow · 0B Flowing water · 0C Shadow follow (inverse of
snow trail) · 0D Sine wave · 0E Left-right scan · 0F Spinning windmill · 10 Colorful waterfall ·
11 Blossom prosperity · 12 Spinning storm · 13 Custom (per-key colours from `06 00`).

### Keymap
| cmd/sub | Status | Meaning |
|---|---|---|
| `03 LL` | SEEN | set one key on layer LL: `key u16 + action(4)`. Only layer 00 captured |
| `83 LL` | DECODED | read layer LL (00 base, 01 Fn, 02 = copy of base: Mac or Fn2?). 10 packets × 9 keys, `key u16 + action(4)` |

### Advanced keys (arg byte = type)
| type | Status | Payload |
|---|---|---|
| 00 | SEEN | clear: `key` (sent before every save, and per key on restore) |
| 01 Toggle | SEEN | `key, action(4), hold ms u16` |
| 02 Mod-Tap | SEEN | `key, hold action(4), tap action(4), hold ms u16` |
| 03 DKS | SEEN | `key, depth×4 u16 (press start, bottom, release from bottom, full release), then 4 × [action(4), stages(4)]`. stages = one byte per stage, `0a` = fire once there (screenshot confirms W/A/C/U). Continuous "hold across stages" value not captured |
| 04 SOCD | SEEN | `02, keyA u16, keyB u16, mode` — mode 00 Neutral, 01 Last input, 03 Absolute key 1, 04 Absolute key 2 (02 maybe First input?) |
| 05 Multipoint | SEEN | `key, count, count × [action(4), depth u16]` |
| 06 Release (END) | SEEN | `key, action(4)` fired on release |

Read: `92 00` with `key` → echoes the type in arg byte and the same payload (all-zero = none).
The first `92 00` read (no key, 4 packets, 216 B) is the physical layout: column-major, 6 rows × 18 cols, 0 = no key.

### Per-key travel tables (read-only so far)
| cmd | Status | Per key | Probably |
|---|---|---|---|
| `93 00` | GUESS | `key, u16 200, u8 0` | actuation 200 × 0.005 = **1.00 mm** (or 2.00 mm if 0.01 mm units) |
| `96 00` | GUESS | `key, u16 26, u16 25, u16 0` | Rapid Trigger press/release 0.13/0.125 mm? |
| `95 00` | GUESS | `key, u8 69` | dead zone / switch type? |
| `99 00` | GUESS | `key, 6 × 00` | ? |
| `9a 00/01/02` | GUESS | 56 × `ff` | macros (3 slots?) |

## Still to capture
- **Performance tab**: global actuation, single-key actuation, RT on/off, RT press and release
  separately, continuous RT, top/bottom dead zones → confirms 93/95/96/99 and their write commands.
- Keymap: one key on the Fn layer, a media key, right/middle mouse button.
- DKS: one binding held across two stages (the drag), to get the non-`0a` stage value.
- SOCD: any mode not tried (first input?).
- Macros: record "ab", assign, delete.
- Settings: each polling rate, sleep, Win lock, WASD swap, debounce, anything else.
- Device / about page (firmware version).
- Key test / travel view (live data).
- Mural: what it does on screen, and one simple frame (whole board one colour) to crack the encoding.
