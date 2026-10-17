/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) */
/* Host render of the Makkuro37 engine (same sources as the firmware): scenes to WAV, level / clip / DC
 * checks, cost per block.   tests/host OUTDIR   (tests/run_tests.sh) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "mk_tables.h"
#include "../firmware/src/synth.h"
#include "../firmware/src/tuning.c"
#include "../firmware/src/synth.c"
#include "../firmware/src/analog.c"
#include "../firmware/src/fx.c"
#include "../firmware/src/keys.c"
#include "../firmware/src/mix.c"
#include "../firmware/src/params.c"
#include "../firmware/src/presets.c"

static int fails;
static void wav(const char *path, const int16_t *s, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    uint32_t v;
    uint16_t a = 1, ch = 2, ba = 4, bits = 16;
    uint32_t sr = FS, br = FS * 4;
    if (!f) { perror(path); exit(1); }
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 4; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 4; fwrite(&v, 4, 1, f);
    fwrite(s, 4, frames, f);
    fclose(f);
}

typedef void (*script_t)(uint32_t block);
static int16_t pcm[2 * FS * 20];

/* render secs seconds, script(block) runs before each block; checks and writes OUTDIR/name.wav */
static void render(const char *dir, const char *name, double secs, script_t script, double min_peak)
{
    uint32_t nb = (uint32_t)(secs * FS / CTL), b, i;
    int32_t out[2 * CTL];
    double sum = 0, sq = 0, peak = 0, ns;
    struct timespec t0, t1;
    char path[512];
    syn.overs = 0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (b = 0; b < nb; b++) {
        script(b);
        synth_block(out, CTL);
        for (i = 0; i < 2 * CTL; i++) {
            int32_t x = out[i];
            if (x > 32767 || x < -32768) {
                printf("  FAIL %s: sample %d out of range\n", name, x);
                fails++;
                x = x > 0 ? 32767 : -32768;
            }
            pcm[b * 2 * CTL + i] = (int16_t)x;
            sum += x;
            sq += (double)x * x;
            if (abs(x) > peak)
                peak = abs(x);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ns = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / nb;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    wav(path, pcm, nb * CTL);
    {
        double n = nb * 2.0 * CTL, dc = sum / n, rms = __builtin_sqrt(sq / n);
        int bad = (min_peak < 0 ? peak > 10 : peak < min_peak) || dc > 500 || dc < -500;   /* (< 0: must be silent) */
        printf("  %-4s %-22s peak %6.0f (%5.1f dBFS)  rms %6.0f  dc %5.0f  limiter %6u  %6.0f ns/block\n",
               bad ? "FAIL" : "ok", name, peak, 20 * __builtin_log10(peak / 32768 + 1e-9), rms, dc,
               syn.overs, ns);
        fails += bad;
    }
}

static void reset(void)
{
    memset(vs, 0, sizeof vs);
    memset((void *)gate_in, 0, sizeof gate_in);
    memset(&kb, 0, sizeof kb);
    kb.cents[0] = kb.cents[1] = 4800;
    kb.lvel = 100;
    memset(&kg, 0, sizeof kg);
    kg.prog = 256;
    memset(&md, 0, sizeof md);
    patch_init(&pt);
#ifdef NOFX
    pt.g[G_FXL] = pt.g[G_FXR] = FX_OFF;
#endif
#ifdef NOFLT
    pt.g[G_FFL] = pt.g[G_FFR] = 127, pt.g[G_FRL] = pt.g[G_FRR] = pt.g[G_FDRIVE] = pt.g[G_FCVSRC] = 0;
#endif
    memset(fx, 0, sizeof fx);
    memset(flt, 0, sizeof flt);
    memset(&syn, 0, sizeof syn);
    memset(&mst, 0, sizeof mst);
    mst.env = LIM_T;
    memset(tape_buf, 0, sizeof tape_buf);
}
#define SEC(s) ((uint32_t)((s) * FS / CTL))

static void s_v1(uint32_t b) { gate_in[0] = b < SEC(3); }
static void s_v1_volt(uint32_t b)
{
    gate_in[0] = 1;
    pt.v[0][CL_VOLT] = (int16_t)(b * 127 / SEC(4));
}
static void s_classic4(uint32_t b) { gate_in[0] = gate_in[1] = gate_in[5] = gate_in[6] = b < SEC(4); }
static void s_all(uint32_t b)                      /* all eight: the drones and both VCOs on a key */
{
    uint32_t v;
    for (v = 0; v < NV; v++)
        gate_in[v] = b < SEC(4);
    if (b == 0)
        kb_press(12, 0, 100);
    if (b == SEC(4))
        kb_release(12);
}
static void s_nothing(uint32_t b) { (void)b; }
static void s_novco(uint32_t b) { gate_in[3] = gate_in[4] = 1; (void)b; }   /* pads do not start the VCOs */
static void s_noise3(uint32_t b) { gate_in[2] = b < SEC(4); }
static void s_noise3_modes(uint32_t b)              /* drone, FM, AM, FM+AM: 1.5 s each, DIVIDER 3 */
{
    gate_in[2] = 1;
    pt.v[2][NZ_MODE] = (int16_t)(b / SEC(1.5) % 4);
    pt.v[2][NZ_DIV] = 3;
    pt.v[2][NZ_RATE] = 70;
}
static void s_noise8(uint32_t b) { gate_in[7] = b < SEC(4); }
static void s_vco_keys(uint32_t b)                  /* a duophonic line (DUO), each wave in turn */
{
    static const int8_t LINE[8][2] = {{0, 7}, {2, 9}, {4, 12}, {5, 12}, {7, 16}, {5, 14}, {4, 12}, {0, 7}};
    uint32_t step = b / SEC(0.5);
    pt.v[3][VC_WAVE] = pt.v[4][VC_WAVE] = (int16_t)(step / 2 % 6);
    pt.g[G_KEYS] = KM_DUO;
    if (b % SEC(0.5) == 0) {
        kb.n = 0;
        if (step < 8) {
            kb_press(12 + LINE[step & 7][0], 0, 100);
            kb_press(12 + LINE[step & 7][1], 1, 100);
        }
    }
}
static void s_vco_fm_sync(uint32_t b)
{
    if (b == 0)
        kb_press(0, 0, 100);
    if (b == SEC(4))
        kb_release(0);
    pt.v[4][VC_SYNC] = 1;
    pt.v[4][VC_FM] = 60;
    pt.v[3][VC_FM] = 30;
    pt.v[4][VP_PITCH] = (int16_t)(b * 2400 / SEC(4) > 2400 ? 2400 : b * 2400 / SEC(4));
}
static void s_arp(uint32_t b)                       /* the arpeggiator over a held chord, SPLIT off */
{
    if (b == 0)
        kb_press(12, 0, 100), kb_press(16, 0, 100), kb_press(19, 0, 100);
    pt.g[G_ARP] = AR_UPDOWN, pt.g[G_ARATE] = 90, pt.g[G_AOCT] = 2;
    pt.v[3][VC_DEC] = 50, pt.v[3][VC_SUS] = 30;
}
static void s_vco_loop(uint32_t b)
{
    if (b == 0)
        kb_press(12, 0, 100);                       /* (a key, then pad 4's hold keeps it) */
    if (b == 10)
        kb_release(12), kb.padhold[0] = 1;
    if (b == SEC(4))
        kb.padhold[0] = 0;
    pt.v[3][VC_EG] = EG_LOOP;
    pt.v[3][VP_ATT] = 20;
    pt.v[3][VC_DEC] = 50;
    pt.v[3][VC_WAVE] = W_SAWMORPH;
    pt.v[3][VC_SHAPE] = (int16_t)(b * 127 / SEC(4) > 127 ? 127 : b * 127 / SEC(4));
}
static void s_seq(uint32_t b)                      /* voice 1 gated by the sequencer, LFO A wobbling gens 4 + 5 */
{
    (void)b;
    pt.g[G_RUN] = 1;
    pt.g[G_PULSER] = 70;
    pt.g[G_SGATES] = 0x1B;
    pt.v[0][VP_GSRC] = GS_SEQ;
    gate_in[0] = 1;                                 /* (its pad on: the sequencer plays it) */
    pt.v[0][VP_ATT] = 10;
    pt.v[0][VP_RLS] = 40;
    pt.v[0][CL_MOD] = 0x18;
    pt.v[0][VP_MSRC] = MS_LFOB;
    pt.v[0][VP_MAMT] = 50;
    pt.v[3][VP_MSRC] = MS_SEQ;                      /* and the sequencer plays VCO 4 */
    pt.v[3][VP_MAMT] = 80;
    pt.v[3][VC_EG] = EG_HOLD;
    gate_in[3] = 1;
}
static int fx_type;
static void s_fx(uint32_t b)                       /* voices 1 + 3 into one cartridge on both sides */
{
    gate_in[0] = b < SEC(2);
    gate_in[3] = b > SEC(0.5) && b < SEC(1.0);
    pt.g[G_FXL] = pt.g[G_FXR] = (int16_t)fx_type;
    pt.g[G_BLEND] = 90;
    pt.g[G_FXLY] = pt.g[G_FXRY] = 90;
    pt.v[0][VP_ATT] = 5;
    pt.v[0][VP_RLS] = 20;
}
static void s_filter(uint32_t b)                   /* voice 1 through a resonant sweep, driven */
{
    gate_in[0] = 1;
    pt.g[G_FFL] = pt.g[G_FFR] = (int16_t)(127 - (b * 127 / SEC(4) > 127 ? 127 : b * 127 / SEC(4)));
    pt.g[G_FRL] = pt.g[G_FRR] = 120;
    pt.g[G_FDRIVE] = 90;
}
static void s_res_fader(uint32_t b)                 /* a fader-swept resonant ladder, the fader's jitter included */
{
    static uint32_t r = 12345;
    if (b == 0) {
        preset_make(1, &pt);
        gate_in[0] = gate_in[5] = 1;
        pt.g[G_FMODE] = FM_LADDER;
    }
    if (b % 20 == 0) {                              /* (the UI's pace: ~15 ms) */
        int32_t pos = (int32_t)(b % SEC(4)) * 2 * 127 / SEC(4);
        pos = pos > 127 ? 254 - pos : pos;          /* down and up */
        r = r * 1103515245u + 12345u;
        pos += (int32_t)((r >> 16) % 3u) - 1;       /* +-1: an ADC */
        pt.g[G_FFL] = pt.g[G_FFR] = (int16_t)clamp(30 + pos * 90 / 127, 0, 127);
        pt.g[G_FRL] = pt.g[G_FRR] = (int16_t)(b < SEC(4) ? 110 : 127);
    }
}
static uint32_t preset_n;
static void s_preset(uint32_t b)                    /* a preset: its voices woken, a chord on the keys at 2..6 s */
{
    uint32_t v;
    if (b == 0) {
        preset_make(preset_n, &pt);
        for (v = 0; v < NV; v++)
            gate_in[v] = VTYPE[v] != VT_VCO && ((pt.g[G_AUTOGATE] >> v) & 1u);
        if (getenv("NOFLT")) pt.g[G_FFL] = pt.g[G_FFR] = 127, pt.g[G_FRL] = pt.g[G_FRR] = pt.g[G_FMD] = pt.g[G_FDRIVE] = 0;
        if (getenv("NOTAPE")) pt.g[G_TAPE] = 0;
        if (getenv("NOFX")) pt.g[G_FXL] = pt.g[G_FXR] = 0;
        if (getenv("NODTN")) for (v = 0; v < NV; v++) pt.v[v][VP_DMD] = 0;
    }
    if (b == SEC(2))
        kb_press(12, 0, 100), kb_press(15, 1, 90), kb_press(19, 1, 90);
    if (b == SEC(6))
        kb_release(12), kb_release(15), kb_release(19);
}

/* ------------------------------------------------------------- the global key (LOW KEYS) --- */
static void quiet(void)                             /* exact pitches: no drift, detune, vibrato, effects */
{
    uint32_t v;
    pt.g[G_DRIFT] = 0, pt.g[G_VIBD] = 0, pt.g[G_FXL] = pt.g[G_FXR] = FX_OFF, pt.g[G_TAPE] = 0;
    for (v = 0; v < NV; v++)
        pt.v[v][VP_DMD] = 0, pt.v[v][VP_MAMT] = 0;
}
static uint32_t classic_inc(uint32_t v, uint32_t g)  /* a drone generator's increment at its own pitch */
{
    const int16_t *p = pt.v[v];
    return cents_inc(GEN_LO[g] * 100 + p[CL_TUNE1 + g] + p[VP_PITCH] - p[CL_VOLT] * 2400 / 127 + pt.g[G_FINE]);
}
static uint32_t secs_ticks(double s) { return (uint32_t)(s * FS / CTL + 0.5); }

/* one key change in tuning set ts over secs: continuous at the start, each voice on its S-curve (no step back,
 * no jump), all at the target by the set time, none much earlier than 93 % of it */
static void glide_case(uint32_t ts, uint32_t r, int32_t dir, double secs)
{
    int32_t out[2 * CTL], from[NV + 1], last[NV + 1], d, p0[NV];
    uint32_t v, b, len = secs_ticks(secs), inc0[NV], ok = 1, early = 0, moving = 0;
    char what[96];
    reset();
    patch_init(&pt);
    quiet();
    pt.g[G_TSET] = (int16_t)ts;
    pt.g[G_KEYS] = KM_SINGLE;
    for (v = 0; v < NV; v++)
        gate_in[v] = 1;
    kb_press(19, 0, 100);                           /* (a key held: the VCOs follow the root) */
    for (b = 0; b < SEC(0.5); b++)
        synth_block(out, CTL);
    for (v = 0; v < NV; v++)
        inc0[v] = vs[v].inc[0], p0[v] = pt.v[v][VP_PITCH];
    d = key_move(r, dir, len);
    synth_block(out, CTL);
    for (v = 0; v < NV; v++) {                      /* the first tick after: still where it was */
        if (VTYPE[v] == VT_CLASSIC && vs[v].inc[0] != inc0[v])
            ok = 0;
        if (VTYPE[v] == VT_VCO && (vs[v].inc[0] > inc0[v] + inc0[v] / 1500u || vs[v].inc[0] + inc0[v] / 1500u < inc0[v]))
            ok = 0;
        if (VTYPE[v] != VT_VCO) {                   /* (VOICE PITCH: moved by the interval, within +-2 octaves) */
            int32_t e = p0[v] + d * 100;
            while (e > 2400)
                e -= 1200;
            while (e < -2400)
                e += 1200;
            if (pt.v[v][VP_PITCH] != e)
                ok = 0;
        }
    }
    for (v = 0; v <= NV; v++)
        from[v] = last[v] = kg.from[v];
    for (b = 1; b <= len + 1u; b++) {
        synth_block(out, CTL);
        for (v = 0; v <= NV; v++) {
            int32_t a = abs(kg.off[v]), al = abs(last[v]), slope = (int32_t)(1.6 * abs(from[v]) / (len ? len * 0.93 : 1)) + 16;
            if (a > al || al - a > slope || (from[v] > 0 ? kg.off[v] < 0 : kg.off[v] > 0)) {
                if (ok && getenv("KGDBG"))
                    printf("    v%u tick %u: off %d last %d from %d slope %d\n", v, b, kg.off[v], last[v], from[v], slope);
                ok = 0;                             /* (back again, too fast a step, past the target) */
            }
            if (abs(from[v]) >= 1600 && !kg.off[v] && b < (uint32_t)(len * 0.92))   /* (100 cents or more) */
                early = 1;
            last[v] = kg.off[v];
        }
        if (b == (uint32_t)(len * 0.97) && len > 20u)
            for (v = 0; v <= NV; v++)
                moving |= kg.off[v] != 0;
    }
    if (getenv("KGDBG"))
        printf("    ok %u prog %u early %u moving %u\n", ok, kg.prog, early, moving);
    for (v = 0; v <= NV; v++)
        ok &= kg.off[v] == 0;
    ok &= kg.prog == 256u && !early && (len <= 20u || moving || d == 0);
    for (v = 0; v < NV; v++)                        /* the drones on their new pitch, exactly */
        if (VTYPE[v] == VT_CLASSIC)
            ok &= vs[v].inc[0] == classic_inc(v, 0);
    snprintf(what, sizeof what, "key %-7s %s -> %-2s (%+d st) %.1f s", TSETS[ts].name, NOTE_NAMES[0], NOTE_NAMES[r % 12u],
             d, secs);
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    fails += !ok;
}

static void key_tests(void)
{
    uint32_t ts;
    int ok;
    printf("== global key (LOW KEYS)\n");
    reset();
    patch_init(&pt);                                /* the way it moves: nearest / down / up */
    ok = key_move(9, 0, 0) == -3 && pt.g[G_ROOT] == 9;
    pt.g[G_ROOT] = 0, ok &= key_move(6, 0, 0) == 6;
    pt.g[G_ROOT] = 0, ok &= key_move(7, 0, 0) == -5;
    pt.g[G_ROOT] = 0, ok &= key_move(9, -1, 0) == -3;
    pt.g[G_ROOT] = 0, ok &= key_move(9, 1, 0) == 9;
    pt.g[G_ROOT] = 0, ok &= key_move(0, -1, 0) == 0 && key_move(0, 1, 0) == 0;
    pt.g[G_ROOT] = 0, ok &= key_move(2, 1, 0) == 2;
    printf("  %-4s the way: nearest (tritone up), 24 keys: the lower octave down, the upper up\n", ok ? "ok" : "FAIL");
    fails += !ok;
    {   /* drones stay within +-2 octaves (an octave back when past it) */
        uint32_t i;
        reset();
        patch_init(&pt);
        for (i = 0; i < 30u; i++)
            key_move((uint32_t)pt.g[G_ROOT] + 5u, 1, 0);
        ok = 1;
        for (i = 0; i < NV; i++)
            ok &= pt.v[i][VP_PITCH] >= -2400 && pt.v[i][VP_PITCH] <= 2400;
        printf("  %-4s 30 fourths up: every drone within +-2 octaves\n", ok ? "ok" : "FAIL");
        fails += !ok;
    }
    for (ts = 0; ts < TS_N; ts++) {
        glide_case(ts, 2, 0, 6.0);
        glide_case(ts, 7, -1, 2.5);
    }
    glide_case(TS_JUST, 5, 1, 8.0);
    glide_case(TS_RAST, 11, 0, 0.0);
    glide_case(TS_PELOG, 4, -1, 0.5);
}

/* a key change across the drones, rendered: preset Ritual-like drones C -> F -> D, 6 s glides */
static void s_keydemo(uint32_t b)
{
    uint32_t v;
    if (b == 0) {
        preset_make(preset_n, &pt);
        for (v = 0; v < NV; v++)
            gate_in[v] = VTYPE[v] != VT_VCO && ((pt.g[G_AUTOGATE] >> v) & 1u);
    }
    if (b == SEC(1.0))
        kb_press(24, 0, 90);
    if (b == SEC(2.5))
        key_move(5, 0, secs_ticks(6.0));
    if (b == SEC(10.5))
        key_move(2, 0, secs_ticks(6.0));
    if (b == SEC(17.0))
        kb_release(24);
}
static double max_step(uint32_t f0, uint32_t f1)   /* the largest sample-to-sample step in pcm, frames f0..f1 */
{
    double m = 0;
    uint32_t i;
    for (i = f0 + 1u; i < f1; i++) {
        double s = abs(pcm[2 * i] - pcm[2 * i - 2]);
        if (s > m)
            m = s;
    }
    return m;
}

/* ------------------------------------------------------------- the played sounds --- */
static int tim_wave;
static void s_timbre(uint32_t b)                   /* a phrase on VCO 4 (+ 5 a third above in DUO), each played sound */
{
    static const int8_t LINE[12] = {0, 2, 3, 7, 5, 3, 2, 0, -2, 0, 3, 2};
    static const uint8_t VEL[12] = {70, 90, 110, 127, 80, 60, 100, 120, 50, 90, 127, 70};
    uint32_t step = b / SEC(0.75);
    if (b == 0) {
        pt.v[3][VC_WAVE] = pt.v[4][VC_WAVE] = (int16_t)tim_wave;
        pt.g[G_KEYS] = KM_DUO;
        pt.v[3][VP_ATT] = pt.v[4][VP_ATT] = tim_wave == W_GLASS ? 4 : 30;
        pt.v[3][VP_RLS] = pt.v[4][VP_RLS] = tim_wave == W_GLASS ? 80 : 70;
        pt.v[3][VC_DEC] = pt.v[4][VC_DEC] = 90, pt.v[3][VC_SUS] = pt.v[4][VC_SUS] = tim_wave == W_GLASS ? 50 : 110;
        pt.v[3][VC_SHAPE] = pt.v[4][VC_SHAPE] = tim_wave == W_GLASS ? 106 : 64;
        pt.v[4][VP_VOL] = 80;
        pt.g[G_GLIDE] = 30;
    }
    if (tim_wave == W_VOICE)                        /* the vowel moves through the phrase (A .. U) */
        pt.v[3][VC_SHAPE] = pt.v[4][VC_SHAPE] = (int16_t)(b * 127 / SEC(9) > 127 ? 127 : b * 127 / SEC(9));
    if (b % SEC(0.75) == 0) {
        kb.n = 0;
        if (step < 12u) {
            kb_press(17 + LINE[step], 0, VEL[step]);
            kb_press(17 + LINE[step] + (step & 1u ? 3 : 4), 1, VEL[step]);
        }
    }
}

/* the VCO 4 alone, one note: its raw output (before tone, mix, limiter) into buf (after 0.8 s, n frames) */
static void one_note(uint32_t wave, int32_t key, uint32_t vel, int32_t shape, int32_t joy, int32_t oct, float *buf, uint32_t n,
                     int32_t *vmax)
{
    int32_t raw[CTL];
    uint32_t b, i, k = 0, nb = (uint32_t)(SEC(0.8) + n / CTL + 1u);
    reset();
    patch_init(&pt);
    quiet();
    pt.g[G_FFL] = 127, pt.g[G_FRL] = 0, pt.g[G_FCVSRC] = 0, pt.g[G_FMD] = 0;
    pt.v[3][VC_WAVE] = (int16_t)wave, pt.v[3][VC_SHAPE] = (int16_t)shape, pt.v[3][VC_OCT] = (int16_t)oct;
    pt.v[3][VC_SUS] = 127, pt.v[3][VP_ATT] = 10, pt.v[3][VP_TONE] = 127;
    pt.v[4][VP_VOL] = 0;
    pt.g[G_KEYS] = KM_SPLIT;                        /* (only 4 plays: the left half) */
    joy_y = joy;
    kb_press(key, 0, vel);
    *vmax = 0;
    for (b = 0; b < nb; b++) {
        synth_tick();
        memset(raw, 0, sizeof raw);
        vco_render(raw, 0, CTL);
        for (i = 0; i < CTL; i++) {
            if (abs(raw[i]) > *vmax)
                *vmax = abs(raw[i]);
            if (b >= SEC(0.8) && k < n)
                buf[k++] = (float)raw[i];
        }
    }
    joy_y = 0;
}
#define NFFT 8192u
static float sbuf[NFFT];
static void harmonics(const float *x, double f0, double *e, uint32_t nh)   /* energy near each harmonic (Goertzel) */
{
    static double xw[NFFT];
    uint32_t h, i;
    for (i = 0; i < NFFT; i++)                      /* (Hann) */
        xw[i] = x[i] * (0.5 - 0.5 * __builtin_cos(2 * 3.14159265358979 * i / (NFFT - 1)));
    for (h = 1; h <= nh; h++) {
        double best = 0, f;
        for (f = h * f0 * 0.97; f <= h * f0 * 1.03 && f < FS / 2; f += 1.5) {
            double c2 = 2 * __builtin_cos(2 * 3.14159265358979 * f / FS), s1 = 0, s2 = 0, p;
            for (i = 0; i < NFFT; i++) {
                double s0 = xw[i] + c2 * s1 - s2;
                s2 = s1, s1 = s0;
            }
            p = s1 * s1 + s2 * s2 - c2 * s1 * s2;
            if (p > best)
                best = p;
        }
        e[h - 1] = best;
    }
}
static double profile_dist(const double *a, const double *b, uint32_t n)   /* 1 - cos of log-ish profiles */
{
    double ab = 0, aa = 0, bb = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double x = __builtin_sqrt(a[i]), y = __builtin_sqrt(b[i]);
        ab += x * y, aa += x * x, bb += y * y;
    }
    return 1 - ab / __builtin_sqrt(aa * bb + 1e-30);
}
static double centroid(const double *e, uint32_t n)   /* by amplitude, in harmonics */
{
    double s = 0, w = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (i + 1) * __builtin_sqrt(e[i]), w += __builtin_sqrt(e[i]);
    return s / (w + 1e-30);
}

/* the cost of each VCO wave: both VCOs on a key, the control tick and their render, ns a block (host) */
static volatile int32_t cost_sink;
static void wave_costs(void)
{
    uint32_t w, b, i;
    int32_t o4[CTL], o5[CTL];
    double base = 0;
    printf("== VCO cost per block, both VCOs (host ns; the chip has 726 us a block)\n");
    for (w = 0; w < W_N; w++) {
        struct timespec t0, t1;
        double ns;
        reset();
        patch_init(&pt);
        pt.v[3][VC_WAVE] = pt.v[4][VC_WAVE] = (int16_t)w;
        pt.g[G_KEYS] = KM_DUO;
        kb_press(12, 0, 100), kb_press(19, 0, 100);
        for (b = 0; b < 200u; b++)
            synth_tick();
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (b = 0; b < 40000u; b++) {
            synth_tick();
            for (i = 0; i < CTL; i++)
                o4[i] = o5[i] = 0;
            vco_render(o4, o5, CTL);
            cost_sink += o4[5] + o5[7];
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        ns = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / 40000.0;
        if (!w)
            base = ns;
        printf("  %-9s %6.0f ns  (x%.2f of SAW)%s\n", N_WAVE[w], ns, ns / base, "");
    }
}

static void timbre_tests(const char *dir)
{
    static const uint32_t W[4] = {W_SAW, W_VOICE, W_BOWED, W_GLASS};
    static const char *const WN[4] = {"SAW", "VOICE", "BOWED", "GLASS"};
    double e[4][24], ea[24], ei[24], elo[24], ehi[24];
    double f0 = 440.0 * __builtin_pow(2.0, (60 - 69) / 12.0);
    uint32_t a, b, k;
    int32_t vmax;
    printf("== the played sounds\n");
    for (k = 1; k < 4u; k++) {                      /* bounded over the corners: pitch, velocity, SHAPE, wheel */
        int32_t vmin_all = 1 << 30, vmax_all = 0;
        static const int32_t KEY[3] = {0, 24, 36}, OCT[3] = {0, 4, 7}, SH[3] = {0, 64, 127}, VL[2] = {1, 127};
        uint32_t ok = 1, i, j, s, q;
        for (i = 0; i < 3u; i++)
            for (j = 0; j < 3u; j++)
                for (s = 0; s < 3u; s++)
                    for (q = 0; q < 2u; q++) {
                        one_note(W[k], KEY[i], (uint32_t)VL[q], SH[s], q ? 32767 : 0, OCT[j], sbuf, 64, &vmax);
                        if (getenv("TIMDBG"))
                            printf("    %s key %d oct %d shape %d vel %d: %d\n", WN[k], KEY[i], OCT[j], SH[s], VL[q], vmax);
                        if (vmax > 34000 || vmax < 600)
                            ok = 0, printf("    %s key %d oct %d shape %d vel %d: voice peak %d\n", WN[k], KEY[i], OCT[j],
                                           SH[s], VL[q], vmax);
                        if (vmax > vmax_all)
                            vmax_all = vmax;
                        if (vmax < vmin_all)
                            vmin_all = vmax;
                    }
        printf("  %-4s %-6s bounded over pitch LO..1', velocity, SHAPE, wheel (54 notes): peak %d .. %d\n",
               ok ? "ok" : "FAIL", WN[k], vmin_all, vmax_all);
        fails += !ok;
    }
    for (k = 0; k < 4u; k++) {
        one_note(W[k], 24, 100, W[k] == W_GLASS ? 106 : W[k] == W_VOICE ? 0 : 64, 0, 4, sbuf, NFFT, &vmax);   /* (A) */
        harmonics(sbuf, f0, e[k], 24);
    }
    for (a = 0; a < 4u; a++)
        for (b = a + 1u; b < 4u; b++) {
            double dd = profile_dist(e[a], e[b], 24);
            int ok = dd > 0.03;
            printf("  %-4s spectra %-5s vs %-5s differ: %.3f\n", ok ? "ok" : "FAIL", WN[a], WN[b], dd);
            fails += !ok;
        }
    one_note(W_VOICE, 24, 100, 0, 0, 4, sbuf, NFFT, &vmax);          /* the vowel: A vs I */
    harmonics(sbuf, f0, ea, 24);
    one_note(W_VOICE, 24, 100, 64, 0, 4, sbuf, NFFT, &vmax);
    harmonics(sbuf, f0, ei, 24);
    {
        double dd = profile_dist(ea, ei, 24);
        printf("  %-4s VOICE: the vowels A and I differ: %.3f (centroid %.1f / %.1f)\n", dd > 0.1 ? "ok" : "FAIL", dd,
               centroid(ea, 24), centroid(ei, 24));
        fails += dd <= 0.1;
    }
    one_note(W_BOWED, 24, 30, 64, 0, 4, sbuf, NFFT, &vmax);          /* BOWED: velocity -> brightness */
    harmonics(sbuf, f0, elo, 24);
    one_note(W_BOWED, 24, 127, 64, 0, 4, sbuf, NFFT, &vmax);
    harmonics(sbuf, f0, ehi, 24);
    {
        double cl = centroid(elo, 24), ch = centroid(ehi, 24);
        printf("  %-4s BOWED: a harder key is brighter (centroid %.2f -> %.2f harmonics)\n", ch > cl * 1.2 ? "ok" : "FAIL",
               cl, ch);
        fails += ch <= cl * 1.2;
    }
    for (k = 1; k < 4u; k++) {                      /* the phrases (demos), and their cost */
        char name[32];
        snprintf(name, sizeof name, "timbre_%s", N_WAVE[W[k]]);
        tim_wave = (int)W[k];
        reset();
        render(dir, name, 10, s_timbre, 3000);
    }
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    printf("== engine renders (%s)\n", dir);
    reset();
    render(dir, "v1_classic", 5, s_v1, 3000);
    reset();
    render(dir, "v1_volt_sweep", 4, s_v1_volt, 3000);
    reset();
    render(dir, "classic_x4", 5, s_classic4, 3000);
    reset();
    render(dir, "noise3_siren", 5, s_noise3, 3000);
    reset();
    render(dir, "noise3_modes", 6, s_noise3_modes, 3000);
    reset();
    render(dir, "noise8_wind", 5, s_noise8, 1000);
    reset();
    render(dir, "vco_keys_waves", 5, s_vco_keys, 3000);
    reset();
    render(dir, "vco_fm_sync", 5, s_vco_fm_sync, 3000);
    reset();
    render(dir, "vco_loop_morph", 5, s_vco_loop, 3000);
    reset();
    render(dir, "vco_arp", 4, s_arp, 3000);
    reset();
    render(dir, "seq_lfo", 6, s_seq, 3000);
    reset();
    render(dir, "filter_sweep_res", 5, s_filter, 1000);
    for (fx_type = 1; fx_type < FX_N; fx_type++) {
        char name[32];
        snprintf(name, sizeof name, "fx_%s", N_FX[fx_type]);
        reset();
        render(dir, name, 5, s_fx, 1000);
    }
    reset();
    render(dir, "all_voices", 5, s_all, 3000);
    reset();
    render(dir, "vco_no_key_silent", 1, s_novco, -1);
    reset();
    render(dir, "nothing_silent", 1, s_nothing, -1);
    reset();
    render(dir, "res_fader_sweep", 8, s_res_fader, 1500);
    key_tests();
    preset_n = 0;
    reset();
    render(dir, "key_glide_drones", 19, s_keydemo, 3000);
    {   /* no click: the glides' largest sample step against the steady drones' */
        double st = max_step((uint32_t)(0.5 * FS), (uint32_t)(2.5 * FS)), gl = max_step((uint32_t)(2.5 * FS), (uint32_t)(18 * FS));
        int ok = gl < 2.0 * st + 500;
        printf("  %-4s key glide, no click: largest step %.0f gliding, %.0f steady\n", ok ? "ok" : "FAIL", gl, st);
        fails += !ok;
    }
    timbre_tests(dir);
    wave_costs();
    for (preset_n = 0; preset_n < NPRESET; preset_n++) {
        char name[48];
        uint32_t i;
        snprintf(name, sizeof name, "preset_%02u_%s", preset_n + 1, PRESET_NAME[preset_n]);
        for (i = 0; name[i]; i++)
            if (name[i] == ' ')
                name[i] = '_';
        reset();
        render(dir, name, 9, s_preset, 1500);
    }
    printf(fails ? "host: %d FAILED\n" : "host: all ok\n", fails);
    return fails != 0;
}
