# Limitations and open questions

Things touched but not finished, known issues, and doubts. Remove an entry once it is solved and
reflected in `akko-protocol.md`.

## Protocol gaps

### Partly decoded
- **Macros** (`akko-capture7.txt`): blob, actions and binding decoded (`akko-capture7.txt`, `7b`).
  Unknown: what a key does if its macro index points past the last macro (the app prevents it).

### Not captured at all
- **Combo Optimization**: the toggle is decoded, what it does on the board is not known.
- **Adaptive Calibration vs bottom drift**: not tested (would need calibration read-backs with it off).
- `82 xx` read blocks and `82 01` (`11 00 00 00 00 59`) are unknown; they are not Base Functions.

### Captured but not decoded
- **Mural**: frame format decoded, but the LED index → key mapping is only inferred from the grid, and the
  firmware's behaviour when frames stop (the 3 s fallback) was not observed in the log.
- **DKS stage bits** (`02` down, `04` hold, `08` up) fit five captured cases (captures 10, 11); `04`
  and `08` were never sent alone.
- **System (`07`) and lighting (`08`) actions** in the keymap: only `08 03 00 00` (brightness cycle) is
  SEEN; the rest are guessed from defaults. The System & Shortcuts tab gave a plain RGui key, not `07`.
- **`11 00` layer reset**: payload `01` seen on Default Layer only; its meaning is a guess.
- **Key calibration**: Finish (`94 04`) is understood, but the stored bottom of F12 after a full
  press (2225) is not exactly the lowest `94 02` min seen (2204). The bit-15 flag in `94 02` is a
  guess ("reached bottom"). Bottom drift between sessions is guessed to be Adaptive Calibration.
  Calibration mode stops the keys typing.
- `84 11` (`01`), `84 13` (`00 b4`), `84 1d`/`84 1e` (`00`), `84 20` (`01`), `85 00`, `87 00`, `a6 ff`: unknown.

### Units
- Travel is 0.005 mm per unit, confirmed against the UI by actuation, RT and dead-zone writes
  (`akko-capture3.txt`).
- Trailing byte of the `13`/`19` entries: always `00`, meaning unknown. Magnetar writes back what it read.
- Factory travel values: `akko-capture2.txt` reads 0.6 mm (matching the UI); `akko-capture.txt` read
  1.00 mm on 83 keys (and `0028` on another read). Where 1.00 came from isn't known; the board under test
  had 1.00 mm on every key on 2026-11-21.

## Existing tooling (`akkoctl`, widget)

- `bin/install` covers the CLI only: the widget is still copied by hand to
  `~/.config/omarchy/plugins/`, and the two copies can drift apart.
- The CLI's `state.json` only keeps `lastBrightness`: any other key written there by hand is dropped
  on the next save (the Python version kept them).
- The CLI's behaviour is pinned to recorded runs of the old Python tool
  (`tests/fixtures/python-cli.jsonl`), against a fake keyboard. Real-board checks were done by hand.
- The widget hardcodes `~/.local/bin/akkoctl` and depends on Omarchy's own QML modules
  (`qs.Commons`, `qs.Ui`), so it only runs inside Omarchy's shell.
- The widget shows the colour picker on profile 1 only.
- `profileCount()` reads `82 0F` payload[3]. Only one sample (value 3) backs that offset; a value
  outside 1–8 falls back to 3.
- `transact()` matches responses on `cmd`/`sub`/packet index, not `arg` (which only advanced keys use).
- The CLI only covers profile and global lighting (`10/90 00`, `04/84 01`); the keymap is app-only.

## Magnetar (desktop app)

- Pages: Keys (tabs Action, Travel, Advanced; Colour to come), Lighting and Device.
- Keys page: editing many keys at once (one action, one advanced function on each, Remove on several)
  is tested against the fake keyboard only. The board's maximum number of advanced keys is unknown.
- Advanced keys: Mod-Tap, DKS, SOCD and the clear were sent by Magnetar's core and read back on the
  board; Toggle, Multi-Point and Release were not (frames match the vendor captures only). The page
  itself hasn't been used by hand on the board yet.
- Advanced tab: hold time 100–1000 ms and depths 0.10–3.50 mm are our limits; the vendor
  app's ranges were never captured (seen: 200 default, 322, 965 ms).
- Advanced tab: whether an advanced key applies on every layer, and how it combines with the
  key's keymap action, isn't known. Assumed per profile (read again on a profile switch).
- Advanced tab: clearing one key of a SOCD pair might leave the other key's record on the
  board; Magnetar always clears both, like the vendor app.
- Advanced tab: DKS patterns other than "once at a point" or "held from one point to another"
  (e.g. two separate taps in one row) are shown as "another pattern" and kept as read, not editable.
- Advanced tab: no Undo (Remove, or saving the old values again). Macros can't be chosen as an
  action (the picker has no macros yet).
- Advanced tab: at 760 px wide the SOCD tag on 1u keys is cut ("SO…").
- Travel page: the Rapid Trigger (up to 1.00 mm) and dead zone (up to 0.50 mm) caps are ours, not the
  vendor's, whose maxima were never captured. Keys set beyond them by the vendor app are shown but
  their slider is locked.
- Travel page: assumed per profile (read again on a profile switch), not checked on the board.
- Travel page: no "restore default" (factory values aren't certain, see Units) and no presets; Undo
  covers the last change only. Switch model (`15 00`) and calibration aren't on the page.
- Travel page: dead zones are sent one key per frame (83 frames for all keys), the only form captured.
- Travel page: at 760 px wide the 1u keycap numbers shrink to 7 px while wide keys keep a larger size.
- Keys page: macros, system (`07`), profile (`09`) and lighting (`08`, except brightness cycle)
  actions are shown but can't be assigned; the Fn layer's factory ones show as "System function" /
  "Lighting control" because their meaning is a guess. Mouse buttons 4/5 and media keys other than
  the twelve in the factory Fn layer aren't offered (not captured).
- Keys page: the layer reset `11 00` isn't used; resetting is per key ("Restore default", from the
  factory table in `core/keymap.cpp`).
- Keys page: labels are the US keycap names; on an AZERTY system the letters still show QWERTY.
- Keys page: at the smallest window size the keycap labels shrink and some get cut ("Éc…").
- A key write the board doesn't keep shows as a connection error banner, then the keymap is re-read.
- Profiles show as "Profile N": the stored names (`9a PP`) aren't read yet.
- Changes made on the board (Fn shortcuts) show up when the window regains focus or on Ctrl+R; the
  unsolicited `fe 0b` brightness reports aren't listened to.
- While unplugged it retries every 3 s; there is no udev hotplug watch.
- Firmware version byte order (`82 02` = `08 01` → v1.08, low byte first) rests on one sample.
- Colour choice is swatches plus a hex field, no free colour picker yet.
- No Arch package for `akkoctl` or the widget yet: `bin/package` installs Magnetar only.

## Firmware behaviour
- Two concurrent HID sessions wedge the keyboard: it stops sending keystrokes until a clean command
  arrives. Any tool must serialise access; the web app and our code can't run together.
- Each effect stores its own colour and speed; `04 02` loads them (it doesn't reset speed).
- Per-key colours (`06 00`): writes go in frames of at most 8 keys, the most ever captured in one
  frame (11 would fit). Not yet sent to the board; whether they stick while another effect is on is unknown.

## Repo / docs
- Capture logs are not published, so the evidence behind `akko-protocol.md` can't be checked
  from the public repo alone.
- The DKS and advanced-key decoding relied on screenshots of the web UI that are not in the repo.

## Legal
- The vendor web app's terms of use were not found, so it's unknown whether they forbid reverse
  engineering. In the EU, studying a program's behaviour (Directive 2009/24/EC, art. 5(3)) can't be
  taken away by contract (art. 8), and the project only observes USB traffic. This is not legal
  advice.
