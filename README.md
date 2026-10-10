# Makkuro37

A microtonal drone synth for the M-Vave SMK-37 Pro. Eight drones in eight columns:
knob Ki above, a fuzzy soot creature on the screen, pads below.

| Voices | |
|---|---|
| 1, 2, 6, 7 | classic drone: 5 oscillators in overlapping ranges (C0–D7), tune / mute / mod each, VOLT (FM past half), WAVE, subharmonic, detune beating |
| 3, 8 | noise drone: tone + square modulator, FM along a divider staircase (sirens), AM, noise, S&H |
| 4, 5 | keyboard VCOs: six waves incl. morphs, octave LO–1', FM 4→5, sync, sub-oscillator, ADSR with HOLD / LOOP |

Analog drift, a tanh knee and TONE per voice, stereo width; LFO A / B, joystick, 5-step sequencer with PULSER,
cascading gates; dual filter (LADDER, POLIVOKS, BAND, DIST); dual effector (plate, room, spring, resonator,
delay, shimmer, chorus, phaser, crush, ring, freeze); tape; Melodee's limiter; SPEAKER output mode; tuning sets
(12-TET, just, Pythagorean, 24-TET, maqam Rast, slendro, pelog); 24 presets, 8 scenes with MORPH, a motion loop.

## Controls

Player's guide: [docs/GUIDE.md](docs/GUIDE.md). Hold KNOB BANK on the unit for HELP.

| Control | Does |
|---|---|
| K1–K8 | home: voice i's pitch; in a menu: the value in column i |
| Top pads 1–8 | the drones: tap = on (hold) / off, hold = momentary; 4 / 5: hold the last key's note |
| Bottom pads | 1–5 sequencer steps · P pulser (tap tempo, hold stop) · H HOLD ALL · C CHAOS |
| Pads in a menu | two-choice values (top = on / A, bottom = B); DETUNE: top = its rate; questions: 15 red, 16 green |
| Keys | SEPARATE: VCOs 4 + 5 (single / duo / split, arp, key sequence) · FREE-PLAY: each key wakes the next drone |
| SEQ PLAY · SEQ REC | FREE-PLAY on / off · HOLD (free-play notes stay, a key again lets go) |
| Faders | A: cutoff, resonance, filter LFO rate, depth · B (FADER BANK): FX L, FX R, space, tape |
| Wheels | bend (springs back) · mod: the filter closes, resonance rises |
| PARA · CHORD · NOTE REPEAT · GLOBE | VOICE (waves, each voice) · ENV · MOD (incl. sub, detune) · MIX (volume, pan, tone, master) |
| SCALE · BT · SEQ · FX · ARP | TUNING · LFO · SEQ (steps, motion loop) · FX (filter, effects) · KEYS |
| PATCH | PRESETS (the power-on screen; < > themes), LOAD / SAVE / MORPH; held: SETUP |
| KNOB BANK · < > · OCTAVE | home (held: HELP) · menu pages (home: the next preset) · transpose |
| OCTAVE − + OCTAVE + held 5 s | update mode |

## Build, test, install

```
sh build.sh                       # Linux / WSL; JieLi toolchain in ~/.jieli/toolchain, AC79 SDK in ~/fw-AC79_AIoT_SDK
sh tests/run_tests.sh OUTDIR      # engine renders (WAV), screens (PNG), board layer tests
python tools/fm1_install.py --yes build/makkuro37.fwsc
```

Simulator (the firmware's engine and screen in the browser, Emscripten in `~/emsdk`):

```
sh web/sim/build.sh && python3 -m http.server -d build/sim 8738    # open http://localhost:8738
node web/sim/sim_test.mjs build/sim/sim.wasm                         # its test
```

Identity `SMK-37 Pro_700`, USB MIDI port `Makkuro37`. Its data area is tagged `SMK37DATAMAKKURO`: installing it
clears another firmware's stored data (and the other way round). USB serial console: `help`.

## Credits

Inspired by the Moog Subharmonicon, the Horn of Abraxas and Abducted Drone. Sound after Melodee by Kerem Kilic (its PROPHET ladder, tanh knee, limiter,
spring reverb; GPL-3.0). Board layer (keys, pads, display, audio, flash, USB, update loader) from SLOOP by isod89,
based on Felucca by Leo Kuroshita. PLATE after J. Dattorro, "Effect Design 1" (1997). Terminus font (SIL OFL 1.1).
GPL-3.0 (LICENSE).
