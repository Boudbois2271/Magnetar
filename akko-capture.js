// SPDX-License-Identifier: GPL-3.0-or-later
// Akko WebHID traffic logger — paste into DevTools console on demo.hubx.pro.
// Read-only: it wraps the WebHID calls to record bytes, then calls the originals unchanged.
(() => {
  if (window.akko) { console.log("akko logger already installed"); return; }
  const log = [];
  const hex = (buf) => Array.from(buf instanceof DataView ? new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength)
                                    : ArrayBuffer.isView(buf) ? new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength)
                                    : new Uint8Array(buf)).map(b => b.toString(16).padStart(2, "0")).join(" ");
  const tag = (d) => `${d.vendorId.toString(16)}:${d.productId.toString(16)}`;
  const push = (dir, dev, id, data) => log.push({ t: Math.round(performance.now()), dir, dev: tag(dev), id, data });

  const P = HIDDevice.prototype;
  for (const [name, dir] of [["sendReport", "OUT"], ["sendFeatureReport", "FEAT_SET"]]) {
    const orig = P[name];
    P[name] = function (id, data) { push(dir, this, id, hex(data)); return orig.call(this, id, data); };
  }
  const origGet = P.receiveFeatureReport;
  P.receiveFeatureReport = async function (id) {
    const r = await origGet.call(this, id); push("FEAT_GET", this, id, hex(r)); return r;
  };
  const seen = new WeakSet();
  const watch = (d) => { if (seen.has(d)) return; seen.add(d);
    d.addEventListener("inputreport", (e) => push("IN", e.device, e.reportId, hex(e.data))); };
  navigator.hid.getDevices().then((ds) => ds.forEach(watch));
  const origOpen = P.open;
  P.open = async function () { const r = await origOpen.call(this); watch(this); return r; };

  window.akko = {
    mark: (label) => { log.push({ t: Math.round(performance.now()), dir: "MARK", data: label }); console.log("marked:", label); },
    count: () => log.length,
    dump: () => log.map(e => e.dir === "MARK" ? `\n### ${e.data}` : `${e.t}\t${e.dir}\t${e.dev}\tid=${e.id}\t${e.data}`).join("\n"),
    clear: () => { log.length = 0; },
    // Download the dump as a file, so a wrong clipboard can't lose it.
    save: (name = "akko-capture2") => {
      const a = document.createElement("a");
      a.href = URL.createObjectURL(new Blob([window.akko.dump()], { type: "text/plain" }));
      a.download = `${name}.txt`; a.click();
      console.log(`saved ${log.length} entries to ${name}.txt`);
    },
  };
  console.log("akko logger installed. akko.mark('label') before each action, akko.save() at the end.");
})();
