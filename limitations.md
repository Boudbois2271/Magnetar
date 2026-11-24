# Limitations and open questions

Things touched but not finished, known issues, and doubts. Remove an entry once it is solved and
reflected in `akko-protocol.md`.

## Protocol gaps

### Partly decoded
- **Macros** (`akko-capture7.txt`): blob, actions and binding decoded (`akko-capture7.txt`, `7b`).
  Unknown: what a key does if its macro index points past the last macro (the app prevents it);
  modifier steps (dir `10`/`90`) work on the board, but the firmware sends the modifier on interface 0
  and the keys on interface 1 (NKRO), so on Linux (Hyprland) the modifier doesn't apply: a Shift+A
  macro types "a". No workaround known yet. Directories over 56 bytes
  (15+ macros) are read in packets by our code, never captured.

### Not captured at all
- **Combo Optimization**: the toggle is decoded, what it does on the board is not known.
- **Adaptive Calibration vs bottom drift**: not tested (would need calibration read-backs with it off).
- `82 xx` read blocks and `82 01` (`11 00 00 00 00 59`) are unknown; they are not Base Functions.

### Captured but not decoded
- **Mural**: the app streams an audio visualiser only (no paint or still-image page: Lighting's
  per-key colours already store a still). How long the board keeps the last frame was only timed
  by eye (1–3 s), and there is no stop command, so after another effect is picked or the app
  quits the board keeps the last frame that long before its own effect comes back; frames more than 6 packets long can't happen (83 LEDs × 4 B at most). Between
  ~1 s streaming sessions the board gets no frames for ~60 ms (longer while an action runs), far
  under that fallback. Audio comes from `@DEFAULT_MONITOR@`, resolved when capture opens:
  changing the default output while it runs is not followed until it is turned off and on.
  The board shows Mural frames as sent, ignoring its brightness: the app scales the colours by
  the board's brightness itself (linearly; the board's own curve for effects is not known).
  Visualiser is an app effect, not the board's: `akkoctl` and the bar widget show the board's own
  effect underneath while it runs, and setting an effect there doesn't stop it.
- **DKS stage bits** (`02` down, `04` hold, `08` up) fit five captured cases (captures 10, 11); `04`
  and `08` were never sent alone.
- **System (`07`) and lighting (`08`) actions** in the keymap: only `08 03 00 00` (brightness cycle) is
  SEEN; the rest are guessed from defaults. The System & Shortcuts tab gave a plain RGui key, not `07`.
- **`11 00` layer reset**: payload `01` seen on Default Layer only; its meaning is a guess.
- **Key calibration**: Finish (`94 04`) is understood, but the stored bottom of F12 after a full
  press (2225) is not exactly the lowest `94 02` min seen (2204). The bit-15 flag in `94 02` is a
  unknown (not "reached bottom": it is set on untouched keys too). Bottom drift between sessions is
  guessed to be Adaptive Calibration. Calibration mode stops the keys typing.
- **Calibration timeout**: what the board does if keep-alives (`94 00`) stop without a Finish (app
  killed) is not known. Magnetar sends Finish on quit, on leaving the page and on errors.
- `84 11` (`01`), `84 13` (`00 b4`), `84 1d`/`84 1e` (`00`), `84 20` (`01`), `85 00`, `87 00`, `a6 ff`: unknown.

### Units
- Travel is 0.005 mm per unit, confirmed against the UI by actuation, RT and dead-zone writes
  (`akko-capture3.txt`).
- Trailing byte of the `13`/`19` entries: always `00`, meaning unknown. Magnetar writes back what it read.
- Factory travel values: `akko-capture2.txt` reads 0.6 mm (matching the UI); `akko-capture.txt` read
  1.00 mm on 83 keys (and `0028` on another read). Where 1.00 came from isn't known; the board under test
  had 1.00 mm on every key on 2026-11-21.

## Existing tooling (`akkoctl`, widget)

- Omarchy only loads plugins from `~/.config/omarchy/plugins/`, which a package can't write to:
  `bin/package` links the packaged widget there, so installing the `.pkg.tar.zst` by other means
  leaves it unlinked. A dev `~/.local/bin/akkoctl` (from `bin/install`) wins over the packaged one,
  in the widget and in most shells' `PATH`, and goes stale unless reinstalled.
- The CLI's `state.json`: other keys are kept as written, but a file that isn't a valid JSON object
  is replaced by `{"lastBrightness": N}` on the next save (as the Python version did).
- The CLI's behaviour is pinned to recorded runs of the old Python tool
  (`tests/fixtures/python-cli.jsonl`), against a fake keyboard. Real-board checks were done by hand.
- The widget depends on Omarchy's own QML modules
  (`qs.Commons`, `qs.Ui`), so it only runs inside Omarchy's shell.
- The widget shows the colour picker on profile 1 only.
- `profileCount()` reads `82 0F` payload[3]. Only one sample (value 3) backs that offset; a value
  outside 1–8 falls back to 3.
- `transact()` matches responses on `cmd`/`sub`/packet index, not `arg` (which only advanced keys use).
- The CLI only covers profile and global lighting (`10/90 00`, `04/84 01`); the keymap and the
  Base Functions settings are app-only.

## Magnetar (desktop app)

- Pages: Keys (tabs Action, Travel, Advanced), Lighting (with per-key colours and the Visualiser
  effect), Mural (workbench for app-streamed effects), Macros, Settings (with Calibration and Device).
- Settings page: the four writes were sent by Magnetar's core and read back on the board, but the
  page itself hasn't been used by hand on the board yet. Whether these settings are per profile is
  unknown (they're read with every batch, so a profile switch would show it). What Combo
  Optimization actually changes is unknown; the page's description stays vague on purpose.
- Polling rate: the board drops off USB right after the write and is back after ~1 s. The app waits
  1.2 s, then retries quietly for up to 10 s before showing an error. Anything queued in the same
  batch after the change is dropped. 8000/4000/2000 Hz were written by the vendor app only.
- Per-key colours (Lighting page): tested against the fake keyboard only, not used on the board yet. Assumed per profile
  (read again on a profile switch), not checked. No undo; no per-key brightness or effects.
- Lighting page: the brightness slider offers 0, 1, 5, 10, 15 and 20 only; other levels are set with
  `akkoctl` (and shown as read). The Lighting drawing is sized like the Keys one by the same rule,
  not shared with it: the two can differ by a few pixels.
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
- Advanced tab: a DKS row can hold several strokes (e.g. once at ↓1, then held ↑1→↑2): all 34
  patterns of the four points are offered. Rows of several strokes are composed from the stage
  bits (`02`/`04`/`08`), never captured nor tried on the board; whether the vendor app can make
  them is unknown. Stage bytes that match no pattern are still shown as "another pattern" and kept.
- Advanced tab: no Undo (Remove, or saving the old values again).
- Advanced tab: in a window at its minimum width the SOCD tag on 1u keys is cut ("SO…").
- Macros page: recording maps a key event's `nativeScanCode()` to a HID
  usage assuming the Linux XKB keycode (evdev + 8), the same on Wayland and xcb (works on Wayland;
  xcb not tried). Keys outside the evdev table (media keys, ...) record as nothing.
- Macros page: steps can only use keyboard usages (no mouse
  or media steps: never captured). A step's key can't be changed in place (remove it, add another).
  Releases of keys pressed before Record are dropped; keys still held at Stop get no release step.
- Macros page: a macro with a name and no steps can be saved (the blob format allows it); never
  written to the board or captured from the vendor app, so what the board does with it is unverified.
- Travel page: the Rapid Trigger (up to 1.00 mm) and dead zone (up to 0.50 mm) caps are ours, not the
  vendor's, whose maxima were never captured. Keys set beyond them by the vendor app are shown but
  their slider is locked.
- Calibration screen (Settings → Calibrate): "pressed all the way down" (green) means 95 % of the travel stored at Start.
  A key whose stored bottom is already too shallow turns green early. No undo: Finish is the
  board's only way out, and the old values can't be written back.
- Calibration screen: while a session runs, other changes (profile shortcuts included) wait for Finish;
  the profile picker is disabled.
- Travel page: assumed per profile (read again on a profile switch), not checked on the board.
- Travel page: no "restore default" (factory values aren't certain, see Units) and no presets; Undo
  covers the last change only. Switch model (`15 00`) and calibration aren't on the page.
- Travel page: dead zones are sent one key per frame (83 frames for all keys), the only form captured.
- Travel page: in a window at its minimum width the 1u keycap numbers shrink to 7 px while wide keys keep a larger size.
- Keys page: system (`07`), profile (`09`) and lighting (`08`, except brightness cycle)
  actions are shown but can't be assigned; the Fn layer's factory ones show as "System function" /
  "Lighting control" because their meaning is a guess. Mouse buttons 4/5 and media keys other than
  the twelve in the factory Fn layer aren't offered (not captured).
- Keys page: the layer reset `11 00` isn't used; resetting is per key ("Restore default", from the
  factory table in `core/keymap.cpp`).
- Keys page: labels are the US keycap names; on an AZERTY system the letters still show QWERTY.
- Keys page: at the smallest window size the keycap labels shrink and some get cut ("Éc…").
- Keys → Colour lights each legend in its LED colour (on a light theme, on a dark cap); a dark
  blue or purple legend is hard to read on either theme. Keys → Travel's depth gauge is
  linear over the board's whole range, so 0.30 and 0.60 mm differ by a few pixels; the value says it.
- A key write the board doesn't keep shows as a connection error banner, then the keymap is re-read.
- Profiles show as "Profile N": the stored names (`9a PP`) aren't read yet.
- Changes made on the board (Fn shortcuts) show up when the window regains focus or on Ctrl+R;
  brightness shows at once from the unsolicited `fe 0b` reports (vendor node opened read-only,
  tested against a fake node only, not with Fn on the board yet). Other unsolicited reports are
  ignored.
- Plugging in is noticed through a udev watch (`hidraw` add events of `38ee:0016`), tested against
  the fake keyboard only, not by replugging the board yet. Without udev it retries every 3 s; with
  it, "not found" is retried every 10 s as a fallback. Unplugging is still noticed on the next read.
- Firmware version byte order (`82 02` = `08 01` → v1.08, low byte first) rests on one sample.
- Colour picker (Lighting): checked offscreen only (tests, EN/FR screenshots, dark and light), not
  used by hand on the desktop yet. A colour is sent when the field or hue strip is let go (or 300 ms
  after the last arrow key), so the board doesn't follow the drag; the drawing does. After a send,
  the picker keeps the chosen colour until the read-back changes it: a write that fails without an
  error would leave it showing that colour. Beside the option rows it needs a page about 1100 u
  wide: at the default window it fits, near the minimum width (900 u) it is cut off on the right.
  Saved colours live in `~/.config/magnetar/settings.ini`, shared by every profile.
- Text size: follows GNOME's `text-scaling-factor` (what `omarchy display text size` sets) times the
  interface font Qt gets from its gtk3 platform theme. The live update (dconf watch) was not tried on
  the desktop. Other desktops without that key (KDE) get Qt's own font size, unscaled.
- UI redesign (`docs/ui-plan.md`, items 1–18): checked offscreen only (tests, EN/FR screenshots in
  a dark and the flexoki-light theme), then reviewed once on the desktop (findings R1–R8 in
  `docs/ui-plan.md`, all fixed). Not used on the board yet. Light
  themes were tried with one theme only. Keyboard focus is tested with Tab offscreen; real keyboard
  use on the desktop isn't. Space toggles the lights only while no control has keyboard focus.
- Fitting the window: checked offscreen only (tests and screenshots at 12 px text), not yet looked
  at on the desktop. At the default size (1440 × 920 layout px, 1234 × 789 at 12 px) every page fits,
  Keys with every tab and Advanced editor (picker open too): the keyboard is sized so the controls
  under it get a fixed height. At 1920 × 1030 everything fits. A tiled window below the minimum size
  (three columns on a 1080p screen) cuts pages off instead of scrolling.

## Firmware behaviour
- Two concurrent HID sessions wedge the keyboard: it stops sending keystrokes until a clean command
  arrives. Any tool must serialise access; the web app and our code can't run together.
- Each effect stores its own colour and speed; `04 02` loads them (it doesn't reset speed).
- Per-key colours (`06 00`): writes go in frames of at most 8 keys, the most ever captured in one
  frame (11 would fit). Verified on the board under Custom only; whether writes stick while another
  effect is on is untested.

## Repo / docs
- Capture logs are not published, so the evidence behind `akko-protocol.md` can't be checked
  from the public repo alone.
- The DKS and advanced-key decoding relied on screenshots of the web UI that are not in the repo.

## Legal
- The vendor web app's terms of use were not found, so it's unknown whether they forbid reverse
  engineering. In the EU, studying a program's behaviour (Directive 2009/24/EC, art. 5(3)) can't be
  taken away by contract (art. 8), and the project only observes USB traffic. This is not legal
  advice.
