# Akko TAC75 HE (38ee:0016) — protocol recap

Sources (local capture logs, not published): `akko-capture.txt` (2026-10-03: profile switch, lighting) and `akko-capture2.txt`
(2026-11-02: connect, all effects, custom per-key RGB, Mural, Basic Key, Advanced Key), `akko-capture3.txt`
(2026-11-18: actuation, rapid trigger, dead zones), `akko-capture4.txt` (2026-11-18: Parameter Settings
sub-tabs), `akko-capture5.txt` (2026-11-18: key calibration), `akko-capture6b.txt` (2026-11-18: Base
Functions; `akko-capture6.txt` is its first half).
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
  - `03 xx yy ii` macro `ii` (0-based index in the macro blob). Bind with Loop Type "Specified Times",
    count 1 → `03 01 01 ii`; which of xx/yy is loop type vs count is GUESS (other loop types:
    "Repeat While Held, Stop on Release", "Repeat until any key is pressed")
  - `ff ff ff ff` unbound (Basic Key right-click: the key does nothing, it is **not** a reset to default)

## Commands

### Profile & device
| cmd/sub | Status | Meaning |
|---|---|---|
| `10 00` / `90 00` | KNOWN | set / read active profile (0-based, payload[0]) |
| `82 0F` | DECODED | `00 01 01 03 00 00 00 04 00 1f` — payload[3] = profile count (3 in `akko-capture2.txt`, mark "unplugging and replugging"); akkoctl reads it (falls back to 3 outside 1–8). Other bytes unknown |
| `82 01` | GUESS | `11 00 00 00 00 59` — firmware/hardware version? |
| `82 02` | DECODED | `08 01` — firmware version: the Device Info page shows v1.08 (0x0108) |
| `82 00` `82 03` `82 04` `82 06` `82 08` `82 09` `82 0c` `82 0d` `82 0e` | GUESS | unknown blocks. Not the Base Functions settings (those are `84 xx`, below). `82 0d` reads `01` |
| `85 00` `87 00` `a6 ff` | GUESS | `ff ff`, `ff`, zeros |
| `a1 00` | DECODED | key → LED index: `key u16, 01, led`. LED = column×8 + row (Esc 0x00, F1 0x08, `` ` `` 0x01) |

### Base Functions (`akko-capture6b.txt`)
One byte each, `04 xx` write `[value]` / `84 xx` read. The page reads all of the `84` subs below on load.

| cmd/sub | Status | Meaning |
|---|---|---|
| `04 15` / `84 15` | SEEN | Win Key Lock: `01` on, `00` off. On = the Win key does nothing (checked on the board). Marks "tick/untick Win Key Lock" |
| `04 19` / `84 19` | SEEN | Adaptive Calibration: `01` on (stock), `00` off. Marks "untick/tick Adaptive Calibration" |
| `04 18` / `84 18` | SEEN | Combo Optimization: `01` on (stock), `00` off. Marks "untick/tick Combo Optimization" |
| `04 17` / `84 17` | SEEN | Polling rate: `00` 1000 Hz (stock), `01` 500 Hz, `04` 8000 Hz. Keyboard restarts, the page re-reads everything. GUESS for the rest: `02` 250, `03` 125, `05` 4000, `06` 2000. Marks "click 500Hz", "click 8000Hz"; the switch back to 1000 Hz sits under the 8000Hz mark (mark was late) |
| `84 11` | GUESS | `01` |
| `84 13` | GUESS | 2 bytes, `00 b4` (180) |
| `84 1d` `84 1e` | GUESS | `00` |
| `84 20` | GUESS | `01` |

### Lighting
| cmd/sub | Status | Meaning |
|---|---|---|
| `04 01` / `84 01` | KNOWN | `[effect, colour mode, R, G, B, brightness 0–20, speed 0–4]`. Colour mode 07 = random/rainbow, 00 = fixed colour |
| `04 02` | KNOWN | effect only `[effect]`. Switching effect resets speed to 2 |
| `06 00` / `86 00` | SEEN | custom per-key colour: list of `key u16, R, G, B` (5 B each, up to 11 per packet). Read = 8 packets |
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
| `15 00` / `95 00` | SEEN | `key u16, model u8` | Switch model (Switch Settings). Read sends `key` only. Luminous `45` (stock), Xingyao `43`. UI order suggests 41 Ultra Mag Pro+, 42 Star Magnetic, 44 Star Gravity (GUESS). `akko-capture4.txt` marks "F12 -> Xingyao", "back to the old model" |
| `9a 00/01/02` | GUESS | 56 × `ff` | unknown (not touched by macro edits in `akko-capture7.txt`) |

### Macros (`akko-capture7.txt`)
All macros live in one blob (UI: "Macro Storage: N B / 4096B"), rewritten whole on every save or delete.

| cmd/sub | Status | Meaning |
|---|---|---|
| `05 00` | SEEN | write the whole blob: `pkt_count` × up to 56 B, `pkt_index` 0.., arg `00`, echoed. Empty blob (last macro deleted) = one packet, len `00` |
| `85 00` | SEEN | read: arg = byte offset (`00`, `38`, `70`…), len = bytes wanted. The app reads 2 B (directory size), then the directory, then the blob. Empty storage reads `ff ff` |

Blob layout (DECODED, sizes match the UI: 127 B, 150 B, 165 B):
- Directory: one entry per macro, `offset u16, size u16`, **little-endian** (unlike the rest of the
  protocol; all values < 256 so far). Directory size = 4 × macros, so the first u16 is also the
  first macro's offset.
- Macro at `offset`: `name_len u8, name[name_len]` (ASCII; the UI requires ≥ 2 chars), then actions.
- Action (4 B): `dir u8, delay u16 BE, hid u8`. dir `00` press, `80` release; delay = ms to wait after
  this action; the last action gets `1`. "Use Standard Delay (50ms)" sets every delay to 50.
  e.g. "ab" = `02 61 62 | 00 00 32 04 | 80 00 32 04 | 00 00 32 05 | 80 00 01 05`;
  "cc" recorded as c↓, 137 ms, c↑ = `02 63 63 | 00 00 89 06 | 80 00 01 06`.
- Deleting a macro removes it and repacks the blob; indices of later macros shift down (GUESS: a
  bound key then points to a different macro; the app refuses to bind a key that already has one).
- Bind = plain `03 00` keymap write with action `03 …` (above), then `83 00` read of that key.
  Marks "drag the macro ab icon onto the Insert key", "10b", "12", "12b".

### Parameter Preset, Visual Feedback, Key Calibration
Source: `akko-capture4.txt` (2026-11-18).

- **Parameter Preset** is client-side: hovering sends nothing; clicking (after an "overwrite, cannot be
  undone" dialog) sends plain `13 00` + `19 00` for all 83 keys, then reads `93`/`99` back. Preset 1 =
  actuation 2.0 mm (`0190`) on most keys, 1.4 mm on `1e 2b 2c 2d`, 1.8 on `37`, 1.6 on `46`; RT off,
  press = release 0.5 mm (`0064`). Dead zones untouched. Mark "click Preset 1".

| cmd/sub | Status | Meaning |
|---|---|---|
| `98 00` | SEEN | Visual Feedback on: sent at start, then every 1 s as keep-alive (echoed) |
| `98 01` | DECODED | unsolicited IN every 50 ms while on: `key u16, depth u16, ad u16` for the last key moved. depth in 0.005 mm (0 → ~700 = 3.5 mm at bottom). ad bit 15 = pressed (UI "Action"), bits 0–14 = raw sensor, **falls** as the key goes down (F12 ~11300 rest → ~2270 bottom) |
| `98 02` | SEEN | Visual Feedback off |
| `94 05` | DECODED | read calibration: request = list of `key u16` (10 packets), reply `key u16, rest u16, bottom u16` (raw sensor, e.g. `01`: 10634 / 1743) = the **stored** calibration. Sent on Start Calibration |
| `94 00` | SEEN | calibration keep-alive, every 1 s while calibrating. Keys don't type in this mode |
| `94 02` | DECODED | unsolicited IN while calibrating: 1–2 × `key u16, cur u16, min u16`; cur bit 15 = flag (GUESS: reached bottom / "green"), bits 0–14 = raw sensor; min = lowest value seen |
| `94 04` | SEEN | Finish Calibration (the only other button; there is no Save/Cancel). Commits the session, see below |

Calibration semantics, from `akko-capture5.txt` (each Start Calibration reads all 83 keys, so each
session shows the result of the previous one):

- Finish (`94 04`) stores, **for each key moved during the session**, the lowest raw value it reached
  as its new bottom. Keys not touched keep their bottom (rest drifts by ±3, i.e. re-measured or noise).
  Mark "calibrate every key…": all 83 keys changed. Opening then finishing with no key moved: no change.
- A key pressed only halfway gets a **wrong bottom**: F12 went 2225 → 2307 → 2326 → 4272 over three
  short sessions where it was pressed lightly (marks "press only F12 lightly…"). Nothing guards against it.
- Between two sessions with no calibration in between (marks 1 → 2), 24 keys' bottoms moved by up to
  +22 while the board was used normally. GUESS: Adaptive Calibration (Base Functions) adjusts them live.

## Still to capture
- Keymap: one key on the Fn layer, a media key, right/middle mouse button.
- DKS: one binding held across two stages (the drag), to get the non-`0a` stage value.
- SOCD: any mode not tried (first input?).
- Macros: bind with another loop type and a count of 3 (which byte is which); a macro > 256 B (to
  confirm the directory's byte order); delete a macro that is still bound (what the key does).
- Polling rate 125/250/2000/4000 Hz, to confirm the guessed values (one restart each).
- Mural: what it does on screen, and one simple frame (whole board one colour) to crack the encoding.
