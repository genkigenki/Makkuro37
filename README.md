# Makkuro37

A microtonal drone synth for the M-Vave SMK-37 Pro. Eight voices, each a fuzzy
soot creature on the screen that a hammer bonks awake.

| Voices | |
|---|---|
| 1, 2, 6, 7 | classic drone: 5 saws in overlapping ranges (C0–D7), TUNE / MUTE / MOD each, VOLT transposes down, cross-FM past half |
| 3, 8 | noise drone: square tone + square modulator, FM along a DIVIDER staircase (sirens), AM, noise, S&H |
| 4, 5 | VCO: saw, pulse, triangle, sine, saw morph, sine/triangle morph, octave LO–1', FM 4→5, sync, ADSR with HOLD / LOOP |

Every oscillator drifts a little (ANALOG), each voice runs through a tanh knee and a two-pole TONE, classic voices
spread their generators in stereo (WIDTH). Plus LFO A / B, joystick (wheels), 5-step sequencer with PULSER, dual
filter (LADDER 24 dB or POLIVOKS 12 dB), dual effector (plate, room, spring, delay, shimmer, chorus, phaser, crush,
ring, freeze), stereo mixer with Melodee's limiter, tuning sets (12-TET, just, Pythagorean, 24-TET,
maqam Rast, slendro, pelog) with root, 8 scenes with MORPH.

## Controls

| Control | Does |
|---|---|
| Pads 1–8 (top row) | gate voice 1–8: tap = hold on / off, hold = momentary; softer = slower swell |
| Pads 9–13 | sequencer step gates (white: playhead) |
| Pad 14 | PULSER: tap = run, taps = tempo, hold = stop |
| Pads 15, 16 | effector L / R on / off |
| Keys, USB MIDI | voices 4 + 5, duophonic with glide, in the tuning set; OCTAVE −/+ |
| Pitch wheel | joystick X (springs back): bends voices 4 + 5, mod source |
| Mod wheel | joystick Y: opens the filter, mod source |
| Faders + FADER BANK | A: volume 1–4 · B: volume 5–8 · C: filter L / R freq + res · D: blend, LFO A / B rate, master |
| K1 · K2 · K3 · K4 | master · page · voice · tuning set |
| K5–K8 | the page's four values (blue, green, yellow, orange); the page's button again = next four |
| ARP · NOTE REPEAT · CHORD · GLOBE | VOICE · MUTE/MOD · ENV · MIX pages |
| BT · PARA | noise voices 3 / 8 · VCOs 4 / 5 |
| SCALE · SEQ REC · SEQ · KNOB BANK · FX | TUNING · LFO · SEQ · FILTER · FX pages |
| SEQ PLAY | sequencer run / stop |
| PATCH | SCENES (slot, load, save, MORPH, init); held: settings |
| ARP held · CHORD held | CHAOS: re-roll the voice within the scale · HOLD ALL / ALL OFF |
| < > | voice |
| OCTAVE − + OCTAVE + held 5 s | update mode |

## Build, test, install

```
sh build.sh                       # Linux / WSL; JieLi toolchain in ~/.jieli/toolchain, AC79 SDK in ~/fw-AC79_AIoT_SDK
sh tests/run_tests.sh OUTDIR      # engine renders (WAV), screens (PNG), board layer tests
python tools/fm1_install.py --yes build/makkuro37.fwsc
```

Identity `SMK-37 Pro_700`, USB MIDI port `Makkuro37`. Its data area is tagged `SMK37DATAMAKKURO`: installing it
clears another firmware's stored data (and the other way round). USB serial console: `help`.

## Credits

Sound after Melodee by Kerem Kilic (its PROPHET ladder, tanh knee, limiter,
spring reverb; GPL-3.0). Board layer (keys, pads, display, audio, flash, USB, update loader) from SLOOP by isod89,
based on Felucca by Leo Kuroshita. PLATE after J. Dattorro, "Effect Design 1" (1997). Terminus font (SIL OFL 1.1).
GPL-3.0 (LICENSE).
