# CLAUDE.md

## Project

**Magnetar**: an open-source, Linux-first configuration tool for the **Akko TAC75 HE** magnetic (Hall effect)
keyboard (USB `38ee:0016`, firmware by BY Tech). The vendor only ships a WebHID configurator
(`demo.hubx.pro`). This repo reverse engineers its HID protocol from captured traffic, so we can build
a native app that does what the web driver does: profiles, lighting, keymap, advanced keys,
per-key travel/rapid trigger, macros, settings.

Current phase: protocol reverse engineering is done (a few gaps remain), and the desktop app is
starting. Stack: **C++20 and Qt Quick, built with qmake6**, the way Omarchy's own apps are built.
One protocol library (`core/`) is shared by the CLI and the app.

What exists today: the core library; `akkoctl`, a CLI on top of it covering profile switching and
global lighting (effect, colour, brightness); an Omarchy bar widget that shells out to `akkoctl`;
and **Magnetar**, the desktop app (sidebar of pages: Keys, with Action / Travel / Advanced / Colour tabs on one shared selection, Macros, Lighting, Settings, Calibration and Device). Next page: Mural.

Names: the project, repo and desktop app are **Magnetar**. `akkoctl` is the CLI inside it, and keeps
that name everywhere it appears (command, `akkoctl.lock`, `~/.local/state/akkoctl/`, log).

The repo publishes **results** (tools, decoded protocol, docs), not the raw material used to get
there: capture logs stay local and are gitignored.

## Files

| Path | What it is |
|---|---|
| `README.md` | Public overview: unofficial disclaimer, install, usage. Keep it in sync when tools change. |
| `core/` | Protocol library, plain C++20 + POSIX, **no Qt**: frames and checksum, session lock, hidraw discovery and transport, KNOWN commands. Everything that talks to the keyboard goes through it. |
| `cli/` | `akkoctl` CLI on top of `core/`, no Qt. Prints the keyboard state as one JSON line after every command. Installed as `~/.local/bin/akkoctl`. |
| `tests/` | Qt Test suite, no keyboard needed (fake device, temp sysfs). `fixtures/` holds frames and CLI runs recorded from the original Python tool (removed; it is in git history) and frames from captures: the reference the C++ code must match. |
| `app/` | Magnetar (Qt Quick). `src/backend` = state/actions for QML; `src/device_worker` = keyboard work off the UI thread, lock taken per batch only; QML: `Main.qml` (sidebar), one `*Page.qml` per page, `MacrosPage.qml` records and edits macros; `CalibrationPage.qml` runs a calibration session; `KeysPage.qml` hosts `ActionTab`/`TravelTab`/`AdvancedTab`/`ColorTab` around a shared `KeyboardView`; `src/scancodes` = evdev → HID usages for macro recording; `src/theme`, `src/translator` from the Omarchy app templates (MIT notice kept); `src/i18n/fr.json` = French. |
| `pkgbuild/` | Arch package for Magnetar: `PKGBUILD`, `.desktop` entry, icon. |
| `bin/` | `build` (qmake6 + make into `build/`, gitignored), `test`, `install` (CLI to `~/.local/bin`), `package` (Magnetar via `makepkg -fsi`; needs sudo, so Ilan runs it). |
| `omarchy-widget/boudbois.akko/` | Omarchy (Quickshell/QML) bar widget: profile picker, lights toggle, brightness, colour. Shells out to `~/.local/bin/akkoctl`, one process at a time. |
| `udev/70-akko-tac75-he.rules` | Grants the logged-in user access to the keyboard's hidraw nodes. Goes in `/etc/udev/rules.d/`. |
| `akko-protocol.md` | **Source of truth** for everything decoded. Update it whenever a capture teaches us something. |
| `akko-capture.js` | WebHID logger pasted into the browser DevTools console on the vendor configurator. Read-only: it wraps `sendReport`/`sendFeatureReport`/`receiveFeatureReport` and `inputreport` events, then calls the originals unchanged. |
| `akko-capture*.txt` | Raw capture logs, one per session. **Gitignored, local only.** Immutable once written. |
| `screenshots/` | Screenshots of the vendor app's pages, named `date-NN-page.png`. **Gitignored** (vendor UI). |
| `limitations.md` | Known gaps, unsolved questions, half-finished topics. Read it before starting work, update it before finishing. |
| `LICENSE` | GPL-3.0. Every source file carries `SPDX-License-Identifier: GPL-3.0-or-later`. |
| `CLAUDE.local.md` | Private, gitignored session state and handoff notes (may not exist on your machine). |

The repo copies are the source of truth; the installed copies don't update themselves. After
editing, run `bin/test`, then `bin/install` for the CLI; the widget is still copied by hand.

## Clean-room rules (legal)

Protocol knowledge comes **only** from observing traffic between the vendor web app and a keyboard
we own. That is studying how the device behaves, which is what keeps this project on safe legal
ground. To stay there:

- Never decompile, deobfuscate or copy the vendor's web app JavaScript, firmware or assets, and
  never paste vendor code into this repo.
- Don't publish firmware images or anything downloaded from the vendor.
- "Akko" and the product name are only used to say what hardware this works with. No logos, and
  never present the project as official or affiliated.

## Protocol in one paragraph

Vendor interface = HID interface 2, usage page `0xFF60`, report ID `9`. Write `[0x09] + 63 bytes`,
read 64 back. Frame: `cmd, sub, arg, pkt_count, pkt_index, len, payload[len]…`, checksum at byte 62
such that `(0x09 + sum(frame[0:62]) + checksum) & 0xFF == 0xFF`. `cmd | 0x80` = read; writes are
echoed back (except the Mural `08 04` stream). Multi-byte values are big-endian. Details, key IDs,
action encoding and the command tables are in `akko-protocol.md`.

## Capture workflow

New knowledge comes from capturing what the vendor web app sends, one `akko.mark()` per UI action.
Full procedure, how to ask for a capture (ready-to-paste commands), log format and one-liners:
**`docs/capture.md`**, read it before asking for or decoding a capture. Never edit an old capture;
cite captures by filename and mark in `akko-protocol.md`.

## Status labels (used in `akko-protocol.md`)

- **KNOWN**: sent by our own code and verified on the board.
- **SEEN**: write captured with a clear before/after, never sent by us.
- **DECODED**: read-only structure, understood.
- **GUESS**: plausible, needs a capture.

Promote a label only with evidence, and say which capture/mark it came from.

## Talking to the hardware

The keyboard under test is a daily-use keyboard. Rules:

- **One session at a time.** Overlapping sessions make the firmware stop sending keystrokes until
  the next clean command. Any tool must hold an exclusive lock (akkoctl uses `flock`), and the web
  configurator must be closed while our code runs.
- Only send writes that are **SEEN** or **KNOWN**. Never probe unknown `cmd`/`sub` writes; reads
  (`cmd | 0x80`) are generally safe.
- Read the current value first, and be able to restore it. For advanced keys the web app always sends
  a type `00` clear before saving: do the same.
- Ask before running anything that writes to the device, and say exactly what it will change.
- Linux access needs a udev rule granting the seat user the hidraw node, e.g.
  `SUBSYSTEM=="hidraw", ATTRS{idVendor}=="38ee", ATTRS{idProduct}=="0016", TAG+="uaccess"`.
  Find the right node by matching `38EE:0016` in `uevent` and `06 60 ff` in `report_descriptor`.

## Conventions

- Hex lowercase, space-separated bytes; name commands as `cmd sub` (e.g. `04 01`, `92 00`).
- "payload[n]" offsets count from the first payload byte (frame byte 6); "frame[n]" from `cmd`.
- Keep docs in English, terse, tables over prose.
- New source files start with an `SPDX-License-Identifier: GPL-3.0-or-later` comment.
- `core/` and `cli/` stay Qt-free (the CLI has no runtime dependencies); Qt is for tests and the app.
- `akkoctl` keeps its commands and JSON output stable, byte for byte: the widget parses it. The
  `tests/fixtures/python-cli.jsonl` replay guards this.
- Protocol changes come with a test against a fake device before anything is sent to the board.
- App: every visible string is `qsTr()` with a French entry in `app/src/i18n/fr.json` (a test fails
  otherwise); the app shows only values read back from the board; pages appear when they work,
  never as placeholders. Check layout with offscreen screenshots in English and French.
- App: the user knows what they're doing. Never grey out, warn about or ask to confirm a choice
  that merely looks odd; block only what can't work on the board.
- Nothing personal in tracked files: no home paths, usernames, machine names. That goes in
  `CLAUDE.local.md`.

## End of session

1. Update `akko-protocol.md` with anything newly decoded (and its status).
2. Move unfinished or doubtful items to `limitations.md`.
3. Write the handoff (where we are, what's next) in `CLAUDE.local.md`.
