// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 M. Geisler (makkuro)
// The simulator's wasm in Node, as the worklet runs it: boots, three pads wake three voices (audio, pad light,
// screen), a knob changes a value, the menus, LOWKEY (a low key glides the key), the played sounds; cost against
// real time; the screen to OUT.ppm (at the end), HOME during the key's glide to KEY.ppm.
//   node web/sim/sim_test.mjs build/sim/sim.wasm [OUT.ppm [KEY.ppm]]
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
run(0.7);                                                // (a tap while it fades silences it: wait)
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
// 0.3: SETUP page KEY (LOWKEY, GLIDE); the lowest keys glide the key of everything; the played sounds
const hold = (b, sec) => { ex.sim_button(b, 1); run(sec); ex.sim_button(b, 0); run(0.05); };
const key = (k, sec) => { ex.sim_key(k, 100); run(sec); ex.sim_key(k, 0); run(0.05); };
const ks = () => ex.sim_key_state(), root = () => ks() & 255, prog = () => ks() >> 16;
hold(12, 0.6);
check("PATCH held: SETUP", menu() === 9 && pg() === 0);
tap(3);
check("> : the page KEY", menu() === 9 && pg() === 1);
ex.sim_enc(0, 1); run(0.2);
check("K1: LOWKEY 12 KEYS", ex.sim_lowkeys() === 1);
tap(12);
const r0 = root(), to = (r0 + 2) % 12;
key(to, 0.05);
check(`a low key picks the key (${r0} > ${root()}), gliding`, root() === to && (ks() >> 8 & 255) === r0 && prog() < 256);
run(2.5);
const mid = prog();
if (process.argv[4]) shot(process.argv[4]);
check(`HOME shows the glide (${mid} / 256 at 2.6 s)`, mid > 40 && mid < 256);
run(4.5);
check("the glide ends on time (6 s)", prog() === 256);
for (let p = 0; p < 8; p++) {                            // all voices off, then quiet
  const c = pads[p];
  if (Math.max(c >> 16 & 255, c >> 8 & 255, c & 255) > 0x08) { ex.sim_pad(p, 110); run(0.05); ex.sim_pad(p, 0); run(0.05); }
}
run(5); peak = 0; run(0.5);
const quiet = peak;
check(`the drones off (peak ${quiet.toFixed(3)})`, quiet < 0.02);
peak = 0; key(0, 0.5);
check("a picker key plays nothing", peak < quiet + 0.02);
tap(13);                                                 // SOUND page 1, WAVE: K4 = V4 (VCO)
for (let i = 0; i < 20 && ex.sim_vparam(3, 0) !== 7; i++) { ex.sim_enc(3, 1); run(0.15); }
check("V4's WAVE: VOICE", ex.sim_vparam(3, 0) === 7);
tap(12);
run(1); peak = 0; key(24, 1.5);
const vpeak = peak;
check(`VOICE plays (peak ${vpeak.toFixed(2)})`, vpeak > 0.05 && vpeak <= 1.0);
for (const [w, n] of [[8, "BOWED"], [9, "GLASS"]]) {
  tap(13);
  for (let i = 0; i < 6 && ex.sim_vparam(3, 0) !== w; i++) { ex.sim_enc(3, 1); run(0.15); }
  tap(12); run(2); peak = 0; key(24, 1.5);
  check(`${n} plays (peak ${peak.toFixed(2)})`, ex.sim_vparam(3, 0) === w && peak > 0.02 && peak <= 1.0);
}
check(`cost: ${(load * 100).toFixed(1)} % of real time`, load < 0.5);
if (process.argv[3]) shot(process.argv[3]);
function shot(file) {
  const out = Buffer.alloc(240 * 240 * 3);
  for (let i = 0; i < 240 * 240; i++) {
    const c = fb[i];
    out[3 * i] = ((c >> 11) & 31) * 255 / 31; out[3 * i + 1] = ((c >> 5) & 63) * 255 / 63; out[3 * i + 2] = (c & 31) * 255 / 31;
  }
  fs.writeFileSync(file,Buffer.concat([Buffer.from("P6\n240 240\n255\n"), out]));
}
console.log(fails ? `sim: ${fails} FAILED` : "sim: all ok");
process.exit(fails ? 1 : 0);
