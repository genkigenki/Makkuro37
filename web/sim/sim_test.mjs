// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com>
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
check("silent at power-on", peak < 0.001);
for (const p of [0, 2, 5]) { ex.sim_pad(p, 110); run(0.05); ex.sim_pad(p, 0); run(0.05); }
const load = run(3);
check(`three drones sound (peak ${peak.toFixed(2)})`, peak > 0.1 && peak <= 1.0);
const pads = new Uint32Array(ex.memory.buffer, ex.sim_pads(), 16);
check("their pads light", [0, 2, 5].every((p) => Math.max(pads[p] >> 16 & 255, pads[p] >> 8 & 255, pads[p] & 255) > 0x18) &&
      Math.max(pads[1] >> 16 & 255, pads[1] >> 8 & 255, pads[1] & 255) < 0x08);
const fb = new Uint16Array(ex.memory.buffer, ex.sim_fb(), 240 * 240);
let lit = 0;
for (let i = 0; i < 240 * 124; i++) if (fb[i] > 0x2000) lit++;
check(`creatures on the screen (${lit} bright pixels)`, lit > 3000);
ex.sim_enc(4, 3); run(0.2);
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
