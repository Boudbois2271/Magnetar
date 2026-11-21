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
- The UI showed 0.6 mm on every key, but `akko-capture2.txt` read actuation 200 (1.00 mm). No `93 00`
  read happened before the first write in `akko-capture3.txt`, so the true starting value is unknown.
- Actuation/RT/dead-zone writes are SEEN only: akkoctl doesn't send them yet. Trailing `00` byte in
  `13`/`19` entries: meaning unknown.

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
- Frames can be multi-packet now (keymap reads), but `arg` is always 0: no advanced keys yet.
- `transact()` matches responses on `cmd`/`sub`/packet index, not `arg`.
- The CLI only covers profile and global lighting (`10/90 00`, `04/84 01`); the keymap is app-only.

## Magnetar (desktop app)

- Pages: Lighting, Keys and Device.
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

## Repo / docs
- Capture logs are not published, so the evidence behind `akko-protocol.md` can't be checked
  from the public repo alone.
- The DKS and advanced-key decoding relied on screenshots of the web UI that are not in the repo.

## Legal
- The vendor web app's terms of use were not found, so it's unknown whether they forbid reverse
  engineering. In the EU, studying a program's behaviour (Directive 2009/24/EC, art. 5(3)) can't be
  taken away by contract (art. 8), and the project only observes USB traffic. This is not legal
  advice.
