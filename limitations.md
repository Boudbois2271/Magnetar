# Limitations and open questions

Things touched but not finished, known issues, and doubts. Remove an entry once it is solved and
reflected in `akko-protocol.md`.

## Protocol gaps

### Partly decoded
- **Macros** (`akko-capture7.txt`): blob, actions and binding decoded (`akko-capture7.txt`, `7b`).
  A key bound past the last macro sends one empty mouse report and nothing else (2026-11-25);
  modifier steps (dir `10`/`90`) work on the board, but the firmware sends the modifier on interface 0
  and the keys on interface 1 (NKRO), so on Linux (Hyprland) the modifier doesn't apply: a Shift+A
  macro types "a" (seen again on the board 2026-11-25). Workaround: a shortcut in Keys → Action.
  Pressing the key again while its macro plays restarts it; the cut-off run's pending steps (e.g.
  the Shift release) are never sent. Directories over 56 bytes are read in packets, as the vendor
  app does (`akko-capture13.txt`). Mouse steps (dir `20`/`a0`): left button only captured.

### Not captured at all
- **Combo Optimization**: the toggle is decoded; what it does is not known, and no public doc,
  review or forum post explains it (searched 2026-11-25, see `akko-protocol.md` Base Functions).
- **Adaptive Calibration vs bottom drift**: most likely the vendor's documented "Dynamic
  Calibration" (the board re-calibrates travel by itself), which fits the drift we saw; not tested
  on the board (would need calibration read-backs with it off).
- `82 xx` read blocks and `82 01` (`11 00 00 00 00 59`) are unknown; they are not Base Functions.

### Captured but not decoded
- **Mural**: the app streams an audio visualiser only (no paint or still-image page: Lighting's
  per-key colours already store a still). How long the board keeps the last frame was only timed
  by eye (1–3 s; about 4 s on video after a kill, 2026-11-25), and there is no stop command, so
  after another effect is picked or the app quits the board keeps the last frame that long before its own effect comes back; frames more than 6 packets long can't happen (83 LEDs × 4 B at most). Between
  ~1 s streaming sessions the board gets no frames for ~60 ms (longer while an action runs), far
  under that fallback. Audio comes from `@DEFAULT_MONITOR@`, resolved when capture opens:
  changing the default output while it runs is not followed until it is turned off and on.
  The board shows Mural frames as sent, ignoring its brightness: the app scales the colours by
  the board's brightness itself (linearly; the board's own curve for effects is not known).
  Visualiser is an app effect, not the board's: `akkoctl` and the bar widget show the board's own
  effect underneath while it runs, and setting an effect there doesn't stop it.
- **DKS stage bits** (`02` down, `04` hold, `08` up) fit five captured cases (captures 10, 11).
  `04` alone and `08` alone are dropped by the board (read back `00`, the key types nothing).
- **System (`07`) actions** in the keymap: guessed from the factory Fn layer only; the System &
  Shortcuts tab gave a plain RGui key, not `07`. Lighting (`08`), mouse 4/5, media and "www" (`11`)
  actions are SEEN (`akko-capture13.txt`); Magnetar offers all but "www", never sent to the board yet.
  What "www" does is unknown, and `08 09 nn` (select effect) is still a guess.
- **Layer reset** `11 LL` payload `01`: SEEN on Default and Fn, not on Fn1. The Fn read after it
  matches our factory Fn table (`tests/fixtures/vendor-actions.txt`).
- **Key calibration**: Finish (`94 04`) is understood, but the stored bottom of F12 after a full
  press (2225) is not exactly the lowest `94 02` min seen (2204). The bit-15 flag in `94 02` is a
  unknown (not "reached bottom": it is set on untouched keys too). Bottom drift between sessions is
  guessed to be Adaptive Calibration. Calibration mode stops the keys typing.
- **Calibration timeout**: when the app is killed mid-session (no Finish), the keys stay dead for
  15 s or more, until the next command from any tool (a read is enough). How long the board
  waits on its own is not known. Magnetar sends Finish on quit, on leaving the page and on errors.
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
  leaves it unlinked. Used after `bin/package` (2026-11-25): the widget (linked from the package)
  changes profile and colour, and Magnetar catches up when its window regains focus. A dev
  `~/.local/bin/akkoctl` (from `bin/install`) wins over the packaged one, in the widget
  and in most shells' `PATH`, and goes stale unless reinstalled.
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
- The CLI covers profile, global lighting (`10/90 00`, `04/84 01`), the keymap (`83/03/11 LL`) and
  the Base Functions settings (`04/84 15–19`); travel, advanced keys, macros, per-key colours and
  calibration are app-only. `akkoctl key` (set and `factory`) and `settings
  win-key-lock` worked on the board; `keymap` and the other settings are tested against the fake
  keyboard only. After `settings polling-rate` the keyboard re-enumerates, so
  the other three values printed are the ones read before the write. Keys are named by hex ID, not
  by label. `akkoctl key` refuses the GUESS action forms (`07`, `09`, `08 09`) unless they are the
  key's factory action.

## Magnetar (desktop app)

- Pages: Keys (tabs Action, Travel, Advanced), Lighting (with per-key colours and the Visualiser
  effect), Mural (workbench for app-streamed effects), Macros, Settings (with Calibration and Device).
- Settings page: used by hand on the board (2026-11-25): Win Key Lock and polling rates 8000,
  4000, 2000 and 1000 Hz all work (typing checked after each). Whether these settings are per profile is
  unknown (they're read with every batch, so a profile switch would show it). The page shows the
  switches by name only: Combo Optimization's effect is unknown and Adaptive Calibration's is a
  likely match, not confirmed (see above).
- Polling rate: the board drops off USB right after the write and is back after ~1 s. The app waits
  1.2 s, then retries quietly for up to 10 s before showing an error. Anything queued in the same
  batch after the change is dropped.
- Per-key colours (Lighting page): used on the board (2026-11-25): the LEDs match the drawing,
  and a colour set while Breathing runs is there on switching back to Custom. Assumed per profile
  (read again on a profile switch), not checked. No undo; no per-key brightness or effects.
- Lighting page: the brightness slider offers 0, 1, 5, 10, 15 and 20 only; other levels are set with
  `akkoctl` (and shown as read). The Lighting drawing shares the Keys sizing rule and matches it,
  except in a window too short for the Lighting options stacked under it: there it is smaller.
- Keys page: editing many keys at once (one action, one advanced function on each, Remove on several)
  was used on the board for Nothing / Restore default on three keys (2026-11-25); the rest is
  tested against the fake keyboard only.
- **Advanced keys past 28 (bug)**: the board keeps one on every key (80 tried, 2026-11-25), but
  the `92 00` list reply is one frame: its len goes past 56 (`a0` for 80 keys) while only 28 IDs
  fit. Magnetar trusts the list, so with more than 28 it can't show, verify or remove the rest
  (writes past the 28th report "kept another advanced key"). Not fixed yet.
- Advanced keys: Toggle (holds the action until the next tap), Multi-Point, Release and DKS were
  used from the page on the board (2026-11-25); Mod-Tap, SOCD and the clear were sent by the core
  and read back (2026-11-21).
- Advanced tab: hold time 0–1000 ms and Multi-Point depths 0.10–3.40 mm are the vendor app's
  (`akko-capture13.txt`). Hold time was captured on Mod-Tap only; Toggle uses the same range,
  assumed. DKS depths (0.10–3.50 mm) are still ours: the vendor's were never captured. Multi-Point
  and DKS depths move in 0.01 mm steps (the vendor shows 0.100, its step isn't known).
- Advanced tab: an advanced key fires on the Default layer only: Fn+Insert with a Release trigger
  on Insert did not fire it (2026-11-25). How it combines with the key's keymap action isn't
  known. Assumed per profile (read again on a profile switch).
- Advanced tab: clearing one key of a SOCD pair might leave the other key's record on the
  board; Magnetar always clears both, like the vendor app.
- Advanced tab: a DKS row can hold several strokes (e.g. once at ↓1, then held ↑1→↑2): all 34
  patterns of the four points are offered. Rows of several strokes are composed from the stage
  bits (`02`/`04`/`08`), never captured; "Once ↓1 ↑1" typed b then b on the board as expected
  (2026-11-25), the others weren't tried. Whether the vendor app can make them is unknown. Stage bytes that match no pattern are still shown as "another pattern" and kept.
- Advanced tab: Undo walks back the last 20 saves or removals and Redo walks forward again, both
  held in memory only (gone when the app closes) and cleared on a profile switch; a new save or
  removal drops what is left to redo. No keyboard shortcuts. Several undos in a row worked on the
  board (2026-11-26), and so did Redo.
- Advanced tab: from the minimum window size up the SOCD, DKS and Multi tags show whole on 1u
  keys; the longer Mod-Tap, Toggle and Release tags are cut there ("Mod…") until the window is
  wider. Shorter tag names would fix it (a wording choice, not made).
- Advanced tab: the keyboard is drawn smaller than on Action and Travel (the key list sits beside
  it, and the editor under it is taller), so the keys shift when switching to that tab. In a narrow
  window the key list keeps 220 px (at 14 px text) and the keyboard gets small; the action picker
  then replaces the editor column while it is open instead of sitting beside it, until an action
  is picked or Back is pressed (Save is in the column). In a short window DKS puts its four points
  beside its four actions. Saving from a tiled window (1028 × 574, a 1440p screen at 125 %)
  worked on the desktop (2026-11-26).
- Macros page: recording maps a key event's `nativeScanCode()` to a HID
  usage assuming the Linux XKB keycode (evdev + 8), the same on Wayland and xcb (both record the
  right keys, checked by hand). Keys outside the evdev table (media keys, ...) record as nothing.
  The vendor app's recorder takes real mouse clicks but not volume keys (they act on the system).
- Macros page: steps use keyboard usages or a mouse button: every one the Action tab offers (left,
  right, middle, back, forward; not shortcuts, not nothing). Left click (`20`/`a0` button `01`) is SEEN in
  `akko-capture13.txt`, run on the board 2026-11-26. Right/middle/back/forward (`02`–`05`, the keymap's
  numbering) are a GUESS, only tested against the fake device: **Ilan must test them on the board**
  (save a macro with each, run it, check the click). Clicks aren't recorded (add them by hand); the vendor's
  recorder takes them. Steps of unknown kinds are shown as "Unknown step" and kept on save. The
  vendor recorder has no media steps either. A step's key is changed from its row's list, along with
  its press or release partner (the next release / last press of that key); keys the list doesn't
  offer (recorded beyond `52`, e.g. keypad) show their name and can be changed, not picked back.
  Releases of keys pressed before Record are dropped on purpose (the macro would release a key it
  never pressed); keys still held at Stop get a release step at Stop's time.
  In a narrow steps box (< 560 u) the ↑ ↓ buttons hide to leave the key list room; dragging moves steps.
- Macros page: a macro with a name and no steps can be saved (the blob format allows it); on the
  board its key sends one empty keyboard report and nothing else (2026-11-25).
- Travel page: Rapid Trigger 0.005–3.5 mm and dead zones 0–0.5 mm are the vendor app's ranges
  (`akko-capture13.txt`), so nothing the vendor app sets falls outside the sliders. Dragging a
  3.5 mm slider moves about 0.02 mm per pixel; finer values are typed in the mm box.
- Calibration screen (Settings → Calibrate): "pressed all the way down" (green) means 95 % of the travel stored at Start.
  A key whose stored bottom is already too shallow turns green early. No undo: Finish is the
  board's only way out, and the old values can't be written back.
- Calibration screen: while a session runs, other changes (profile shortcuts included) wait for Finish;
  the profile picker is disabled.
- Travel page: assumed per profile (read again on a profile switch), not checked on the board.
- Travel page: no "restore default" (the vendor app has none either; factory values aren't certain,
  see Units) and no presets; Undo
  covers the last change only. Switch model (`15 00`) and calibration aren't on the page.
- Travel page: dead zones are sent one key per frame (83 frames for all keys), the only form captured.
- Travel page: the keycap numbers are one size on every key, the largest that fits a 1u cap
  beside its depth gauge; at the minimum window size that is about 8 px with 14 px text
  (7 px with 12 px text). Larger needs a larger drawing or shorter numbers.
- Keys page: system (`07`), profile (`09`), "www" (`11`) and select-effect (`08 09 nn`) actions are
  shown but can't be assigned; the Fn layer's factory ones show as "System function" / "Profile N" /
  "Lighting control". The vendor's 15 lighting actions, mouse back/forward and media keys are
  offered (Lighting, Mouse, Media) but untried on the board; Fn+Left/Right show the vendor's swapped
  "Direction right"/"Direction left".
- Keys page: "Reset … layer to factory" (`11 LL`, then the layer read again) is offered on Default and
  Fn only (Fn1's wasn't captured), untried on the board. It resets at once, no confirmation; undone
  only by setting the keys again. Per-key "Restore default" uses the factory table in `core/keymap.cpp`.
- Keycap labels follow the desktop's layout (Wayland compositor keymap, else `XKB_DEFAULT_LAYOUT`),
  read once at start, first layout group only: switching layouts while the app is open, or a
  second group, isn't followed. X11 sessions without those variables keep the US names. Only
  the typing keys change; Enter, F1, arrows… keep their names. The CLI stays on US names.
- Keys page: keycap legends fit whole from the app's minimum window size up (the drawing never
  goes under 560 px at 14 px text). In a window tiled narrower than that (a third of a 1080p
  screen) the longest 1u names ("Échap") can still be cut; no window rule forces the
  minimum size (kept on purpose). At the minimum window height the
  pages run past the bottom edge (they already did before the larger drawing).
- Legends on LED colours (Lighting, Mural, and lit legends, which no page uses since per-key
  colour moved to Lighting) reach WCAG 4.5:1 against the cap as drawn (tests, offscreen screenshots
  in tokyo-night and flexoki-light): a coloured cap gets black or white ink; a lit legend too dark
  to read is lifted in OKLCH, same hue, so a deep blue shows as a lighter blue. Not judged against
  the board's real LEDs. Keys → Travel's depth gauge is
  linear over the board's whole range, so 0.30 and 0.60 mm differ by a few pixels; the value says it.
- A key write the board doesn't keep shows as a connection error banner, then the keymap is re-read.
- Profile names (`1a`/`9a PP`): the app reads and renames them (1–55 printable ASCII characters), not
  yet sent to the board (SEEN only). The vendor app's own length limit and non-ASCII names are
  unknown; a name can't be set back to the stock all-`ff` (never captured), so an empty name is
  refused. `akkoctl` and the bar widget still show "Profile N" (their JSON stays byte for byte), and
  so does a key bound to a profile switch on the Keys page.
- Changes made on the board (Fn shortcuts) show up when the window regains focus or on Ctrl+R;
  brightness shows at once from the unsolicited `fe 0b` reports (vendor node opened read-only,
  reopened after every read of the board; the board's reports match the parser, 2026-11-25). On
  the board the page didn't follow them (2026-11-25), most likely because the listener stayed
  closed after its node went away unseen; the fix is tested against the fake keyboard only, not
  on the board yet. Other unsolicited reports are ignored.
- Plugging in and out is noticed through a udev watch (`hidraw` add events of `38ee:0016`; remove
  events of the vendor node in use, matched by devpath), tested against the fake keyboard only.
  Replugging was seen to work on the board; the unplug half isn't checked on the board yet. Without
  udev, unplugging is noticed on the next read and "not found" is retried every 3 s; with it, every
  10 s as a fallback.
- Firmware version byte order (`82 02` = `08 01` → v1.08, low byte first) rests on one sample.
- Colour picker (Lighting): checked offscreen only (tests, EN/FR screenshots, dark and light), not
  used by hand on the desktop yet. A colour is sent when the field or hue strip is let go (or 300 ms
  after the last arrow key), so the board doesn't follow the drag; the drawing does. After a send,
  the picker keeps the chosen colour until the read-back changes it: a write that fails without an
  error would leave it showing that colour. When the page is too narrow for it beside the option
  rows, it goes under them, and its swatches wrap to the width left.
  Saved colours live in `~/.config/magnetar/settings.ini`, shared by every profile.
- Text size: follows GNOME's `text-scaling-factor` (what `omarchy display text size` sets) times the
  interface font Qt gets from its gtk3 platform theme. It updates live, no restart (checked on the
  desktop). Other desktops without that key (KDE) get Qt's own font size, unscaled.
- UI redesign (`docs/ui-plan.md`, items 1–18): checked offscreen (tests, EN/FR screenshots in
  a dark and the flexoki-light theme), reviewed on the desktop (findings R1–R8 in
  `docs/ui-plan.md`, all fixed), then used with the board
  (2026-11-25): mouse and keyboard navigation work on every page, a second light theme looks
  right, and the fullscreen layout is fine. Space toggles the lights only while no control has keyboard focus.
- Fitting the window: on the desktop, fullscreen fits. At the default size (1440 × 920 layout px,
  1234 × 789 at 12 px), at half a 1080p screen (960 × 1030) and at a third (640 × 1030) every page
  fits (tests, EN and FR); the narrow reflow (bars under the keyboards wrap, colour picker under
  the options, Advanced key list under the keyboard below ~640 u, Multi-Point depths under their
  action, Travel's "Separate" switch under the Rapid Trigger title and shorter Travel sliders,
  Macros list above the editor) is checked offscreen only, not yet on the desktop. Pages never
  scroll, even below the minimum size. At a third of the screen the Travel sliders are at their
  shortest (100 u, about 0.05 mm per pixel over Rapid Trigger's 3.5 mm): type exact values there.
- Short windows (1028 × 574, a tiled window on a 1440p screen at 125 %): Keys (every Advanced
  type), Macros and Mural fit; Lighting (its Colour section), Settings and Calibration run off the
  bottom, and the test skips them at that size.

## Firmware behaviour
- Two concurrent HID sessions wedge the keyboard: it stops sending keystrokes until a clean command
  arrives. Any tool must serialise access; the web app and our code can't run together.
- Each effect stores its own colour and speed; `04 02` loads them (it doesn't reset speed).
- Per-key colours (`06 00`): writes go in frames of at most 8 keys, the most ever captured in one
  frame (11 would fit). Verified on the board under Custom; a write made while Breathing ran
  stuck (2026-11-25).

## Repo / docs
- Capture logs are not published, so the evidence behind `akko-protocol.md` can't be checked
  from the public repo alone.
- The DKS and advanced-key decoding relied on screenshots of the web UI that are not in the repo.

## Legal
- The vendor web app's terms of use were not found, so it's unknown whether they forbid reverse
  engineering. In the EU, studying a program's behaviour (Directive 2009/24/EC, art. 5(3)) can't be
  taken away by contract (art. 8), and the project only observes USB traffic. This is not legal
  advice.
