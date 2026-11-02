# Limitations and open questions

Things touched but not finished, known issues, and doubts. Remove an entry once it is solved and
reflected in `akko-protocol.md`.

## Protocol gaps

### Not captured at all
- **Performance tab**: global/single-key actuation, rapid trigger (on/off, press, release,
  continuous), top/bottom dead zones. Only the read tables `93 00`, `95 00`, `96 00`, `99 00` are
  known, and their meaning is guessed. No write command known.
- **Macros**: `9a 00/01/02` read back 56 × `ff` (empty). Recording, assigning and deleting are
  not captured.
- **Settings**: polling rate, sleep, debounce, Win lock, WASD swap. The `82 xx` read blocks are
  probably these, but nothing is mapped.
- **Device/about page**: firmware version. `82 01` (`11 00 00 00 00 59`) and `82 02` (`08 01`) are
  guesses.
- **Key test / live travel view**: no capture.

### Captured but not decoded
- **Mural (`08 04`)**: 2-packet header (`00 00 53` + 83 LED indices), then 5-packet frames (254 B)
  re-sent about once per second, **not echoed** by the firmware. Encoding is not raw RGB. Needs a
  capture of one trivial frame (whole board one colour).
- **DKS stage value for "hold across stages"**: only `0a` (fire once) was captured.
- **SOCD mode `02`**: probably "first input", never captured.
- **Layer 02** of the keymap (`83 02`): it reads as a copy of the base layer. Mac layer or Fn2?
- **System (`07`) and lighting (`08`) actions** in the keymap: values are guessed from defaults,
  never captured as a write.
- Mouse buttons other than left (`01 bb 01 00`, bb = 02/04 guessed).
- `84 11` (returns `01`), `85 00`, `87 00`, `a6 ff`: unknown.

### Units
- Travel is 0.005 mm per unit, consistent across three multipoint values and one DKS value. The
  `93 00` default of 200 → 1.00 mm is consistent with that, but unconfirmed against the UI.

## Existing tooling (`akkoctl`, widget)

- No install script: the repo copies must be copied by hand to `~/.local/bin/` and
  `~/.config/omarchy/plugins/`, and the two can drift apart.
- The widget hardcodes `~/.local/bin/akkoctl` and depends on Omarchy's own QML modules
  (`qs.Commons`, `qs.Ui`), so it only runs inside Omarchy's shell.
- The widget shows the colour picker on profile 1 only.
- `profile_count()` reads `82 0F` payload[18], which is always 0, so it always falls back to 3. The
  count is very likely payload[3].
- Only builds single-packet frames (`pkt_count = 1`, `pkt_index = 0`, `arg = 0`): no multi-packet
  reads/writes, no advanced-key `arg`.
- `transact()` matches responses on `cmd`/`sub` only, not `arg` or packet index.
- Only covers profile and global lighting (`10/90 00`, `04/84 01`).

## Firmware behaviour
- Two concurrent HID sessions wedge the keyboard: it stops sending keystrokes until a clean command
  arrives. Any tool must serialise access; the web app and our code can't run together.
- Changing effect with `04 02` resets speed to 2.

## Repo / docs
- The full desktop app hasn't been started; language, UI toolkit and packaging are undecided.
- Capture logs are not published, so the evidence behind `akko-protocol.md` can't be checked
  from the public repo alone.
- The DKS decoding relied on a screenshot of the web UI that is not in the repo.
- No README yet (it should say the project is unofficial and not affiliated with Akko).

## Legal
- The vendor web app's terms of use were not found, so it's unknown whether they forbid reverse
  engineering. In the EU, studying a program's behaviour (Directive 2009/24/EC, art. 5(3)) can't be
  taken away by contract (art. 8), and the project only observes USB traffic. This is not legal
  advice.
