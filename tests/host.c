/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
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
    fx_mute = 0;
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
