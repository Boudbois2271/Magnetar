# Limitations and open questions

Things touched but not finished, known issues, and doubts. Remove an entry once it is solved and
reflected in `akko-protocol.md`.

## Protocol gaps

### Partly decoded
- **Macros** (`akko-capture7.txt`): blob, actions and binding decoded (`akko-capture7.txt`, `7b`).
  Unknown: the directory's byte order above 256 B, what a key
  does if its macro index points past the last macro (the app prevents it), and what `9a 00/01/02` is.

### Not captured at all
- **Polling rate** values for 125/250/2000/4000 Hz are guessed; only 500, 1000 and 8000 were captured.
- **Combo Optimization**: the toggle is decoded, what it does on the board is not known.
- **Adaptive Calibration vs bottom drift**: not tested (would need calibration read-backs with it off).
- `82 xx` read blocks and `82 01` (`11 00 00 00 00 59`) are unknown; they are not Base Functions.

### Captured but not decoded
- **Mural (`08 04`)**: 2-packet header (`00 00 53` + 83 LED indices), then 5-packet frames (254 B)
  re-sent about once per second, **not echoed** by the firmware. Encoding is not raw RGB. Needs a
  capture of one trivial frame (whole board one colour).
- **DKS stage value for "hold across stages"**: only `0a` (fire once) was captured.
- **SOCD mode `02`**: probably "first input", never captured.
- **System (`07`) and lighting (`08`) actions** in the keymap: values are guessed from defaults,
  never captured as a write.
- **Key calibration**: Finish (`94 04`) is understood, but the stored bottom of F12 after a full
  press (2225) is not exactly the lowest `94 02` min seen (2204). The bit-15 flag in `94 02` is a
  guess ("reached bottom"). Bottom drift between sessions is guessed to be Adaptive Calibration.
  Calibration mode stops the keys typing.
- **Switch models** `41`, `42`, `44` are guessed from the UI order; only `43` and `45` were captured.
- `84 11` (`01`), `84 13` (`00 b4`), `84 1d`/`84 1e` (`00`), `84 20` (`01`), `85 00`, `87 00`, `a6 ff`: unknown.

### Units
- Travel is 0.005 mm per unit, confirmed against the UI by actuation, RT and dead-zone writes
  (`akko-capture3.txt`).
- The UI showed 0.6 mm on every key, but `akko-capture2.txt` read actuation 200 (1.00 mm). No `93 00`
  read happened before the first write in `akko-capture3.txt`, so the true starting value is unknown.
- Actuation/RT/dead-zone writes are SEEN only: akkoctl doesn't send them yet. Trailing `00` byte in
  `13`/`19` entries: meaning unknown.

## Existing tooling (`akkoctl`, widget)

- No install script: the repo copies must be copied by hand to `~/.local/bin/` and
  `~/.config/omarchy/plugins/`, and the two can drift apart.
- The widget hardcodes `~/.local/bin/akkoctl` and depends on Omarchy's own QML modules
  (`qs.Commons`, `qs.Ui`), so it only runs inside Omarchy's shell.
- The widget shows the colour picker on profile 1 only.
- `profile_count()` reads `82 0F` payload[3]. Only one sample (value 3) backs that offset; a value
  outside 1–8 falls back to 3.
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

## Legal
- The vendor web app's terms of use were not found, so it's unknown whether they forbid reverse
  engineering. In the EU, studying a program's behaviour (Directive 2009/24/EC, art. 5(3)) can't be
  taken away by contract (art. 8), and the project only observes USB traffic. This is not legal
  advice.
