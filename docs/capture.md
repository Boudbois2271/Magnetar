# Capture workflow

New knowledge comes from capturing what the vendor web app sends:

1. Open the vendor configurator in a Chromium-based browser (WebHID), connect the keyboard.
2. Paste `akko-capture.js` into the DevTools console.
3. Before **each** UI action, call `akko.mark('what I am about to do')`. One action per mark. Write
   down in a mark what you saw on the keyboard afterwards (marks are free text and often carry the
   only description of an effect).
4. When done, `akko.save('akko-captureN')` downloads the log. Move it into the repo folder as a
   new file (it stays untracked). Never append to or edit an old capture. Cite captures by
   filename and mark in `akko-protocol.md`, since readers of the public repo won't have them.

When asking for a capture, always hand over ready-to-paste commands: the
`flock "$XDG_RUNTIME_DIR/akkoctl.lock" sleep infinity` line first, then the full numbered list of
`akko.mark('…')` calls, then `akko.save('akko-captureN')`. Each mark is a plain instruction using the
buttons' exact labels (check `screenshots/` first; if a page isn't in there, ask what it shows).
Ask for an observation only when the capture can't show it, as a precise question in its **own mark
right after the action** (marks are pasted before the action, so a mark can't report its own result):
e.g. `akko.mark('result: did F12 light up? yes/no: ___')`.

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
