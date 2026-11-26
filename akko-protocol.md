# Akko TAC75 HE (38ee:0016) — protocol recap

Sources (local capture logs, not published): `akko-capture.txt` (2026-10-03: profile switch, lighting) and `akko-capture2.txt`
(2026-11-02: connect, all effects, custom per-key RGB, Mural, Basic Key, Advanced Key), `akko-capture3.txt`
(2026-11-18: actuation, rapid trigger, dead zones), `akko-capture4.txt` (2026-11-18: Parameter Settings
sub-tabs), `akko-capture5.txt` (2026-11-18: key calibration), `akko-capture6b.txt` (2026-11-18: Base
Functions; `akko-capture6.txt` is its first half), later captures up to `akko-capture10.txt` (2026-11-18:
polling rates, DKS hold, Mural solid colours) are cited by mark, as is `akko-capture13.txt` (2026-11-25:
long macro directories, mouse macro steps, the vendor's lighting/mouse/media actions, Fn layer reset, vendor
ranges, profile rename).
Status: **KNOWN** = our code (`core/`) sends it, verified on the board · **SEEN** = captured write with a clear
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
- Key IDs: u16 BE, 83 keys (Select All in `akko-capture3.txt`), 0x01..0x67 with gaps (layout in the Keymap section).
- Travel: **0.005 mm per unit** (SEEN: 127 = 0.635 mm, 233 = 1.165, 639 = 3.195; DKS 227 = 1.14 shown).
- Time: ms, u16 BE (322, 965 SEEN).
- Action (4 bytes), same in keymap and advanced keys:
  - `00 00 00 uu` plain key, `uu` = HID usage
  - `00 mm 00 00` modifier key alone (mm: 01 LCtrl 02 LShift 04 LAlt 08 LGui 10 RCtrl 20 RShift 40 RAlt 80 RGui; "Right Windows" = `00 80 00 00`, `akko-capture11.txt` mark 12)
  - `00 mm uu 00` shortcut, e.g. `00 01 19 00` = Ctrl+V (SEEN; note usage moves to byte 2). Two keys: Ctrl+Shift, First Key A, second key B = `00 03 05 04` (SEEN, `akko-capture11.txt` mark 16: the second key comes first)
  - `01 bb 01 00` mouse button, bb = 01 left, 02 right, 03 middle, 04 back, 05 forward (SEEN: 01–03
    `akko-capture8.txt` marks 2, 4; all five `akko-capture13.txt` mark 12, table below)
  - `02 00 hi lo` consumer usage (HID page 0x0C, u16 BE); Volume Up = `02 00 00 e9` (SEEN, capture8 mark 6;
    the vendor's whole list in the table below)
  - `07 00 00 nn` system function (Fn+Win = 01 Win lock?, Fn+W = 00 WASD swap?) GUESS
  - `08 gg oo 00` lighting control: group gg 00 effect, 01 direction, 02 colour, 03 brightness, 04 speed;
    op oo 00 cycle, 01 increase / left, 02 decrease / right (SEEN, all 15, `akko-capture13.txt` mark 10, table
    below). Brightness Cycle `08 03 00 00` steps 0→5→10→15→20 on the board (`akko-capture11.txt` mark 14).
    `08 09 nn 00` (factory Fn+Home/End/PgUp/PgDn, nn = 01/10/06/0b, which are effect IDs) = cycle the effects
    of a group starting at nn: Fn+Home 01–05, Fn+PgUp 06–0a, Fn+PgDn 0b–0f, Fn+End 10–13, wrapping back to the
    group's first (SEEN on the board through the `fe 0b 01` reports, 2026-11-26). Not in the vendor's list
  - `09 00 00 nn` switch to profile nn (Fn+F9/F10/F11) GUESS, very likely
  - `0d nn 00 00` layer key: nn 00 = Fn, 01 = Fn1 ("Function Key 1", SEEN capture8 mark 8). Moving a key
    off Fn1 shows a client-side warning (Fn1 layer unreachable without an Fn1 key); nothing is sent until Continue
  - `11 00 00 00` "www" in the vendor's Lighting & Special Keys list (SEEN, `akko-capture13.txt` mark 10;
    what it does on the board was not checked)
  - `03 tt nn ii` macro `ii` (0-based index in the macro blob), loop type `tt`, count `nn` (SEEN:
    "Specified Times" = `01`; count 1 → `03 01 01 01`, count 4 → `03 01 04 00`, `akko-capture7b.txt`).
    "Repeat until any key is pressed" = `02`, "Repeat While Held, Stop on Release" = `04`, both with
    count `00` (SEEN, `akko-capture9.txt` marks 5, 9; both repeated on the board, marks 6, 10)
  - `ff ff ff ff` unbound (Basic Key right-click: the key does nothing, it is **not** a reset to default)

The vendor's Basic Key lists, dragged one by one onto Insert on the Fn layer (`03 01`), each echoed and read
back identical (`akko-capture13.txt` marks 10–13, frames in `tests/fixtures/vendor-actions.txt`). All SEEN;
none sent by us yet.

| Lighting & Special Keys | Action | | Media & Tools | Action |
|---|---|---|---|---|
| Effect Cycle | `08 00 00 00` | | Mouse Left Button | `01 01 01 00` |
| Effect Increase | `08 00 01 00` | | Mouse Right Button | `01 02 01 00` |
| Effect Decrease | `08 00 02 00` | | Mouse Middle Button | `01 03 01 00` |
| Direction Cycle | `08 01 00 00` | | Mouse Forward Button | `01 05 01 00` |
| Direction Left | `08 01 01 00` | | Mouse Back Button | `01 04 01 00` |
| Direction Right | `08 01 02 00` | | Play/Pause Key | `02 00 00 cd` |
| Color Cycle | `08 02 00 00` | | System Multimedia Key | `02 00 01 83` (AL Consumer Control Config) |
| Color Increase | `08 02 01 00` | | Mute Key | `02 00 00 e2` |
| Color Decrease | `08 02 02 00` | | Pause Key | `02 00 00 b7` (HID "Stop") |
| Brightness Cycle | `08 03 00 00` | | Previous Song Key | `02 00 00 b6` |
| Brightness Increase | `08 03 01 00` | | Next Song Key | `02 00 00 b5` |
| Brightness Decrease | `08 03 02 00` | | Volume Up Key | `02 00 00 e9` |
| Speed Cycle | `08 04 00 00` | | Volume Down Key | `02 00 00 ea` |
| Speed Increase | `08 04 01 00` | | Browser | `02 00 02 23` (AC Home) |
| Speed Decrease | `08 04 02 00` | | Calculator | `02 00 01 92` |
| Fn1 | `0d 01 00 00` | | Email | `02 00 01 8a` |
| www | `11 00 00 00` | | My Computer | `02 00 01 94` |
| | | | Favorites | `02 00 02 2a` (AC Bookmarks) |

The factory Fn layer (read after the layer reset, mark 14) uses these: Fn+Up/Down = brightness +/−,
Fn+`=`/`-` = speed +/−, Fn+`\` = colour cycle, Fn+Left = `08 01 02 00` and Fn+Right = `08 01 01 00` (the
vendor's labels "Direction Right"/"Direction Left", swapped against the arrows; not checked on the board).

## Commands

### Profile & device
| cmd/sub | Status | Meaning |
|---|---|---|
| `10 00` / `90 00` | KNOWN | set / read active profile (0-based, payload[0]) |
| `82 0F` | DECODED | `00 01 01 03 00 00 00 04 00 1f` — payload[3] = profile count (3 in `akko-capture2.txt`, mark "unplugging and replugging"); akkoctl reads it (falls back to 3 outside 1–8). Other bytes unknown |
| `1a PP` / `9a PP` | SEEN | profile PP name (PP 0-based, in sub): write `1a PP 00 01 00 <1+n> | n, ASCII name[n]`, echoed; read request `9a PP 00 01 00 00` (no payload), answer len `38`: `n, name`, rest `ff` (stock names are all `ff`, the UI then shows "Onboard Profile N"). A shorter name only overwrites the start, `n` bounds it. Profile Management pencil, `akko-capture11.txt` (marks 1–3 lost, frames before mark 4); profile 2 renamed "Gaming" = `1a 01 … 07 | 06 47 61 6d 69 6e 67` and back to "Onboard Profile 2" (n = `11`), `akko-capture13.txt` marks 28–30, frames in `tests/fixtures/vendor-profile-names.txt`. The board showed nothing (mark 29). Renaming back stores the literal text, not `ff`. Seen: letters, digits, space; the longest 17 chars (max ≤ 55 by the frame, UI limit unknown). Magnetar's core sends these exact frames (tested against the fixture, not yet on the board) and allows 1–55 printable ASCII chars |
| `82 01` | GUESS | `11 00 00 00 00 59` — firmware/hardware version? |
| `82 02` | DECODED | `08 01` — firmware version: the Device Info page shows v1.08 (0x0108) |
| `82 00` `82 03` `82 04` `82 06` `82 08` `82 09` `82 0c` `82 0d` `82 0e` | GUESS | unknown blocks. Not the Base Functions settings (those are `84 xx`, below). `82 0d` reads `01` |
| `85 00` `87 00` `a6 ff` | GUESS | `ff ff`, `ff`, zeros |
| `a1 00` | DECODED | key → LED index: `key u16, 01, led`. LED = column×8 + row (Esc 0x00, F1 0x08, `` ` `` 0x01) |

**Per profile or global** (our own reads on the board, 2026-11-26): everything read on profile 0, `10 00`
to profile 1, read again, `10 00` back to 0, read again (profile 0 unchanged).

| What | Per profile? | Evidence |
|---|---|---|
| Lighting `84 01` | yes | effect `01` vs `13`, brightness `14` vs `0a` |
| Per-key colours `86 00` | yes | 74 of 83 keys differ (profile 1 all black) |
| Travel `93`/`99`/`96 00` | yes | actuation 200 vs 44 on all 83 keys, dead zones differ |
| Macros `85 00` | yes | 2 macros vs none |
| Advanced keys `92 00` | yes | Toggle written on Insert on profile 0 (`12 00`): profile 1's list stayed empty; cleared after |
| Win Key Lock `84 15` | yes | set `01` on profile 0 (`04 15`): profile 1 read `00`; set back to `00` after |
| Polling rate `84 17` | unknown, likely global | `06` (2000 Hz, not stock) on both profiles; a write restarts the board, not tried |
| Combo Optimization, Adaptive Calibration `84 18`/`84 19` | unknown | stock `01` on both profiles, never written |
| Keymap `83 LL` | not shown | Default layer identical on both (factory) |

### Base Functions (`akko-capture6b.txt`)
One byte each, `04 xx` write `[value]` / `84 xx` read. The page reads all of the `84` subs below on load.
KNOWN: sent by Magnetar's core (each switch toggled and restored, polling 1000 → 500 → 1000), read back right.

| cmd/sub | Status | Meaning |
|---|---|---|
| `04 15` / `84 15` | KNOWN | Win Key Lock: `01` on, `00` off. On = the Win key does nothing (checked on the board). Marks "tick/untick Win Key Lock" |
| `04 19` / `84 19` | KNOWN | Adaptive Calibration: `01` on (stock), `00` off. Marks "untick/tick Adaptive Calibration" |
| `04 18` / `84 18` | KNOWN | Combo Optimization: `01` on (stock), `00` off. Marks "untick/tick Combo Optimization" |
| `04 17` / `84 17` | KNOWN | Polling rate: `00` 1000 Hz (stock), `01` 500 Hz, `02` 250, `03` 125, `04` 8000, `05` 4000, `06` 2000 Hz. Echo and `84 17` read-back come at once, then the keyboard re-enumerates on USB: on Linux the old node answers for ~200 ms, is gone by ~350 ms, the new one appears at ~0.9 s and opens (udev access) at ~1 s; the page re-reads everything. 500/8000: capture6b; 125/250/2000/4000 and back to 1000: `akko-capture10.txt` marks 1–5 |
| `84 11` | GUESS | `01` |
| `84 13` | GUESS | 2 bytes, `00 b4` (180) |
| `84 1d` `84 1e` | GUESS | `00` |
| `84 20` | GUESS | `01` |

What the switches do, from public docs and reviews only (searched 2026-11-25; no source names
either toggle as the vendor app labels it):

| Setting | Effect | Confidence | Sources |
|---|---|---|---|
| Win Key Lock | Win key does nothing while on | Checked on the board | — |
| Adaptive Calibration | Most likely the vendor's "Dynamic Calibration": the board re-calibrates key travel on its own (every 6 h at first, every second since April 2024 firmware), so manual calibration is rarely needed. Still needed after a switch swap, a firmware update or above 50 °C; a key pressed very slowly (~5 s) can fool it. Which end (rest, bottom or both) it adjusts is not said. Fits our bottom drift between sessions (Key calibration, below) | Name match is a guess; the feature is documented by two vendor KBs | [MonsGeek KB](https://www.monsgeek.com/monsgeek-magnetic-switches-knowledge-base/), [Akko KB](https://en.akkogear.com/akko-magnetic-switches-knowledge-base), [M1 V5 TMR review](https://damiancooper.medium.com/monsgeek-m1-v5-tmr-tmr-take-two-ff3d5f2b5420) |
| Combo Optimization | Unknown. No manual, FAQ, review or forum post found. The closest feature elsewhere is MCHOSE's "Multi-Press Optimization" (a different vendor) | Unknown | [MCHOSE settings guide](https://www.mchose.store/blogs/all-posts/mchose-he-keyboards-optimize-performance-firmware-settings) |
| Polling rate | Reports per second sent to the PC | Checked on the board | — |

### Lighting
| cmd/sub | Status | Meaning |
|---|---|---|
| `04 01` / `84 01` | KNOWN | `[effect, colour mode, R, G, B, brightness 0–20, speed 0–4]`. Colour mode 07 = random/rainbow, 00 = fixed colour |
| `04 02` | KNOWN | effect only `[effect]`. Switching effect resets speed to 2 |
| `fe 0b` | SEEN | unsolicited IN `fe 0b kk …` after an on-board (Fn shortcut) lighting change, checksum valid. kk `01` effect: `fe 0b 01 ee` (Fn+Home/End/PgUp/PgDn); `02` brightness: `fe 0b 02 bb` (Fn+Up/Down; `00 05 0a 0f 14` while cycling, `akko-capture11.txt` mark 14); `03` speed: `fe 0b 03 ss` (Fn+= `04`, Fn+- `03`); `04` colour: `fe 0b 04 mm rr gg bb` = colour mode and RGB (Fn+\ from mode `01` green: `02 0000ff`, `03 ff6100`, `04 ff00ff`, `05 ffff00`, `06 ffffff`, `07 00ff00` (rainbow), `00 00ff00`). After an effect report `84 01` showed that effect with its own colour mode, colour, brightness and speed (effect `04`: `07 ff0000`, 20, 2, where effect `01` had `00 00ff00`, 15, 3). Speed and colour reports matched the next `84 01`. Logged on the vendor node (hidraw opened read-only, no app), one shortcut at a time, 2026-11-26 (kk 02 first seen 2026-11-25). Magnetar shows them live |
| `fe 09` | SEEN | unsolicited IN `fe 09 pp` after an on-board profile switch, pp = new profile, 0-based: Fn+F10 `01`, Fn+F11 `02`, Fn+F9 `00`. Same log, 2026-11-26. Magnetar reads the board again on it |
| — | SEEN | no report at all: Fn+Left/Right (`08 01 02/01 00`, `84 01` unchanged after them), Fn+Win, Fn+W. Fn+Win and Fn+W gave only all-zero reports `07`, `02`, `04` on interface 1 (hidraw1), key releases most likely; no other Fn shortcut gave anything there. Same log, 2026-11-26 |
| `06 00` / `86 00` | KNOWN | custom per-key colour: list of `key u16, R, G, B` (5 B each). Read: key IDs, 11 per packet, 8 packets for all keys. Write: one single-packet frame per batch (up to 8 keys seen, `akko-capture2.txt` "selecting F row"), the vendor app reads each batch back with `86 00`. Shown under effect `13` (Custom). KNOWN 2026-11-21: core `setColors` set F12 blue under Custom, read back and seen on the board, then restored. Whether a write sticks under another effect is untested |
| `08 04` | KNOWN | **Mural** frame, not echoed, nothing stored. Payload = one byte stream of groups `colour u16 (RGB565, BE), n, n × LED index`, cut into 56-byte (`len 38`) packets, groups running across packets; every LED appears once per frame. Solid red/green/blue = `f8 00` / `07 e0` / `00 1f` + `53` (83) + all indices, 2 packets (`akko-capture10.txt` Mural marks). The vendor app resends a still image every ~1008 ms and streams animations (Rainbow, 5 packets) every ~32 ms (`akko-capture11.txt` mark 8); Colour Enhancement is applied in the browser. There is no stop command: the app stops sending and the board returns to its stored effect (`akko-capture11.txt` marks 8–11). KNOWN 2026-11-22: core `streamMural` showed solid red, a one-key sweep in key order, and an 83-colour frame (6 packets, more than the vendor app sent), then the board went back to its Custom colours 1–3 s after the last frame |

Everything else in `akko-capture13.txt` is already known: `85 00` ×155 and `05 00` ×79 are the macro reads
and writes above, `03 01` ×35 / `83 01` ×45 the Fn-layer writes and their single-key and full-layer reads,
`96 00` ×96 the full dead-zone reads after each `16 00`. One stray write: during mark 5 the vendor app sent
`03 00 … 00 40 02 00 00 e9` (Default layer, `.` key = Volume Up), read back and never undone in the capture.

Profile Management's second icon ("back up to inactive profiles") only reads (`84`, `85`, `92`, `93`, `95`, `96`, `99`); the backup lives in the browser (`akko-capture11.txt` mark 4).

Mural LED index = `column × 8 + row` (6 rows, columns 0–14, 0x00–0x72, 83 LEDs). Not the key IDs: row r is the key row r
of the Keymap table below and column c the c-th key from the left in it (rows have 15/15/15/14/14/10 LEDs, as many as keys).
KNOWN 2026-11-22: a one-key-at-a-time sweep in that order lit every key once, in order (`core/mural.cpp`).

Each effect keeps its own colour mode, colour and speed: `04 02 02` (Breathing) read back `07 ff 00 00 14 00`, then `04 02 01` (Steady) read back `00 00 ff 00 14 02` (`akko-capture11.txt` mark 6). So `04 02` doesn't reset speed, it loads that effect's stored values.

Effects (SEEN): 00 Off · 01 Steady · 02 Breathing · 03 Dream rainbow · 04 Instant trigger (reactive, row) ·
05 Walking in the rain · 06 Rainbow wheel · 07 Ripple (reactive, all) · 08 Starry night ·
09 Snow trail (reactive, fades key) · 0A Endless flow · 0B Flowing water · 0C Shadow follow (inverse of
snow trail) · 0D Sine wave · 0E Left-right scan · 0F Spinning windmill · 10 Colorful waterfall ·
11 Blossom prosperity · 12 Spinning storm · 13 Custom (per-key colours from `06 00`).

### Keymap
| cmd/sub | Status | Meaning |
|---|---|---|
| `03 LL` | KNOWN | set one key on layer LL: `key u16 + action(4)`, echoed. Layers 00, 01 (capture8 marks 2–16) and 02 (capture9 marks 16–17) captured. Sent by Magnetar and checked on the board 2026-11-21: Insert → Home, Ctrl+V, Volume up on Default and A on Fn1, each read back and restored |
| `11 LL` | SEEN | Basic Key "Reset" next to a layer: reset layer LL to factory, payload always `01`, echoed, then the app re-reads the whole layer (`83 LL`, 10 packets). Default Layer = `11 00` (Insert came back, `akko-capture11.txt` marks 17–18); Fn Layer = `11 01 00 01 00 01 01` (`akko-capture13.txt` mark 14: Insert's Fn action back to `00 00 00 49`, Fn+Insert typed Insert again, mark 15). Fn1 (`11 02`) not captured |
| `83 LL` | KNOWN | read layer LL: 00 Default, 01 Fn, 02 Fn1 (confirmed: a write on the UI's Fn1 Layer goes to `03 02`, capture9 mark 16). Layer 02 reads as a copy of base, as the UI shows. 10 packets × 9 keys, `key u16 + action(4)`. Single-key form: `83 LL 00 01 00 02 key u16` → `key + action(4)`; the web app sends it after every `03 LL` write |

Key IDs on the board (rows top to bottom, left to right; factory action in brackets when not obvious):

| Row | Key IDs |
|---|---|
| 0 | `01` Esc, `02`–`0d` F1–F12, `63` Delete, `62` Insert |
| 1 | `0e` `` ` ``, `0f`–`18` 1–0, `19` -, `1a` =, `1b` Backspace, `64` Home |
| 2 | `1c` Tab, `1d`–`26` Q–P, `27` [, `28` ], `29` \, `66` PgUp |
| 3 | `2a` Caps, `2b`–`35` A–', `36` Enter, `67` PgDn |
| 4 | `37` LShift, `38`–`41` Z–/, `42` RShift, `4a` Up, `65` End |
| 5 | `43` LCtrl, `44` LWin, `45` LAlt, `46` Space, `47` RAlt, `48` Fn (`0d 00 00 00`), `49` RCtrl, `4c` Left, `4b` Down, `4d` Right |

Geometry (key widths, the nav column half a key to the right) is in `core/keymap.cpp`, measured on
the board. The factory keymap of layers 00 and 01 is the first `83` read of `akko-capture.txt`
(identical in `akko-capture2.txt`); layer 02 starts as a copy of 00. The web app reads a layer as
10 packets (`pkt_count 0a`) of 9 key IDs in a fixed order, also in `core/keymap.cpp`.

### Advanced keys (arg byte = type)
| type | Status | Payload |
|---|---|---|
| 00 | KNOWN | clear: `key` (sent before every save, and per key on restore) |
| 01 Toggle | KNOWN | `key, action(4), hold ms u16`. A tap holds the action down until the next tap (Magnetar, B on Insert, 2026-11-25) |
| 02 Mod-Tap | KNOWN | `key, hold action(4), tap action(4), hold ms u16` |
| 03 DKS | KNOWN | `key, depth×4 u16 (press start, bottom, release from bottom, full release), then 4 × [action(4), stages(4)]`. stages = one byte per stage (press 1st depth, bottom, release from bottom, full release), bit flags: `02` key down here, `04` stay held until the next stage, `08` key up (at the next stage if `04`, else here). `0a` = fire once; `0a` at press 1st depth and at release from bottom (`0a 00 0a 00`) taps twice, as composed (Magnetar 2026-11-25). `04` alone and `08` alone are not kept: they read back `00` (2026-11-25). B held 1st depth→bottom = `0e 00 00 00`; held across all four = `06 04 0c 00` (`akko-capture10.txt` DKS marks; the board kept B held over the 2-stage bar). In `akko-capture11.txt` marks 29–31: B held bottom → release from bottom = stages `00 0e 00 00`, C fire-once at full release = `00 00 00 0a`; the board did exactly that. Depths there: `00c8 0258 0258 00c8` |
| 04 SOCD | KNOWN | `02, keyA u16, keyB u16, mode` — mode 00 Neutral, 01 Last input, 03 Absolute key 1, 04 Absolute key 2. The UI offers only these four (screenshot 2026-11-18-29); `02` is unused |
| 05 Multipoint | KNOWN | `key, count, count × [action(4), depth u16]`. Each depth takes over from the previous one, held (B 0.50 / C 1.50 / D 3.00 mm on Insert, Magnetar 2026-11-25) |
| 06 Release (END) | KNOWN | `key, action(4)` fired on release (Magnetar, B on Insert, 2026-11-25) |

Frames: `12 00 <type> 01 00 len payload` (the arg byte is the type), echoed. Details from
`akko-capture2.txt` Advanced Key marks, `akko-capture10.txt` DKS marks, `akko-capture11.txt` marks 29–32:
- SOCD's first byte is always `02` (maybe a key count); the record is stored on both keys and either
  key reads it back whole. KNOWN: the two copies are independent. A type `00` clear on one key
  removes only that key's copy: after PgUp + PgDn (last input), clearing PgUp alone left the list
  `[0067]`, `92 00 0066` answered type `00`, and `92 00 0067` still answered the whole pair record
  `02, 0066, 0067, 01` (our own board read, 2026-11-26; whether the leftover still acts as SOCD
  wasn't tried). Clearing PgDn then emptied the list. So clear both keys, as the vendor app does.
- Multi-Point always carries 3 slots; an unused slot is action `00 00 00 00` with a depth
  (A only = `03, 00000004 007f, 00000000 00e9, 00000000 01c0`).
- DKS rows as the vendor UI makes them: one click = `0a` at that stage; a drag from stage a to b =
  `06` at a, `04` in between, and `08` added to the stage before b (`0e` when b = a + 1).
  GUESS: several strokes in one row (each starting after the previous one's last stage) are the
  OR of their bytes, e.g. once at stage 0 then once at 2 = `0a 00 0a 00`. Magnetar offers them.
- Saving over a key that has an advanced key: the vendor app sends the type `00` clear first; it
  doesn't clear a key that has none. Removing a SOCD pair clears both keys, one frame each.
- After each write the app reads the list, then each listed key.
- Sent by Magnetar and checked on the board 2026-11-21: Mod-Tap on Insert (hold R, tap T, 322 ms),
  DKS on End (depths 200/600/600/200, B held bottom → release from bottom, C once at full release),
  SOCD PgUp + PgDn (last input). Each read back identical (list and per key), then cleared with type
  `00`; the list was empty and the layer-0 actions unchanged afterwards. Toggle, Multi-Point,
  Release and DKS were sent from Magnetar's Advanced tab and typed as expected on 2026-11-25.
- An advanced key fires on the Default layer only: with a Release on Insert, Fn+Insert did not fire
  it (2026-11-25).
- Vendor ranges (`akko-capture13.txt` marks 16–21, frames in `tests/fixtures/vendor-ranges.txt`): Mod-Tap
  Hold Duration 0–1000 ms (max saved = `03 e8`), Multi-Point Binding Depth 0.100–3.400 mm (max saved = `02 a8`,
  680). Only the value at Save is sent, so the minima are what the UI showed. A new Multi-Point fills the
  other two slots with depths `00 c8` (1.00 mm) and `01 2c` (1.50 mm), action `00 00 00 00`.
- No limit found on the count: the board kept a Mod-Tap on 80 keys at once (every key but Esc,
  Insert and Fn), each checked with a per-key read, then all cleared (2026-11-25).

Read: `92 00` with no key (`pkt_count 1`, len 0) → the IDs (u16) of the keys that have an advanced key,
both keys of a SOCD pair; empty when none (captures 2, 10, 11). `92 00` with `key` → the type in the arg
byte and the same payload as the write; a key with none answers arg `00` and just its key. Magnetar
reads both like that (`core/`; checked on the board 2026-11-21, see above).
In `akko-capture.txt` (the oldest, 2026-10-03) the list read answered 4 packets (216 B) of what looks
like the physical layout: column-major, 6 rows × 18 cols, 0 = no key. Later captures never did;
Magnetar asks for any extra packets and skips IDs that aren't keys.
The list answer always says `pkt_count 01`, but its len counts every ID: `38` (56) at 28 keys, `3a`
at 29, `a0` at 80 (2026-11-25). Only the first 28 IDs fit in the frame, and how to get the rest is
unknown; per-key reads see all of them.

### Per-key travel (Parameter Settings → Actuation Settings)
Source: `akko-capture3.txt` (2026-11-18). Test key F12 = `0d`. Writes are echoed, then the web app
reads the same keys back. Multi-packet: `pkt_count`/`pkt_index` as usual, `arg` 0. Reads list the key IDs
wanted (any subset works); a full read is `93` 8 × 11 keys, `96`/`99` 12 × 7 keys, in the keymap read order.
`13`/`16`/`19` and their reads are KNOWN: `core/` sent each one on F12 and read back exactly what it wrote, and
`13` to all 83 keys at once (8 packets) also read back. `16` was only ever sent one key per frame (vendor and
ours). Out of the box (`akko-capture2.txt`): actuation 0.6 mm, RT off at 0 / 0, dead zones off at 0.13 / 0.125.
The vendor app has no restore-default or factory button for travel; Parameter Preset is the only bulk change
(`akko-capture13.txt` mark 27).

| cmd/sub | Status | Per key | Notes |
|---|---|---|---|
| `13 00` / `93 00` | KNOWN | `key u16, actuation u16, 00` (5 B, 11 per packet) | 1.37 mm → `01 12` (274), 0.42 → `00 54` (84), 0.6 → `00 78` (120). Select All = 83 keys, 8 packets. Marks "Select All, actuation -> 1.37 mm", "Deselect, select F12 only…", "restore…" |
| `19 00` / `99 00` | KNOWN | `key u16, rt on u8, press u16, release u16, 00` | Rapid Trigger. The panel has only the RT toggle, a sensitivity slider and the independent toggle; there is no "continuous RT" option (the wording in `akko-capture4.txt` mark 1 is a slip). On with defaults = press 1 / release 1 (0.005 mm). Linked slider writes press = release (0.13 → `1a`/`1a`). Independent: press 0.17 → `22`, release "0.29" → `39` (57 = 0.285; the UI rounds). Off keeps the values. The "independent" toggle sends nothing (UI only). Vendor range 0.005–3.5 mm (`0001`–`02bc`) for press and release alike, SEEN and read back (`akko-capture13.txt` marks 22–23) |
| `16 00` / `96 00` | KNOWN | `key u16, top u16, bottom u16, on u16` | Dead zones. Toggle on → `00 1a 00 19 00 01` (top 0.13, bottom 0.125). Top 0.185 → `25`, bottom 0.23 → `2e`. Off → last u16 `00`, values kept. Read = 12 packets × 7 keys. Vendor range 0–0.5 mm (`0000`–`0064`) for both, SEEN and read back (`akko-capture13.txt` mark 24) |
| `15 00` / `95 00` | SEEN | `key u16, model u8` | Switch model (Switch Settings). Read sends `key` only. Ultra Mag Pro+ `1f`, Star Magnetic `27`, Xingyao `43`, Star Gravity `44`, Luminous `45` (stock); all five SEEN (`akko-capture11.txt` marks 19–21). `akko-capture4.txt` marks "F12 -> Xingyao", "back to the old model" |

### Macros (`akko-capture7.txt`)
All macros live in one blob (UI: "Macro Storage: N B / 4096B"), rewritten whole on every save or delete.

| cmd/sub | Status | Meaning |
|---|---|---|
| `05 00` | KNOWN | write the whole blob: `pkt_count` × up to 56 B, `pkt_index` 0.., arg `00`, echoed. Empty blob (last macro deleted) = one packet, len `00` |
| `85 00` | KNOWN | read: bytes 1–2 (sub + arg) = byte offset, u16 BE (`00 00`, `00 38`, … `01 18`, `akko-capture11.txt`), len = bytes wanted. The app reads 2 B (directory size), then the directory, then the blob. Empty storage reads `ff ff` |

Reads longer than 56 B go in packets like the writes: `85 <offset u16 BE> <pkt_count> <pkt_index> <len>`, the
offset stepping by `38`. The directory is read that way too once it is over 56 B (15+ macros): 60 B =
`85 00 00 02 00 38` then `85 00 38 02 01 04`; the blob (231 B) = 5 packets at offsets `00 38 70 a8 e0`, the last
len `07`; at exactly 56 B (14 macros) the directory is one packet (`akko-capture13.txt` marks 1 and 4, frames in
`tests/fixtures/vendor-macros-long.txt`). Magnetar's `readMacros()` sends exactly these frames (offsets, counts,
indices and lens); `writeMacros()` matches the vendor's 5-packet `05 00` write too (`longDirectoriesLikeTheVendorApp`; not yet sent to the board with 15+ macros).

Blob layout (DECODED, sizes match the UI: 127 B, 150 B, 165 B):
- Directory: one entry per macro, `offset u16, size u16`, **little-endian** (unlike the rest of the
  protocol; confirmed above 256: one 609 B macro = `04 00 5d 02`, size 605, `akko-capture11.txt` mark 35). Directory size = 4 × macros, so the first u16 is also the
  first macro's offset.
- Macro at `offset`: `name_len u8, name[name_len]` (ASCII; the UI requires ≥ 2 chars), then actions.
- Action (4 B): `dir u8, delay u16 BE, hid u8`. dir `00` press, `80` release, `10` modifier press, `90` modifier release,
  `20` mouse button press, `a0` mouse button release (bit `80` = release, bit `10` = modifier, HID `e0`–`e7`, bit `20` =
  mouse, last byte = button: `01` left); delay = ms to wait after
  this action; the last action gets `1`. "Use Standard Delay (50ms)" sets every delay to 50.
  e.g. "ab" = `02 61 62 | 00 00 32 04 | 80 00 32 04 | 00 00 32 05 | 80 00 01 05`;
  "cc" recorded as c↓, 137 ms, c↑ = `02 63 63 | 00 00 89 06 | 80 00 01 06`.
- Mouse steps (SEEN, `akko-capture13.txt` mark 7): the vendor recorder records real mouse clicks (not volume
  or media keys, mark 6). One left click = `02 6d 73 | 20 00 3e 01 | a0 00 01 01` ("ms", 62 ms down). Only
  the left button was captured; the button byte is GUESS `01`–`05` (left, right, middle, back, forward) as in
  the keymap's `01 bb 01 00`. Magnetar writes a left click with these bytes (fake device only, never sent to
  the board) and also offers right, middle, back and forward as `02`–`05` (GUESS, fake device only). Direction bytes outside `00 10 20` (+ `80`) are kept as read.
- Size: 15 macros "m1"…"m15" of one A tap each = 231 B (UI "231B"): 4 B directory entry + 1 + name + 8 B of
  steps each (`akko-capture13.txt` mark 2).
- Deleting a macro removes it and repacks the blob; indices of later macros shift down. The app
  refuses to delete a macro that is bound, or to bind a key that already has one (both client-side:
  no traffic). A key whose index points past the last macro sends one empty mouse report (report
  `02`, all zero) and nothing else; a macro with a name and no steps sends one empty keyboard report
  (2026-11-25, listened on the keyboard's input interfaces).
- Playback: the macro's keys go out on interface 1 (NKRO bitmap, report `07`, usages `00`–`9f`) and
  its modifiers on interface 0 (boot report), so they arrive from two input devices (Hyprland
  doesn't share Shift between them). Pressing the key again during playback restarts the macro and
  drops the pending steps of the run in progress (2026-11-25).
- Sent by Magnetar 2026-11-21: one macro written, read back, bound to Insert (`03 01 01 00`), ran on
  the board; then the empty blob written and Insert restored.
- **Per profile**: after that write, profile 2 read as empty storage.
- Modifiers in steps (KNOWN, `akko-capture12.txt` marks 2–8): L-Shift↓ A↓ A↑ L-Shift↑ =
  `02 73 68 | 10 00 32 e1 | 00 00 32 04 | 80 00 32 04 | 90 00 01 e1`; R-Ctrl + C the same with `e4`, `06`.
  The editor listed them as "L-Shift Down … L-Shift Up". Deleting in the other order (`ct` then `sh`,
  marks 9–10) repacked the same way.
- Sent by Magnetar 2026-11-22 (those exact bytes, bound to Insert): the board's USB reports showed
  L-Shift on interface 0 (boot keyboard, modifier byte `02`) but A on interface 1 (NKRO report `07`,
  bitmap byte `10`), Shift released after A. Linux makes those two input devices with separate
  modifier state, so the desktop (Hyprland) typed "a". Typed by hand, Shift and A both come on
  interface 0. A host that merges modifiers across keyboards (likely Windows/macOS) would see "A".
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
| `94 05` | KNOWN | read calibration: request = list of `key u16` (10 packets), reply `key u16, rest u16, bottom u16` (raw sensor, e.g. `01`: 10634 / 1743) = the **stored** calibration. Sent on Start Calibration |
| `94 00` | KNOWN | calibration keep-alive, every 1 s while calibrating (echoed). Keys don't type in this mode. No `94 02` report comes while no key moves (live run 2026-11-22: 5 s, none) |
| `94 02` | DECODED | unsolicited IN every 50 ms while calibrating: `len / 6` × `key u16, cur u16, min u16` (1–9 entries, the keys moved most recently; bytes past `len` are stale). cur bits 0–14 = raw sensor; min = lowest value seen this session. cur bit 15: set almost always, untouched keys included; clear for one report as a key comes back up after a full press (capture5, 125 of 6600 entries). Meaning unknown, not used |
| `94 04` | KNOWN | Finish Calibration (the only other button; there is no Save/Cancel). Commits the session, see below. Leaving calibration without Finish sends it too (marks "leave … WITHOUT Save", "leave calibration without Save"). If the app dies mid-session (no Finish), the keys stay dead for 15 s or more, until the next command (a read is enough; 2026-11-25) |

Calibration semantics, from `akko-capture5.txt` (each Start Calibration reads all 83 keys, so each
session shows the result of the previous one):

- Live run (2026-11-22, our code): read, 5 s session with no key touched, Finish, read: no bottom
  changed, 2 rests moved by ≤ 3; the board answered normally after.
- Finish (`94 04`) stores, **for each key moved during the session**, the lowest raw value it reached
  as its new bottom. Keys not touched keep their bottom (rest drifts by ±3, i.e. re-measured or noise).
  Mark "calibrate every key…": all 83 keys changed. Opening then finishing with no key moved: no change.
- A key pressed only halfway gets a **wrong bottom**: F12 went 2225 → 2307 → 2326 → 4272 over three
  short sessions where it was pressed lightly (marks "press only F12 lightly…"). Nothing guards against it.
- Between two sessions with no calibration in between (marks 1 → 2), 24 keys' bottoms moved by up to
  +22 while the board was used normally. GUESS: Adaptive Calibration (Base Functions) adjusts them live.

## Still to capture
- Profile Management: applying an inactive profile (reboots, overwrites a profile). Skipped on purpose.
- Device Info: Factory Reset. Skipped on purpose.
- `11 02` (Reset on the Fn1 layer): Default and Fn are SEEN, payload `01` both times.
- No UI found for `82 xx`, `84 11/13/1d/1e/20`, `87 00`, `a6 ff`: maybe on-board Fn shortcuts.
  Diff read dumps before/after Fn combos (reads only).
