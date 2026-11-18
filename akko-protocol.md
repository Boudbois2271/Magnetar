# Akko TAC75 HE (38ee:0016) — protocol recap

Sources (local capture logs, not published): `akko-capture.txt` (2026-10-03: profile switch, lighting) and `akko-capture2.txt`
(2026-11-02: connect, all effects, custom per-key RGB, Mural, Basic Key, Advanced Key), `akko-capture3.txt`
(2026-11-18: actuation, rapid trigger, dead zones).
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
- Key IDs: u16 BE, 83 keys (Select All in `akko-capture3.txt`), 0x01..0x67 with gaps (layout below).
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
| `82 0F` | DECODED | `00 01 01 03 00 00 00 04 00 1f` — payload[3] = profile count (3 in `akko-capture2.txt`, mark "unplugging and replugging"); akkoctl reads it (falls back to 3 outside 1–8). Other bytes unknown |
| `82 01` | GUESS | `11 00 00 00 00 59` — firmware/hardware version? |
| `82 02` | DECODED | `08 01` — firmware version: the Device Info page shows v1.08 (0x0108) |
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
| `83 LL` | DECODED | read layer LL (00 Default, 01 Fn, 02 probably Fn1: the UI lists exactly these three layers; 02 reads as a copy of base). 10 packets × 9 keys, `key u16 + action(4)` |

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

### Per-key travel (Parameter Settings → Actuation Settings)
Source: `akko-capture3.txt` (2026-11-18). Test key F12 = `0d`. Writes are echoed, then the web app
reads the same keys back. Multi-packet: `pkt_count`/`pkt_index` as usual, `arg` 0.

| cmd/sub | Status | Per key | Notes |
|---|---|---|---|
| `13 00` / `93 00` | SEEN | `key u16, actuation u16, 00` (5 B, 11 per packet) | 1.37 mm → `01 12` (274), 0.42 → `00 54` (84), 0.6 → `00 78` (120). Select All = 83 keys, 8 packets. Marks "Select All, actuation -> 1.37 mm", "Deselect, select F12 only…", "restore…" |
| `19 00` / `99 00` | SEEN | `key u16, rt on u8, press u16, release u16, 00` | Rapid Trigger. On with defaults = press 1 / release 1 (0.005 mm). Linked slider writes press = release (0.13 → `1a`/`1a`). Independent: press 0.17 → `22`, release "0.29" → `39` (57 = 0.285; the UI rounds). Off keeps the values. The "independent" toggle sends nothing (UI only) |
| `16 00` / `96 00` | SEEN | `key u16, top u16, bottom u16, on u16` | Dead zones. Toggle on → `00 1a 00 19 00 01` (top 0.13, bottom 0.125). Top 0.185 → `25`, bottom 0.23 → `2e`. Off → last u16 `00`, values kept. Read = 12 packets × 7 keys |
| `95 00` | GUESS | `key, u8 69` | not touched by this tab (Switch Settings / calibration?) |
| `9a 00/01/02` | GUESS | 56 × `ff` | macros (3 slots?) |

## Still to capture
- Parameter Settings, other sub-tabs (UI as of 2026-11-18, plan below):
  - Switch Settings: assign a switch model per key (Ultra Mag Pro+, Star Magnetic, Xingyao, Star
    Gravity, Luminous); eye icon toggles a view. Likely `95 00` (`key, u8`; 69 = current model?).
  - Key Calibration: Start Calibration → hold each key at the bottom until green → Save.
  - Parameter Preset: 5 presets (trigger / RT sensitivity). Hover previews, click applies.
  - Visual Feedback: Start Monitoring → live Position (hex), Action, AD, Depth (0–4 mm).
  - Continuous RT is on none of them: look again on Actuation Settings.

- Keymap: one key on the Fn layer, a media key, right/middle mouse button.
- DKS: one binding held across two stages (the drag), to get the non-`0a` stage value.
- SOCD: any mode not tried (first input?).
- Macros: record "ab", assign, delete. The UI shows 4096 B of macro storage.
- Base Functions page (UI as of 2026-11-18): Win Key Lock, Adaptive Calibration, Combo Optimization,
  polling rate 125/250/500/1000/2000/4000/8000 Hz (the device restarts on change). One toggle per mark.
- Mural: what it does on screen, and one simple frame (whole board one colour) to crack the encoding.

### Capture plan: `akko-capture4` (Parameter Settings sub-tabs)
Test key F12, safest first. Lock akkoctl and the widget out first (Ctrl+C after closing the tab):

```sh
flock "$XDG_RUNTIME_DIR/akkoctl.lock" sleep infinity
```

Then paste `akko-capture.js` in the configurator's console and run each mark before its action
(fill the `___`):

```js
akko.mark('Actuation Settings, Select All, no change: shows actuation ___, RT ___, dead zone ___; continuous RT option: ___')
akko.mark('Visual Feedback: Start Monitoring')
akko.mark('F12: press slowly to half travel')
akko.mark('F12: press to bottom')
akko.mark('F12: release')
akko.mark('Visual Feedback: Stop Monitoring')
akko.mark('Switch Settings: click the eye icon (it shows: ___)')
akko.mark('Switch Settings: F12 -> Xingyao (old model: ___)')
akko.mark('Switch Settings: F12 -> back to the old model')
akko.mark('Parameter Preset: hover Preset 1, no click')
akko.mark('Parameter Preset: click Preset 1 (Actuation tab then shows for F12: ___)')
akko.mark('restore: Select All, actuation -> 0.6 mm')
akko.mark('restore: Select All, RT toggle off')
akko.mark('restore: Select All, dead zone toggle off')
akko.mark('Key Calibration: Start Calibration')
akko.mark('Key Calibration: hold F12 at the bottom until green, then release')
akko.mark('Key Calibration: leave the tab without Save (Cancel button exists: ___)')
akko.save('akko-capture4')
```

If F12 already is Xingyao, pick any other model. Skip a restore mark if mark 1 showed that
setting differently. Calibration is never saved; if keys misbehave afterwards, run a full
calibration and Save.
