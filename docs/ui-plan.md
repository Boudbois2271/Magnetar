# UI plan: from "works" to "respectable app"

Audit of the app at b0ddf24, from offscreen screenshots at the default window (1234×789, 12 px
text, English and French, the live Omarchy theme). Each numbered item is one commit, checked with
offscreen screenshots in both languages and `bin/test`.

## Status

Done on `main` (one commit per item), except:

| Item | State |
|---|---|
| 6 | Done without the 100 ms selection fade (it would make tests that read colours racy). |
| 10 | Skipped by Ilan: no empty-state or instructional sentences anywhere. |
| 14 | Done without the empty-list sentence, for the same reason. |

Checked offscreen only (tests, EN/FR screenshots in a dark and a light theme), not yet on the
desktop or the board.

## What holds it back today

| # | Problem | Where |
|---|---|---|
| a | The keyboard, the one thing this app is about, is drawn small (keys ≈ 28 px) in a corner; 40 % of every page is empty below. | Keys, all tabs |
| b | Every group is the same filled box with the same small grey bold title: nothing leads, nothing recedes. | every page |
| c | Four button styles: square chips, Material pills, plain text that doesn't look clickable ("Calibrate", "Switch to Custom", "Start calibration"), one accent block ("+ New macro"). Pills clash with Hyprland rounding 0, which makes everything else square. | every page |
| d | Mixed heights: fields and combos 44 px, chips 34 px, side by side. | Macros, Travel, Settings |
| e | No type scale: sizes 11, 12, 13, 14, 22, 26; secondary text at 45 % opacity (fails contrast). | sidebar footer, card titles |
| f | Travel shows a wall of identical "0.60": differences between keys are invisible. | Keys → Travel |
| g | Colour tab with Custom off draws every key black: looks broken. | Keys → Colour |
| h | Empty panels with no words: "Selected key" with nothing selected, "Keys with an advanced function" with none. | Keys |
| i | "3 keys selected" said twice (toolbar and panel); Undo floats alone above the panel. | Keys |
| j | Layer switch is a second tab bar with its own dark background, unlike the page tabs. | Keys → Action |
| k | Macro editor is shown even with no macro; one wide column with a 900 px name field. | Macros |
| l | Calibration instructions are one 150-character-wide paragraph. | Calibration |
| m | No icons at all; sidebar selection is a grey block; no connection status outside Settings. | sidebar |

## Direction

The app is an instrument panel for one keyboard. **The keyboard drawing is the hero**: big, drawn
like real keycaps, and it carries the data (depth, function, colour) on the keys themselves.
Everything around it is quiet, flat and dense, native to Omarchy: colours and corners from the
theme, no shadows, no gradients, no boxes around everything.

| Token | Choice |
|---|---|
| Colour | Theme roles only. Background = page; `sidebar` = rail; **plate** (new, `shade .05`) = the keyboard case; **cap top** / **cap skirt** (two shades) = keys; accent = selection and primary action only; `error` for destructive. Secondary text = foreground mixed 65 % into background (not opacity). |
| Type | The desktop UI font, one family. Four sizes from `ui`: title 22 semibold, section 15 semibold, body 14, small 12. Sentence case, no caps labels. Tabular figures for every number (mm, %, bytes). |
| Shape | One radius: `theme.radius` (Hyprland's), for buttons, fields, caps, plate. One control height: 34 u. |
| Layout | Left-aligned. Page = header row (title, tabs, page actions right) → keyboard plate full width → controls below in columns. Groups separated by space and a section title, not boxes. |
| Motion | One: the selection highlight fades in 100 ms. Nothing else moves. |

Generic-default check: a stock plan here would be "cards with soft shadows, stat tiles for device
info, caps section labels, icons on every row". Dropped all four: boxes go, device info stays a
key/value list, labels are sentence case, icons only where they replace words people scan
(sidebar, back, undo, record, delete).

## Planned changes, in order

### Foundation
1. **Type scale and secondary colour** in `ui`/`theme`; replace every ad-hoc size and opacity.
2. **One button family**: primary (accent fill), secondary (chip fill), quiet (text, for Back).
   Same height and radius as chips. Disabled = 40 % opacity. Text buttons that look like labels
   become real buttons.
3. **One control height** for combos, text fields and mm fields (34 u).
4. **Sections instead of cards**: `Card` becomes a titled section with no fill. A filled surface
   stays only for objects: the keyboard plate, the macro list, popups.
5. **Small icon set** (~12 SVGs from Lucide, ISC licence, tinted by theme) in the qrc.

### Keyboard (the hero)
6. **Keycap drawing**: plate behind the keys, caps with a lighter top face on a darker skirt
   (flat, two shades), legend top-left like real caps. Same component on Keys, Calibration,
   Lighting.
7. **Keys page layout**: header (title + tabs + Undo/Restore at right) → keyboard full width
   (keys ≈ 55–60 px at the default window) → selection bar attached under the plate (All, Invert,
   None, "3 keys selected" once) → the tab's controls in 2–3 columns below. Replaces today's
   drawing-beside-panel layout.
8. **Data on the keys**: Travel = a depth bar inside each cap plus the value, so a 0.3 mm key
   stands out from 2.0 mm ones; Advanced = short function name on the cap; Colour = the cap
   legend lit in the LED colour, normal caps plus one line "Custom effect is off" when it is.
9. **Layer switch** as a segmented control (same as the profile picker, item 10).
10. ~~**Empty states**~~ (skipped): nothing selected → "Click a key, or drag across several."; no advanced
    keys → "None yet."

### Shell
11. **Sidebar**: icon + label per page, selected page = accent text and a 3 px accent bar;
    profile picker = segmented "1 2 3" (one click instead of a menu); under the model name a
    status line (dot + "Connected" / "Not found"); footer = a "Shortcuts" button.
12. **Shortcut overlay**: keys drawn as small caps matching the keyboard, not teal monospace.

### Pages
13. **Lighting**: a keyboard preview at the top showing colour and brightness; brightness slider
    without 20 tick dots; colour presets and the preview swatch the same shape.
14. **Macros**: list on the left (scrolls in its box, storage bar under it), editor on the right,
    shown only for a selected or new macro; empty list → "No macros yet. Record one to play a key
    sequence from a single key."
15. **Settings**: "Calibrate" as a real button; About split from Device info.
16. **Calibration**: the instructions as three numbered steps (it is a real sequence: start,
    press each key fully, finish), lines under ~70 characters; primary "Start calibration".

### Quality floor
17. Visible keyboard focus on every control (keys, sliders, segments); sensible tab order.
18. Screenshots in one light Omarchy theme as well as the current dark one.

## Decided

- Keys layout: keyboard on top, full width, controls below (item 7).
- Profile picker: "1 2 3" buttons, no dropdown (item 11).
- The "not affiliated" disclaimer leaves the sidebar; it stays in Settings → About and the README.
- Icons in the sidebar and on a few buttons (item 5).

## Ilan's desktop review (2026-11-23), to fix

Reviewed on the desktop after items 1–18. Not fixed yet; one commit per item.

| # | Where | Feedback |
|---|---|---|
| R1 | Keys | Clicking a selected key again must deselect it (today it stays selected). **Fixed**: a plain click on the only selected key clears it; on one of several it keeps only that key. |
| R2 | Keys → Action | The controls sit at the bottom of the page instead of right under the keyboard like the other tabs, so they jump when something needs more room. Anchor them under the selection bar. |
| R3 | Lighting | Rework the layout: keyboard the same size as on Keys, all options in one column under it (no split). Rows, in order: **Lights** (title with the switch on it; drop the duplicate "Lights on" row) · **Brightness** (slider + level/max) · **Effect** (dropdown; drop the duplicate "Effect" title) · **Speed** (slider + level/max) · **Color** (swatches on the same line) · **Hex color** (text field) · **Rainbow** (switch). Tight, even vertical spacing (today's gaps look random, Lights on→Brightness and Effect→Speed are huge). |
| R3a | Lighting | Brightness offers only levels **0, 1, 5, 10, 15, 20** (the 20 steps are barely distinguishable). Brightness and Speed sliders short and the same length (Speed has 5 positions). |
| R3b | UI text | Spelling **"color"**, not "colour" ("Hex color", "Color"). Check every visible string. |
| R4 | Sidebar | Page order: Keys, Lighting, Macros, Settings. |
| R5 | Macros | While recording, new actions go at the top of the list; older ones scroll away below (today new ones disappear below and need scrolling). |
| R6 | Settings | Same random, too-large vertical spacing. Switches, dropdown and button sit half a screen from their labels: keep them next to the label. Device and About go under the options. |
| R7 | Sidebar | Replace the Mac ⌘ icon (`command.svg`) on the Shortcuts button with a shortcut/keyboard-style icon. |
| R8 | Keys → Colour | Per-key colour moves to the Lighting page (select keys on its keyboard). Keep the "Custom effect is off" line with its switch-to-custom button, shown when keys are selected and the effect isn't custom. Keys keeps Action, Travel, Advanced. |
