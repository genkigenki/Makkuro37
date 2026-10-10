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
#include "../firmware/src/params.c"

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
        int bad = peak < min_peak || dc > 500 || dc < -500;
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
    patch_init(&pt);
}
#define SEC(s) ((uint32_t)((s) * FS / CTL))

static void s_v1(uint32_t b) { gate_in[0] = b < SEC(3); }
static void s_v1_volt(uint32_t b)
{
    gate_in[0] = 1;
    pt.v[0][CL_VOLT] = (int16_t)(b * 127 / SEC(4));
}
static void s_classic4(uint32_t b) { gate_in[0] = gate_in[1] = gate_in[5] = gate_in[6] = b < SEC(4); }
static void s_all(uint32_t b) { uint32_t v; for (v = 0; v < NV; v++) gate_in[v] = b < SEC(4); }

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
    render(dir, "all_voices", 5, s_all, 3000);
    printf(fails ? "host: %d FAILED\n" : "host: all ok\n", fails);
    return fails != 0;
}
