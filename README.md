# Magnetar

Open-source tools to configure the **Akko TAC75 HE** magnetic keyboard (USB `38ee:0016`) on Linux,
without the vendor's web configurator: **Magnetar**, a desktop app, and `akkoctl`, a command-line
tool for scripts and status bars.

> **Unofficial project.** It is not affiliated with, endorsed by or supported by Akko or the
> keyboard's firmware vendor. "Akko" and "TAC75 HE" are used only to name the hardware this works
> with. The protocol was worked out by observing USB traffic between the official web app and a
> keyboard we own; no vendor code or firmware is included. Use at your own risk: writing to the
> keyboard changes its onboard settings.

## What's here

| Path | What it does |
|---|---|
| `app/` | **Magnetar**, the desktop app (see below). English and French. |
| `cli/` | `akkoctl`, the command-line tool: switch onboard profile, set lighting colour and brightness, turn lighting on/off. Prints the keyboard state as JSON. |
| `core/` | The C++ library that talks to the keyboard, shared by the CLI and Magnetar. |
| `tests/` | Automated tests (no keyboard needed). |
| `omarchy-widget/boudbois.akko/` | Bar widget for [Omarchy](https://omarchy.org) built on `akkoctl`. |
| `udev/70-akko-tac75-he.rules` | udev rule giving your user access to the keyboard. |
| `akko-protocol.md` | The decoded HID protocol, with how confident we are in each part. |
| `akko-capture.js` | Browser console logger used to record the official web app's traffic. |
| `limitations.md` | What isn't decoded or supported yet. |
| `docs/` | Longer references, e.g. `docs/capture.md` (how traffic is recorded). |

## Magnetar pages

| Page | What you can do |
|---|---|
| **Keys** | Click keys on the drawing (or drag a box, Ctrl+drag to add) and edit them together in four tabs: **Action** (what the key sends, on all three layers), **Travel** (actuation point, Rapid Trigger, dead zones), **Advanced** (Mod-Tap, Toggle, Release, Multi-point, Dynamic keystroke, SOCD) and **Colour** (per-key colours for the Custom lighting effect). |
| **Macros** | Record key presses and releases with their timing, edit the steps and delays (or use the standard 50 ms), save them to the keyboard's 4096-byte storage. Macros are per profile; bind one to a key on Keys → Action → Macros, played a number of times, until a key is pressed, or while held. |
| **Lighting** | Lights on/off, brightness, effect, speed, colour or rainbow. |
| **Settings** | Win Key Lock, Combo Optimization, Adaptive Calibration, polling rate (125–8000 Hz; the keyboard restarts for about a second when it changes). |
| **Calibration** | Key calibration: press keys all the way down and watch each one fill in, then Finish to save. The keys don't type during a session. |
| **Device** | Model, firmware, connection status. |

The onboard profile (1–3) is picked at the top of the sidebar, or with Ctrl+1/2/3.

Every page planned so far is in; Magnetar grows one page at a time.

## Install

Build requirements: Linux, a C++20 compiler, `make` and Qt 6 (on Arch:
`base-devel qt6-base qt6-declarative`). The installed `akkoctl` itself has no dependencies.

```sh
# Let your user talk to the keyboard
sudo cp udev/70-akko-tac75-he.rules /etc/udev/rules.d/
sudo udevadm control --reload && sudo udevadm trigger   # or replug the keyboard

# CLI: builds, then installs to ~/.local/bin/akkoctl
bin/install

# Magnetar, as an Arch package (shows up in the app launcher; asks for your password)
bin/package

# Tests (optional, no keyboard needed)
bin/test

# Omarchy widget (optional)
mkdir -p ~/.config/omarchy/plugins
cp -r omarchy-widget/boudbois.akko ~/.config/omarchy/plugins/
```

The widget expects `akkoctl` at `~/.local/bin/akkoctl`.

## Usage

```sh
akkoctl                      # print state as JSON (same as `akkoctl status`)
akkoctl profile 2            # switch to onboard profile 2
akkoctl brightness 10        # 0-20
akkoctl color '#ff6a00'      # fixed colour, or: akkoctl color rainbow
akkoctl toggle               # also: on, off (off remembers the last brightness)
```

Close the vendor web app while using `akkoctl`: if two programs talk to the keyboard at the same
time, the firmware stops sending keystrokes until the next clean command (`akkoctl status` or a
replug fixes it). `akkoctl` locks the device so two copies of itself can't collide.

## Contributing

Protocol gaps are listed in `limitations.md` and under "Still to capture" in `akko-protocol.md`.
To help, record the official web app's traffic with `akko-capture.js` (instructions in
`docs/capture.md`) while changing one setting at a time. Please don't decompile or
copy the vendor's code or firmware: only observed traffic.

## License

[GPL-3.0-or-later](LICENSE).
