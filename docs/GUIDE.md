# Makkuro37 – player's guide

Eight drone voices, each one a little soot creature on the screen. Wake one up and it hums until you send it
back to sleep.

**Lost?** Hold **KNOB BANK** to see HELP: the pads, the knobs, the buttons. Turn K2 to flip through the pages.

## First sound in 30 seconds

1. Tap **pad 1** (top row, far left). A hammer bonks creature 1, and a deep chord swells in.
2. Tap **pad 2** and **pad 6**. More drones join in. A tap keeps a drone on (HOLD). Tap it again to stop it.
3. Hold **pad 3** for a while and let go. It sounds only while you hold it (a siren).
4. Play the **keys**. Voices 4 and 5 follow them, two notes at a time, gliding.
5. Push the **mod wheel** up. The filter closes for a dark swell.
6. Turn **K4** for a different tuning: just, Pythagorean, quarter tones, maqam Rast, gamelan.

## The voices

| Voice | Sound | Pad |
|---|---|---|
| 1, 2, 6, 7 | **classic drone**: five saws, each in its own range, tuned to a chord | top 1, 2, 6, 7 |
| 3, 8 | **noise drone**: a tone that a slow square bends (sirens, birds) or chops, plus wind noise | top 3, 8 |
| 4, 5 | **VCO**: a classic oscillator played by the keys, with an envelope | top 4, 5 |

Every oscillator drifts a little, as analog ones do (ANALOG on the MIX page). Classic voices spread their five
saws across the stereo field (WIDTH).

## Pads

| Pads | |
|---|---|
| 1–8 (top) | the voices. **Tap**: on and held, tap again: off. **Hold**: sounds while held. Hit softer for a slower swell |
| 9–13 | the sequencer's 5 steps: on / off (the white one is playing) |
| 14 | **PULSER**: tap to start; tap in rhythm to set the tempo; hold to stop |
| 15, 16 | effect L / R on / off |

## Keys and wheels

- **Keys** (or USB MIDI): voices 4 + 5. The last two keys you hold give two notes; one key plays both.
  OCTAVE − / + shift the keys.
- **Pitch wheel** (springs back): bends voices 4 + 5.
- **Mod wheel**: closes the filter. Both wheels can also drive anything that has MOD SRC (JOY X / JOY Y).

## Knobs and faders

| | |
|---|---|
| K1 | master volume |
| K2 | page (and its next four values) |
| K3 or < > | which voice you edit |
| K4 | tuning set |
| K5 K6 K7 K8 | the four values on the page, coloured **blue, green, yellow, orange** on the screen |
| Faders | FADER BANK switches: A volume 1–4 · B volume 5–8 · C filter L / R · D effect blend, LFO A / B rate, master |

A turned knob shows its value big for a moment. A new page shows its four knobs.

## Pages

Press a page's button again to see its next four values.

| Button | Page | What you set |
|---|---|---|
| ARP | VOICE | the selected voice. Classic: TUNE 1–5 (each snaps to the tuning set), VOLT (lower; past half the saws bend each other: FM). Noise: PITCH, RANGE, RATE, ×10, FM, DIVIDER (steps), MODE, NOISE. VCO: WAVE, SHAPE, OCTAVE, TUNE, FM, SYNC |
| NOTE REPEAT | MUTE/MOD | classic: each saw on / off, which saws wobble; MOD SRC and amount |
| CHORD | ENV | ATTACK, RELEASE (VCO: + DECAY, SUSTAIN, EG GATE / HOLD / LOOP), GATE source (pad, sequencer, LFO) |
| GLOBE | MIX | volume, pan, TONE (bright ↔ warm), master, effect blend, ANALOG (drift), WIDTH |
| BT / PARA | NOISE / VCO | jump to voices 3 / 8, or 4 / 5 |
| SCALE | TUNING | tuning set, root, SNAP, FINE, glide, keys DUO / UNISON |
| SEQ REC | LFO | LFO A (slow) and B (fast): rate and wave; joystick offsets |
| SEQ | SEQ | the 5 step levels, STAGES 3 / 4 / 5, PULSER speed, run. SEQ PLAY starts and stops it too |
| KNOB BANK | FILTER | FREQ and RES left and right, LADDER (24 dB) or POLIVOKS (12 dB), LINK, DRIVE, CV source and amount |
| FX | FX | the two effects (PLATE, ROOM, SPRING, DELAY, SHIMMER, CHORUS, PHASER, CRUSH, RING, FREEZE), X Y Z each |
| PATCH | SCENES | slot, LOAD, SAVE, MORPH. Hold PATCH for settings: pad light, USB console, boot scene |

To use the sequencer: set a voice's GATE to SEQ (ENV page) and its pad plays in rhythm. Or set its MOD SRC to SEQ
and the steps play a melody.

## Live tricks

- **HOLD ALL**: hold CHORD. Everything that sounds stays on. Hold CHORD again: all off.
- **CHAOS**: hold ARP. The selected voice is rolled anew: new notes inside the tuning set, a new siren, a new wave.
- **MORPH**: on SCENES, pick a stored slot and turn MORPH. Your sound glides into that scene.
- **Tap tempo**: tap pad 14 in rhythm.
- **Swells**: soft pad hits swell in slowly, hard ones fast. Faders A / B are the voice volumes.
- **Freeze**: choose FREEZE as an effect. It records a second and loops it. Pad 15 / 16 off and on takes a new loop.

## Scenes

SCENES page (PATCH): pick a slot with K5, then turn SAVE or LOAD right twice. The first turn asks, the second does
it. There are 8 scenes. Settings (PATCH held) choose the scene you hear at power-on.

## Update mode

Hold OCTAVE − and OCTAVE + for 5 seconds. Hold OCTAVE − while switching on for USB rescue.
