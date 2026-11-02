# CLAUDE.md

## Project

An open-source, Linux-first configuration tool for the **Akko TAC75 HE** magnetic (Hall effect)
keyboard (USB `38ee:0016`, firmware by BY Tech). The vendor only ships a WebHID configurator
(`demo.hubx.pro`). This repo reverse engineers its HID protocol from captured traffic, so we can build
a native app that does what the web driver does: profiles, lighting, keymap, advanced keys,
per-key travel/rapid trigger, macros, settings.

Current phase: **protocol reverse engineering**. The full desktop app hasn't been started and its
stack/UI is not chosen. Don't scaffold one unless asked.

What exists today: `akkoctl`, a small CLI covering profile switching and global lighting (effect,
colour, brightness), and an Omarchy bar widget built on top of it. `akkoctl` is the reference
implementation for the transport layer.

The repo publishes **results** (tools, decoded protocol, docs), not the raw material used to get
there: capture logs stay local and are gitignored.

## Files

| Path | What it is |
|---|---|
| `akkoctl/akkoctl` | Python 3 CLI, stdlib only, talks to `/dev/hidraw*` directly. Prints the keyboard state as JSON after every command. Installed as `~/.local/bin/akkoctl`. |
| `omarchy-widget/boudbois.akko/` | Omarchy (Quickshell/QML) bar widget: profile picker, lights toggle, brightness, colour. Shells out to `~/.local/bin/akkoctl`, one process at a time. |
| `udev/70-akko-tac75-he.rules` | Grants the logged-in user access to the keyboard's hidraw nodes. Goes in `/etc/udev/rules.d/`. |
| `akko-protocol.md` | **Source of truth** for everything decoded. Update it whenever a capture teaches us something. |
| `akko-capture.js` | WebHID logger pasted into the browser DevTools console on the vendor configurator. Read-only: it wraps `sendReport`/`sendFeatureReport`/`receiveFeatureReport` and `inputreport` events, then calls the originals unchanged. |
| `akko-capture*.txt` | Raw capture logs, one per session. **Gitignored, local only.** Immutable once written. |
| `limitations.md` | Known gaps, unsolved questions, half-finished topics. Read it before starting work, update it before finishing. |
| `LICENSE` | GPL-3.0. Every source file carries `SPDX-License-Identifier: GPL-3.0-or-later`. |
| `CLAUDE.local.md` | Private, gitignored session state and handoff notes (may not exist on your machine). |

The repo copies of `akkoctl` and the widget are the source of truth; reinstall them after editing
(the installed copies don't update themselves).

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

New knowledge comes from capturing what the vendor web app sends:

1. Open the vendor configurator in a Chromium-based browser (WebHID), connect the keyboard.
2. Paste `akko-capture.js` into the DevTools console.
3. Before **each** UI action, call `akko.mark('what I am about to do')`. One action per mark. Write
   down in a mark what you saw on the keyboard afterwards (marks are free text and often carry the
   only description of an effect).
4. When done, `akko.save('akko-captureN')` downloads the log. Move it into the repo folder as a
   new file (it stays untracked). Never append to or edit an old capture. Cite captures by
   filename and mark in `akko-protocol.md`, since readers of the public repo won't have them.

Log line format: `t_ms<TAB>DIR<TAB>vid:pid<TAB>id=N<TAB>hex bytes`, where DIR is `OUT`, `IN`,
`FEAT_SET`, `FEAT_GET`, and `### label` lines are the marks.

Useful one-liners:

```sh
grep -n '^###' akko-capture2.txt                                  # list the marks
awk -F'\t' '$2=="OUT"{split($5,b," ");print b[1],b[2]}' akko-capture2.txt | sort | uniq -c | sort -rn  # command histogram
sed -n '/^### setting A/,/^###/p' akko-capture2.txt               # one marked section
```

When decoding, diff the frame before/after a single change, then cross-check against a second,
independent change. Ask the user for a targeted capture rather than guessing; list what's needed
under "Still to capture" in `akko-protocol.md`.

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
- `akkoctl` stays stdlib-only Python and keeps its JSON output stable: the widget parses it.
- Nothing personal in tracked files: no home paths, usernames, machine names. That goes in
  `CLAUDE.local.md`.

## End of session

1. Update `akko-protocol.md` with anything newly decoded (and its status).
2. Move unfinished or doubtful items to `limitations.md`.
3. Write the handoff (where we are, what's next) in `CLAUDE.local.md`.
