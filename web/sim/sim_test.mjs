// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 M. Geisler (makkuro)
// The simulator's wasm in Node, as the worklet runs it: boots, three pads wake three voices (audio, pad light,
// screen), a knob changes a value; cost against real time; the screen to OUT.ppm.
//   node web/sim/sim_test.mjs build/sim/sim.wasm [OUT.ppm]
import fs from "fs";
const bytes = fs.readFileSync(process.argv[2] || "build/sim/sim.wasm");
const { instance } = await WebAssembly.instantiate(bytes, {});
const ex = instance.exports;
if (ex._initialize) ex._initialize();
ex.sim_init();
let fails = 0, peak = 0;
const check = (what, ok) => { console.log(`sim: ${what.padEnd(60)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
function run(sec) {
  const t0 = performance.now(), n = Math.round(sec * 44100 / 128);
  for (let i = 0; i < n; i++) {
    ex.sim_render(128);
    const l = new Float32Array(ex.memory.buffer, ex.sim_out_l(), 128);
    for (const x of l) peak = Math.max(peak, Math.abs(x));
  }
  return (performance.now() - t0) / (sec * 1000);
}
run(0.5);
check("silent at power-on (the presets)", peak < 0.001);
ex.sim_pad(0, 110); run(0.05); ex.sim_pad(0, 0);       // pick preset 1 (heard at once)
ex.sim_button(0, 1); run(0.05); ex.sim_button(0, 0);  // KNOB BANK: home
for (const p of [0, 1, 5]) { ex.sim_pad(p, 110); run(0.05); ex.sim_pad(p, 0); run(0.05); }   // (toggles some off)
for (const p of [0, 2, 5]) { ex.sim_pad(p, 110); run(0.05); ex.sim_pad(p, 0); run(0.05); }
const load = run(4);
check(`drones sound (peak ${peak.toFixed(2)})`, peak > 0.1 && peak <= 1.0);
const pads = new Uint32Array(ex.memory.buffer, ex.sim_pads(), 16);
check("their pads light", [2, 5].every((p) => Math.max(pads[p] >> 16 & 255, pads[p] >> 8 & 255, pads[p] & 255) > 0x18) &&
      Math.max(pads[3] >> 16 & 255, pads[3] >> 8 & 255, pads[3] & 255) < 0x08);
const fb = new Uint16Array(ex.memory.buffer, ex.sim_fb(), 240 * 240);
let lit = 0;
for (let i = 0; i < 240 * 124; i++) if (fb[i] > 0x2000) lit++;
check(`creatures on the screen (${lit} bright pixels)`, lit > 3000);
ex.sim_enc(4, 3); run(0.2);
// one logic: a button opens its menu at page 1, < > wrap, PATCH = home, KNOB BANK = PRESETS, a MINE pad held = save
const tap = (b) => { ex.sim_button(b, 1); run(0.05); ex.sim_button(b, 0); run(0.05); };
const menu = () => ex.sim_ui() >> 16, pg = () => (ex.sim_ui() >> 8) & 255;
tap(13);                                                 // PARA: SOUND
check("PARA opens SOUND at page 1", menu() === 1 && pg() === 0);
tap(2);
check("< wraps to the last page", pg() === 8);
tap(12);
check("PATCH: home", menu() === 0);
tap(0);
check("KNOB BANK: PRESETS", menu() === 10);
for (let i = 0; i < 6; i++) tap(3);                      // > to MINE (the 7th theme)
ex.sim_pad(2, 110); run(1.2); ex.sim_pad(2, 0); run(0.1);
check("MINE: a pad held 1 s saves", (ex.sim_user_used() & 4) !== 0 && (ex.sim_ui() & 255) === 96 + 2);
tap(12);
check(`cost: ${(load * 100).toFixed(1)} % of real time`, load < 0.5);
if (process.argv[3]) {
  const out = Buffer.alloc(240 * 240 * 3);
  for (let i = 0; i < 240 * 240; i++) {
    const c = fb[i];
    out[3 * i] = ((c >> 11) & 31) * 255 / 31; out[3 * i + 1] = ((c >> 5) & 63) * 255 / 63; out[3 * i + 2] = (c & 31) * 255 / 31;
  }
  fs.writeFileSync(process.argv[3], Buffer.concat([Buffer.from("P6\n240 240\n255\n"), out]));
}
console.log(fails ? `sim: ${fails} FAILED` : "sim: all ok");
process.exit(fails ? 1 : 0);
