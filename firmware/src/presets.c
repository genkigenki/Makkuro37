/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Factory presets: six themes of 16 (a page of pads each): DRONES (dark textures), ABRAXAS (after the
 * Horn of Abraxas set), NATURE, SCI-FI, ORGAN, PULSE. Each the power-on sound with changes; AUTO: the voices it
 * wakes. Measured by tests/host.c (level, low-mids, highs, width, movement). */
#define NTHEME 6u
#define PRESET_PAGE 16u
#define NPRESET (NTHEME * PRESET_PAGE)
static const char *const THEME_NAME[NTHEME] = {"DRONES", "ABRAXAS", "NATURE", "SCI-FI", "ORGAN", "PULSE"};
static const char *const PRESET_NAME[NPRESET] = {
    "LAYERED BED", "BLADE RUN", "UNSTABLE", "MICRO CLUSTER", "NOISE ORGANIC", "EERIE SWELL", "EVOLVING",
    "DEEP SPACE", "MONASTERY", "GAMELAN", "MAQAM", "SIRENS", "PULSE", "INIT", "WARM BED", "HOLLOW",
    "MANDY DRONE", "CORRODED BELL", "DEEP VOID", "MACHINERY", "RITUAL PAD", "STRING GHOST", "STAR WIND",
    "SCI-FI LAB", "QUARTERS", "CATHEDRAL", "DUST CHOIR", "IRON LUNG", "BLACK SUN", "TEMPLE", "SALT MARSH", "SLOW GOD",
    "WIND", "SURF", "RAIN", "BIRDS", "CRICKETS", "THUNDER", "RIVER", "OCEAN", "FOREST", "CAVE DRIP", "STORM",
    "DUNES", "GLACIER", "NIGHT", "MONSOON", "AURORA",
    "SPACESHIP", "ALARM", "TRANSMISSION", "ALIEN CHOIR", "TELEMETRY", "BEACON", "LASER GRID", "HYPERDRIVE",
    "ROBOT", "SIGNAL LOSS", "NEBULA", "ORBIT", "REACTOR", "PROBE", "STATIC", "AIRLOCK",
    "CHAPEL", "PIPE ORGAN", "REED", "HARMONIUM", "CHOIR AAH", "CHOIR OOH", "DRONE CHOIR", "CALLIOPE", "MONKS",
    "CANTOR", "VESPERS", "LAUDS", "TAMBURA", "SRUTI BOX", "BAGPIPE", "ACCORDION",
    "HEARTBEAT", "MOTORIK", "TRAIN", "CLOCKWORK", "MORSE", "PULSAR", "TICKER", "GALLOP", "DRIP", "SONAR",
    "PING PONG", "TRANCE", "STEPPER", "LOCOMOTIVE", "BREATH", "ECHO PULSE"};

static void chord(patch_t *p, uint32_t v, int32_t m0, int32_t m1, int32_t m2, int32_t m3, int32_t m4)   /* MIDI notes */
{
    const int32_t m[5] = {m0, m1, m2, m3, m4};
    uint32_t g;
    p->v[v][CL_MUTE] = 0;
    for (g = 0; g < 5u; g++) {
        int32_t lo = GEN_LO[g], hi = GEN_HI[g], n = m[g];
        if (n < 0) {                               /* (-1: this generator off) */
            p->v[v][CL_MUTE] |= (int16_t)(1 << g);
            continue;
        }
        while (n < lo) n += 12;
        while (n > hi) n -= 12;
        p->v[v][CL_TUNE1 + g] = (int16_t)((n - lo) * 100);
    }
}
static void env(patch_t *p, uint32_t v, int32_t a, int32_t r) { p->v[v][VP_ATT] = (int16_t)a, p->v[v][VP_RLS] = (int16_t)r; }
static void fx2(patch_t *p, int32_t t, int32_t x, int32_t y, int32_t z, int32_t blend)
{
    p->g[G_FXL] = p->g[G_FXR] = (int16_t)t;
    p->g[G_FXLX] = p->g[G_FXRX] = (int16_t)x;
    p->g[G_FXLY] = p->g[G_FXRY] = (int16_t)y;
    p->g[G_FXLZ] = p->g[G_FXRZ] = (int16_t)z;
    p->g[G_BLEND] = p->g[G_BLENDR] = (int16_t)blend;
}
static void voice(patch_t *p, uint32_t v, int32_t wave, int32_t tone, int32_t det, int32_t drate, int32_t vol)
{
    if (VTYPE[v] == VT_VCO)
        p->v[v][VC_WAVE] = (int16_t)wave;
    else
        p->v[v][VP_WAVE] = (int16_t)wave;
    p->v[v][VP_TONE] = (int16_t)tone, p->v[v][VP_DMD] = (int16_t)det, p->v[v][VP_DMR] = (int16_t)drate;
    p->v[v][VP_VOL] = (int16_t)vol;
}
static void sub(patch_t *p, uint32_t v, int32_t amt, int32_t div) { p->v[v][CL_SUB] = (int16_t)amt, p->v[v][CL_SUBDIV] = (int16_t)div; }
static void pflt(patch_t *p, int32_t mode, int32_t cut, int32_t res, int32_t fmr, int32_t fmd)
{
    p->g[G_FMODE] = (int16_t)mode;
    p->g[G_FFL] = p->g[G_FFR] = (int16_t)cut;
    p->g[G_FRL] = p->g[G_FRR] = (int16_t)res;
    p->g[G_FMR] = (int16_t)fmr, p->g[G_FMD] = (int16_t)fmd;
}

static void preset_old(uint32_t n, patch_t *p)   /* the first 24 (their old numbers) */
{
    p->g[G_FFL] = p->g[G_FFR] = 112;               /* (the faders' filter: a little closed, room to sweep) */
    p->g[G_FRL] = p->g[G_FRR] = 16;
    switch (n) {
    case 0:                                        /* LAYERED BED: four just chords, warm, beating, a long plate */
        p->g[G_TSET] = TS_JUST;
        chord(p, 0, 24, 43, 52, 67, 79), chord(p, 1, 31, 43, 55, 71, 83);
        chord(p, 5, 28, 40, 59, 64, 88), chord(p, 6, 36, 48, 55, 76, 84);
        voice(p, 0, CW_SAW, 72, 30, 30, 96), voice(p, 1, CW_TRI, 80, 34, 22, 90);
        voice(p, 5, CW_SAW, 68, 26, 36, 84), voice(p, 6, CW_TRI, 76, 30, 28, 84);
        sub(p, 0, 50, 2);
        env(p, 0, 85, 100), env(p, 1, 95, 100), env(p, 5, 100, 100), env(p, 6, 105, 100);
        fx2(p, FX_PLATE, 115, 40, 40, 60);
        p->g[G_AUTOGATE] = 0x63;
        break;
    case 1:                                        /* BLADE RUN: a wide minor 9, slow swells, a gliding lead */
        chord(p, 0, 21, 40, 60, 71, 83), chord(p, 5, 33, 45, 55, 64, 79);
        voice(p, 0, CW_SAW, 76, 40, 24, 100), voice(p, 5, CW_SAW, 72, 44, 30, 96);
        sub(p, 0, 40, 2);
        env(p, 0, 105, 110), env(p, 5, 110, 110);
        p->v[3][VC_WAVE] = W_SAW, p->v[4][VC_WAVE] = W_SAW, p->v[4][VC_TUNE] = 12, p->v[3][VC_SUB] = 40;
        p->v[3][VP_ATT] = p->v[4][VP_ATT] = 50, p->v[3][VP_RLS] = p->v[4][VP_RLS] = 90;
        p->v[3][VP_TONE] = p->v[4][VP_TONE] = 84;
        p->g[G_GLIDE] = 60, p->g[G_VIBD] = 24, p->g[G_VIBR] = 40, p->g[G_KEYS] = KM_SINGLE;
        p->g[G_WIDTH] = 40, p->g[G_DRIFT] = 70;
        pflt(p, FM_LADDER, 104, 30, 20, 30);
        fx2(p, FX_PLATE, 120, 40, 60, 70);
        p->g[G_AUTOGATE] = 0x21;
        break;
    case 2:                                        /* UNSTABLE: drifting, wobbling, a little FM, worn tape */
        chord(p, 0, 26, 38, 57, 69, 81), chord(p, 6, 29, 41, 53, 72, 84);
        p->v[0][CL_VOLT] = 76, p->v[6][CL_VOLT] = 70;
        voice(p, 0, CW_SAW, 70, 70, 70, 96), voice(p, 6, CW_SQUARE, 64, 60, 60, 80);
        env(p, 0, 70, 90), env(p, 6, 80, 90);
        p->g[G_DRIFT] = 120, p->g[G_TAPE] = 90;
        pflt(p, FM_LADDER, 100, 40, 50, 40);
        fx2(p, FX_PLATE, 110, 50, 30, 55);
        p->g[G_AUTOGATE] = 0x41;
        break;
    case 3:                                        /* MICRO CLUSTER: quarter tones, soft triangles, shimmer */
        p->g[G_TSET] = TS_24TET, p->g[G_SNAP] = 0;
        chord(p, 0, 36, 48, 60, 72, 84), chord(p, 1, 36, 48, 60, 72, 84);
        p->v[0][CL_TUNE1 + 2] += 50, p->v[1][CL_TUNE1 + 3] += 50, p->v[1][CL_TUNE1 + 1] += 150;
        voice(p, 0, CW_TRI, 90, 20, 20, 96), voice(p, 1, CW_TRI, 90, 24, 26, 90);
        env(p, 0, 95, 110), env(p, 1, 105, 110);
        fx2(p, FX_SHIMMER, 110, 60, 50, 65);
        p->g[G_AUTOGATE] = 0x03;
        break;
    case 4:                                        /* NOISE ORGANIC: wind and surf through the resonator */
        voice(p, 2, CW_SQUARE, 60, 0, 50, 90), voice(p, 7, CW_SQUARE, 50, 0, 50, 90);
        p->v[2][NZ_PITCH] = 0, p->v[2][NZ_NOISE] = 100, p->v[2][NZ_MODE] = NM_AM, p->v[2][NZ_RATE] = 2;
        p->v[7][NZ_PITCH] = 0, p->v[7][NZ_NOISE] = 90, p->v[7][NZ_MODE] = NM_DRONE;
        p->v[7][VP_MSRC] = MS_LFOA;
        env(p, 2, 90, 100), env(p, 7, 100, 100);
        p->g[G_FXL] = FX_RESO, p->g[G_FXLX] = 110, p->g[G_FXLY] = 60, p->g[G_FXLZ] = 64, p->g[G_BLEND] = 70;
        p->g[G_FXR] = FX_PLATE, p->g[G_FXRX] = 112, p->g[G_BLENDR] = 60;
        p->g[G_AUTOGATE] = 0x84;
        break;
    case 5:                                        /* EERIE SWELL: one wakes the next (CASCADE), close seconds */
        chord(p, 0, 26, 39, 51, -1, -1), chord(p, 1, 27, 38, 62, -1, -1);
        chord(p, 5, 29, 41, 63, 74, -1), chord(p, 6, 30, 42, 66, 75, 87);
        voice(p, 0, CW_SAW, 70, 40, 20, 90), voice(p, 1, CW_TRI, 74, 40, 24, 90);
        voice(p, 5, CW_SAW, 66, 36, 28, 84), voice(p, 6, CW_TRI, 70, 36, 32, 80);
        env(p, 0, 110, 110), env(p, 1, 110, 110), env(p, 5, 110, 110), env(p, 6, 110, 110);
        p->v[1][VP_GSRC] = GS_CASC, p->v[5][VP_GSRC] = GS_CASC, p->v[6][VP_GSRC] = GS_CASC;
        p->v[2][VP_GSRC] = GS_PAD;
        fx2(p, FX_SPRING, 100, 50, 40, 50);
        p->g[G_AUTOGATE] = 0x01;
        break;
    case 6:                                        /* EVOLVING: wobbling generators, the filter breathing */
        chord(p, 1, 24, 43, 55, 67, 81), chord(p, 5, 28, 47, 52, 71, 84);
        p->v[1][CL_MOD] = 0x1C, p->v[1][VP_MSRC] = MS_LFOA, p->v[1][VP_MAMT] = 24;
        p->v[5][CL_MOD] = 0x0E, p->v[5][VP_MSRC] = MS_DRIFT, p->v[5][VP_MAMT] = 40;
        voice(p, 1, CW_SAW, 84, 30, 30, 96), voice(p, 5, CW_SAW, 80, 30, 40, 90);
        env(p, 1, 85, 100), env(p, 5, 95, 100);
        pflt(p, FM_LADDER, 92, 50, 30, 70);
        fx2(p, FX_PLATE, 112, 50, 40, 55);
        p->g[G_AUTOGATE] = 0x22;
        break;
    case 7:                                        /* DEEP SPACE: low sines, subharmonics, a long plate */
        chord(p, 0, 21, 40, 52, 67, -1), chord(p, 1, 28, 43, 59, 71, -1);
        voice(p, 0, CW_SINE, 100, 20, 20, 110), voice(p, 1, CW_TRI, 90, 24, 26, 100);
        sub(p, 0, 70, 2), sub(p, 1, 50, 3);
        env(p, 0, 90, 110), env(p, 1, 100, 110);
        fx2(p, FX_PLATE, 122, 40, 50, 70);
        p->g[G_AUTOGATE] = 0x03;
        break;
    case 8:                                        /* MONASTERY: just fifths, soft squares, a breath of wind */
        p->g[G_TSET] = TS_JUST;
        chord(p, 0, 24, 43, 55, 67, 79), chord(p, 1, 31, 36, 60, 72, -1);
        voice(p, 0, CW_SQUARE, 60, 16, 20, 96), voice(p, 1, CW_SQUARE, 64, 20, 26, 90);
        sub(p, 0, 40, 2);
        env(p, 0, 80, 100), env(p, 1, 95, 100);
        p->v[7][NZ_PITCH] = 0, p->v[7][NZ_NOISE] = 70, p->v[7][NZ_MODE] = NM_AM, p->v[7][NZ_RATE] = 4;
        p->v[7][VP_VOL] = 60, p->v[7][VP_TONE] = 50;
        fx2(p, FX_PLATE, 124, 30, 70, 75);
        p->g[G_AUTOGATE] = 0x83;
        break;
    case 9:                                        /* GAMELAN: pelog, the keys ring bells, the resonator */
        p->g[G_TSET] = TS_PELOG;
        chord(p, 0, 24, 43, 55, 67, 79);
        voice(p, 0, CW_TRI, 80, 20, 30, 70);
        p->v[3][VC_WAVE] = W_SINTRI, p->v[3][VC_SHAPE] = 30, p->v[4][VC_WAVE] = W_SINE;
        p->v[3][VP_ATT] = p->v[4][VP_ATT] = 0, p->v[3][VC_DEC] = p->v[4][VC_DEC] = 55, p->v[3][VC_SUS] = p->v[4][VC_SUS] = 20;
        p->v[3][VP_RLS] = p->v[4][VP_RLS] = 70;
        p->g[G_ARP] = AR_UPDOWN, p->g[G_ARATE] = 78, p->g[G_AOCT] = 2, p->g[G_ALATCH] = 1;
        p->g[G_FXL] = FX_RESO, p->g[G_FXLX] = 100, p->g[G_FXLY] = 70, p->g[G_FXLZ] = 64, p->g[G_BLEND] = 45;
        p->g[G_FXR] = FX_SPRING, p->g[G_FXRX] = 100, p->g[G_BLENDR] = 55;
        p->g[G_AUTOGATE] = 0x01;
        break;
    case 10:                                       /* MAQAM: Rast, a C-G drone, a singing VCO */
        p->g[G_TSET] = TS_RAST;
        chord(p, 0, 24, 43, 55, -1, -1), chord(p, 1, 36, 52, 60, 64, -1);
        voice(p, 0, CW_SAW, 72, 20, 24, 96), voice(p, 1, CW_TRI, 78, 24, 30, 90);
        env(p, 0, 70, 90), env(p, 1, 80, 90);
        p->v[3][VC_WAVE] = W_SAWMORPH, p->v[3][VC_SHAPE] = 90, p->v[3][VP_ATT] = 30, p->v[3][VP_RLS] = 70;
        p->g[G_VIBD] = 40, p->g[G_VIBR] = 55, p->g[G_GLIDE] = 40, p->g[G_KEYS] = KM_SINGLE;
        fx2(p, FX_PLATE, 105, 60, 30, 55);
        p->g[G_AUTOGATE] = 0x03;
        break;
    case 11:                                       /* SIRENS */
        p->v[2][NZ_PITCH] = 80, p->v[2][NZ_RATE] = 30, p->v[2][NZ_FMAMT] = 90, p->v[2][NZ_DIV] = 1, p->v[2][NZ_MODE] = NM_FM;
        p->v[7][NZ_PITCH] = 100, p->v[7][NZ_RANGE] = 1, p->v[7][NZ_RATE] = 60, p->v[7][NZ_FMAMT] = 70;
        p->v[7][NZ_DIV] = 5, p->v[7][NZ_MODE] = NM_FMAM, p->v[7][NZ_NOISE] = 0, p->v[7][VP_VOL] = 70;
        voice(p, 2, CW_TRI, 70, 0, 50, 90), voice(p, 7, CW_SQUARE, 64, 0, 50, 70);
        env(p, 2, 40, 80), env(p, 7, 40, 80);
        fx2(p, FX_DELAY, 90, 70, 60, 45);
        p->g[G_AUTOGATE] = 0x84;
        break;
    case 12:                                       /* PULSE: the sequencer gates two drones, a delay */
        p->v[0][VP_GSRC] = p->v[1][VP_GSRC] = GS_SEQ;
        env(p, 0, 20, 50), env(p, 1, 30, 60);
        p->g[G_RUN] = 1, p->g[G_PULSER] = 60, p->g[G_SGATES] = 0x1B;
        fx2(p, FX_DELAY, 70, 60, 50, 40);
        p->g[G_AUTOGATE] = 0x03;
        break;
    case 14:                                       /* MANDY DRONE: dark saws through the distortion */
        chord(p, 0, 24, 36, 55, 60, -1), chord(p, 6, 25, 37, 56, -1, -1);
        voice(p, 0, CW_SAW, 70, 30, 20, 72), voice(p, 6, CW_SAW, 64, 36, 26, 64);
        sub(p, 0, 60, 2), sub(p, 6, 50, 2);
        env(p, 0, 80, 100), env(p, 6, 90, 100);
        pflt(p, FM_DIST, 70, 30, 20, 20);
        p->g[G_FDRIVE] = 60;
        fx2(p, FX_PLATE, 118, 30, 40, 55);
        p->g[G_AUTOGATE] = 0x41;
        break;
    case 15:                                       /* CORRODED BELL: FM VCOs, a metallic ring on the keys */
        p->v[3][VC_WAVE] = W_SINE, p->v[4][VC_WAVE] = W_SINE, p->v[4][VC_FM] = 80, p->v[4][VC_TUNE] = 380;
        p->v[3][VP_ATT] = p->v[4][VP_ATT] = 0, p->v[3][VC_DEC] = p->v[4][VC_DEC] = 80, p->v[3][VC_SUS] = p->v[4][VC_SUS] = 30;
        p->v[3][VP_RLS] = p->v[4][VP_RLS] = 95;
        chord(p, 1, -1, 43, 55, 67, -1);
        voice(p, 1, CW_TRI, 84, 30, 20, 64);
        p->v[3][VP_VOL] = p->v[4][VP_VOL] = 120;
        p->g[G_FXL] = FX_RESO, p->g[G_FXLX] = 110, p->g[G_FXLY] = 90, p->g[G_FXLZ] = 64, p->g[G_BLEND] = 40;
        p->g[G_FXR] = FX_PLATE, p->g[G_FXRX] = 116, p->g[G_BLENDR] = 60;
        p->g[G_AUTOGATE] = 0x02;
        break;
    case 16:                                       /* DEEP VOID: the lowest sines, subharmonics, closed */
        chord(p, 0, 12, 36, -1, -1, -1), chord(p, 1, 19, 43, -1, -1, -1), chord(p, 5, -1, -1, 55, 62, -1);
        voice(p, 0, CW_SINE, 90, 30, 16, 120), voice(p, 1, CW_SINE, 90, 30, 20, 110), voice(p, 5, CW_TRI, 80, 40, 12, 60);
        env(p, 5, 120, 115);
        sub(p, 0, 80, 2), sub(p, 1, 60, 3);
        env(p, 0, 100, 115), env(p, 1, 110, 115);
        pflt(p, FM_LADDER, 80, 40, 16, 50);
        fx2(p, FX_PLATE, 126, 20, 60, 70);
        p->g[G_AUTOGATE] = 0x23;
        break;
    case 17:                                       /* MACHINERY: stepped noise FM, crushed, a delay, the pulser */
        p->v[2][NZ_PITCH] = 50, p->v[2][NZ_RATE] = 70, p->v[2][NZ_FMAMT] = 100, p->v[2][NZ_DIV] = 6, p->v[2][NZ_MODE] = NM_FM;
        p->v[7][NZ_PITCH] = 30, p->v[7][NZ_RATE] = 90, p->v[7][NZ_MULT] = 1, p->v[7][NZ_MODE] = NM_AM, p->v[7][NZ_NOISE] = 50;
        voice(p, 2, CW_SQUARE, 70, 0, 50, 64), voice(p, 7, CW_SQUARE, 60, 0, 50, 52);
        p->v[0][VP_GSRC] = GS_SEQ, p->v[0][VP_VOL] = 64;
        env(p, 0, 10, 40), env(p, 2, 20, 60), env(p, 7, 20, 60);
        p->g[G_RUN] = 1, p->g[G_PULSER] = 80;
        p->g[G_FXL] = FX_CRUSH, p->g[G_FXLX] = 40, p->g[G_FXLY] = 60, p->g[G_BLEND] = 50;
        p->g[G_FXR] = FX_DELAY, p->g[G_FXRX] = 60, p->g[G_FXRY] = 70, p->g[G_BLENDR] = 50;
        p->g[G_AUTOGATE] = 0x85;
        break;
    case 18:                                       /* RITUAL PAD: soft squares beating, chorus and plate */
        chord(p, 0, 26, 38, 57, 62, 74), chord(p, 5, 33, 45, 50, 69, 81);
        voice(p, 0, CW_SQUARE, 62, 50, 40, 90), voice(p, 5, CW_SQUARE, 60, 50, 46, 90);
        env(p, 0, 90, 100), env(p, 5, 100, 100);
        p->g[G_FXL] = FX_CHORUS, p->g[G_FXLX] = 40, p->g[G_FXLY] = 80, p->g[G_BLEND] = 50;
        p->g[G_FXR] = FX_PLATE, p->g[G_FXRX] = 116, p->g[G_BLENDR] = 65;
        p->g[G_AUTOGATE] = 0x21;
        break;
    case 19:                                       /* STRING GHOST: saws through a breathing ladder, tape */
        chord(p, 1, 28, 40, 59, 64, 79), chord(p, 6, 31, 43, 55, 71, 83);
        voice(p, 1, CW_SAW, 90, 40, 30, 96), voice(p, 6, CW_SAW, 86, 40, 36, 90);
        env(p, 1, 95, 105), env(p, 6, 100, 105);
        pflt(p, FM_LADDER, 94, 30, 24, 60);
        p->g[G_TAPE] = 60;
        p->g[G_FXL] = FX_CHORUS, p->g[G_FXLX] = 30, p->g[G_FXLY] = 70, p->g[G_BLEND] = 45;
        p->g[G_FXR] = FX_PLATE, p->g[G_FXRX] = 112, p->g[G_BLENDR] = 60;
        p->g[G_AUTOGATE] = 0x42;
        break;
    case 20:                                       /* STAR WIND: slendro, wobbling generators, shimmer */
        p->g[G_TSET] = TS_SLENDRO;
        chord(p, 1, 24, 41, 55, 67, 81), chord(p, 5, 28, 45, 52, 71, 84);
        p->v[1][CL_MOD] = 0x1C, p->v[1][VP_MSRC] = MS_LFOA, p->v[1][VP_MAMT] = 28;
        p->v[5][CL_MOD] = 0x0E, p->v[5][VP_MSRC] = MS_DRIFT, p->v[5][VP_MAMT] = 40;
        voice(p, 1, CW_TRI, 84, 20, 30, 96), voice(p, 5, CW_SAW, 76, 24, 30, 90);
        env(p, 1, 85, 100), env(p, 5, 95, 100);
        p->g[G_LARATE] = 30;
        fx2(p, FX_SHIMMER, 100, 50, 60, 50);
        p->g[G_AUTOGATE] = 0x22;
        break;
    case 21:                                       /* SCI-FI LAB: looping morph VCO, S&H noise, ring */
        p->v[3][VC_WAVE] = W_SAWMORPH, p->v[3][VC_EG] = EG_LOOP, p->v[3][VC_DEC] = 50, p->v[3][VP_ATT] = 20;
        p->v[3][VP_MSRC] = MS_LFOB, p->v[3][VP_MAMT] = 30;
        p->v[2][NZ_PITCH] = 60, p->v[2][NZ_SHAMT] = 90, p->v[2][NZ_SHCLK] = 2, p->v[2][NZ_MODE] = NM_AM;
        p->v[2][NZ_RATE] = 80, p->v[2][NZ_MULT] = 1, p->v[2][VP_VOL] = 70;
        p->g[G_LBRATE] = 70;
        p->g[G_FXL] = FX_RING, p->g[G_FXLX] = 40, p->g[G_FXLY] = 60, p->g[G_BLEND] = 50;
        p->g[G_FXR] = FX_PLATE, p->g[G_FXRX] = 100, p->g[G_BLENDR] = 50;
        p->g[G_AUTOGATE] = 0x04;
        break;
    case 22:                                       /* QUARTERS: 24-TET clusters, shimmer */
        p->g[G_TSET] = TS_24TET;
        chord(p, 0, 24, 43, 55, 67, 79);
        p->v[0][CL_TUNE1 + 2] += 50, p->v[0][CL_TUNE1 + 4] += 50;
        chord(p, 6, 31, 38, 62, 74, 86);
        p->v[6][CL_TUNE1 + 1] += 50, p->v[6][CL_TUNE1 + 3] += 50;
        voice(p, 0, CW_TRI, 80, 20, 20, 96), voice(p, 6, CW_SAW, 76, 20, 24, 90);
        env(p, 0, 90, 100), env(p, 6, 100, 100);
        fx2(p, FX_SHIMMER, 110, 60, 50, 60);
        p->g[G_AUTOGATE] = 0x41;
        break;
    case 23:                                       /* CATHEDRAL: all four chords, a huge plate */
        voice(p, 0, CW_SAW, 76, 20, 20, 96), voice(p, 1, CW_TRI, 80, 20, 26, 90);
        voice(p, 5, CW_SAW, 72, 20, 30, 90), voice(p, 6, CW_TRI, 76, 20, 34, 84);
        env(p, 0, 80, 110), env(p, 1, 90, 110), env(p, 5, 100, 110), env(p, 6, 110, 110);
        fx2(p, FX_PLATE, 126, 30, 80, 80);
        p->g[G_MASTER] = 92;
        p->g[G_AUTOGATE] = 0x63;
        break;
    default:                                       /* INIT */
        break;
    }
}

/* ------------------------------------------------------------- the newer themes, from small recipes --- */
enum { CH_FIFTH, CH_MAJ, CH_MIN, CH_SUS2, CH_MIN7, CH_MAJ7, CH_CLUSTER, CH_OCT };
static void drone(patch_t *p, uint32_t v, int32_t root, uint32_t type, int32_t wave, int32_t tone, int32_t det,
                  int32_t vol, int32_t att, int32_t rls)
{
    static const int8_t IV[8][5] = {{0, 7, 12, 19, 24}, {0, 7, 16, 19, 24}, {0, 7, 15, 19, 24}, {0, 7, 14, 19, 26},
                                    {0, 7, 15, 22, 24}, {0, 7, 16, 23, 28}, {0, 2, 3, 5, 7}, {0, 12, 24, 36, 48}};
    const int8_t *iv = IV[type & 7u];
    chord(p, v, root + iv[0], root + iv[1] + 12, root + iv[2] + 12, root + iv[3] + 24, root + iv[4] + 24);
    voice(p, v, wave, tone, det, 20 + det / 2, vol);
    env(p, v, att, rls);
}
static void noisev(patch_t *p, uint32_t v, int32_t pitch, int32_t range, int32_t rate, int32_t mult, int32_t fm,
                   int32_t div, int32_t mode, int32_t noise, int32_t tone, int32_t vol, int32_t att, int32_t rls)
{
    int16_t *q = p->v[v];
    q[NZ_PITCH] = (int16_t)pitch, q[NZ_RANGE] = (int16_t)range, q[NZ_RATE] = (int16_t)rate, q[NZ_MULT] = (int16_t)mult;
    q[NZ_FMAMT] = (int16_t)fm, q[NZ_DIV] = (int16_t)div, q[NZ_MODE] = (int16_t)mode, q[NZ_NOISE] = (int16_t)noise;
    q[VP_TONE] = (int16_t)tone, q[VP_VOL] = (int16_t)vol;
    env(p, v, att, rls);
}
static void vco(patch_t *p, uint32_t v, int32_t wave, int32_t shape, int32_t oct, int32_t tune, int32_t fm, int32_t subl,
                int32_t att, int32_t dec, int32_t sus, int32_t rls, int32_t eg)
{
    int16_t *q = p->v[v];
    q[VC_WAVE] = (int16_t)wave, q[VC_SHAPE] = (int16_t)shape, q[VC_OCT] = (int16_t)oct, q[VC_TUNE] = (int16_t)tune;
    q[VC_FM] = (int16_t)fm, q[VC_SUB] = (int16_t)subl;
    q[VP_ATT] = (int16_t)att, q[VC_DEC] = (int16_t)dec, q[VC_SUS] = (int16_t)sus, q[VP_RLS] = (int16_t)rls;
    q[VC_EG] = (int16_t)eg;
}
static void fxlr(patch_t *p, int32_t tl, int32_t xl, int32_t yl, int32_t zl, int32_t bl, int32_t tr, int32_t xr, int32_t yr,
                 int32_t zr, int32_t br)
{
    p->g[G_FXL] = (int16_t)tl, p->g[G_FXLX] = (int16_t)xl, p->g[G_FXLY] = (int16_t)yl, p->g[G_FXLZ] = (int16_t)zl;
    p->g[G_BLEND] = (int16_t)bl;
    p->g[G_FXR] = (int16_t)tr, p->g[G_FXRX] = (int16_t)xr, p->g[G_FXRY] = (int16_t)yr, p->g[G_FXRZ] = (int16_t)zr;
    p->g[G_BLENDR] = (int16_t)br;
}
static void pulser(patch_t *p, int32_t rate, int32_t gates, int32_t stages, int32_t s1, int32_t s2, int32_t s3,
                   int32_t s4, int32_t s5)
{
    p->g[G_RUN] = 1, p->g[G_PULSER] = (int16_t)rate, p->g[G_SGATES] = (int16_t)gates, p->g[G_STAGES] = (int16_t)stages;
    p->g[G_SEQ1] = (int16_t)s1, p->g[G_SEQ2] = (int16_t)s2, p->g[G_SEQ3] = (int16_t)s3, p->g[G_SEQ4] = (int16_t)s4;
    p->g[G_SEQ5] = (int16_t)s5;
}

static void theme_more(uint32_t k, patch_t *p)   /* k: the preset number */
{
    p->g[G_FFL] = p->g[G_FFR] = 112, p->g[G_FRL] = p->g[G_FRR] = 16;
    switch (k) {
    /* DRONES */
    case 14:                                       /* WARM BED */
        drone(p, 0, 26, CH_MAJ7, CW_TRI, 80, 30, 100, 90, 100), drone(p, 1, 33, CH_SUS2, CW_SAW, 70, 30, 90, 100, 100);
        sub(p, 0, 50, 2), fx2(p, FX_PLATE, 116, 40, 40, 60), p->g[G_AUTOGATE] = 0x03;
        break;
    case 15:                                       /* HOLLOW */
        drone(p, 0, 24, CH_OCT, CW_SINE, 100, 40, 84, 100, 110), drone(p, 6, 31, CH_FIFTH, CW_TRI, 90, 50, 70, 110, 110);
        fx2(p, FX_RESO, 110, 50, 64, 50), p->g[G_FXR] = FX_PLATE, p->g[G_BLENDR] = 60, p->g[G_AUTOGATE] = 0x41;
        break;
    /* ABRAXAS */
    case 26:                                       /* DUST CHOIR */
        drone(p, 0, 26, CH_MIN, CW_TRI, 76, 60, 96, 100, 110), drone(p, 5, 29, CH_CLUSTER, CW_TRI, 70, 50, 70, 110, 110);
        fxlr(p, FX_CHORUS, 30, 80, 40, 50, FX_PLATE, 120, 30, 50, 70), p->g[G_AUTOGATE] = 0x21;
        break;
    case 27:                                       /* IRON LUNG */
        noisev(p, 2, 20, 0, 6, 0, 80, 4, NM_FMAM, 40, 50, 90, 60, 90);
        drone(p, 0, 24, CH_FIFTH, CW_SQUARE, 60, 20, 80, 70, 100), pflt(p, FM_DIST, 80, 40, 10, 30);
        fx2(p, FX_PLATE, 110, 30, 30, 55), p->g[G_AUTOGATE] = 0x05;
        break;
    case 28:                                       /* BLACK SUN */
        drone(p, 0, 22, CH_MIN, CW_SAW, 64, 50, 100, 80, 110), drone(p, 6, 23, CH_MIN, CW_SAW, 60, 50, 80, 90, 110);
        sub(p, 0, 70, 2), pflt(p, FM_LADDER, 88, 60, 12, 50), fx2(p, FX_PLATE, 124, 20, 50, 70), p->g[G_AUTOGATE] = 0x41;
        break;
    case 29:                                       /* TEMPLE */
        p->g[G_TSET] = TS_JUST, drone(p, 0, 24, CH_FIFTH, CW_SQUARE, 64, 20, 100, 80, 110);
        drone(p, 1, 36, CH_OCT, CW_SINE, 90, 30, 80, 100, 110), fx2(p, FX_PLATE, 126, 30, 70, 80), p->g[G_AUTOGATE] = 0x03;
        break;
    case 30:                                       /* SALT MARSH */
        noisev(p, 7, 0, 0, 3, 0, 0, 1, NM_AM, 80, 46, 90, 100, 110), drone(p, 1, 31, CH_SUS2, CW_TRI, 74, 40, 80, 100, 110);
        fx2(p, FX_RESO, 104, 40, 64, 55), p->g[G_FXR] = FX_PLATE, p->g[G_BLENDR] = 65, p->g[G_AUTOGATE] = 0x82;
        break;
    case 31:                                       /* SLOW GOD */
        drone(p, 0, 12, CH_FIFTH, CW_SAW, 66, 20, 110, 120, 120), drone(p, 1, 19, CH_OCT, CW_TRI, 80, 30, 90, 120, 120);
        sub(p, 0, 80, 3), p->g[G_DRIFT] = 90, fx2(p, FX_PLATE, 126, 20, 60, 75), p->g[G_AUTOGATE] = 0x03;
        break;
    /* NATURE: wind and water */
    case 32:                                       /* WIND */
        noisev(p, 2, 0, 0, 3, 0, 0, 1, NM_AM, 100, 56, 100, 90, 100), noisev(p, 7, 0, 0, 5, 0, 0, 1, NM_AM, 90, 40, 80, 100, 100);
        p->v[7][VP_MSRC] = MS_LFOA, fx2(p, FX_PLATE, 110, 40, 20, 50), p->g[G_AUTOGATE] = 0x84;
        break;
    case 33:                                       /* SURF */
        noisev(p, 2, 0, 0, 1, 0, 0, 1, NM_AM, 110, 60, 110, 80, 100), noisev(p, 7, 0, 0, 2, 0, 0, 1, NM_AM, 100, 70, 90, 90, 100);
        drone(p, 0, 24, CH_OCT, CW_SINE, 90, 30, 60, 110, 110), fx2(p, FX_PLATE, 118, 40, 30, 60), p->g[G_AUTOGATE] = 0x85;
        break;
    case 34:                                       /* RAIN */
        noisev(p, 2, 0, 0, 100, 1, 0, 1, NM_AM, 100, 84, 90, 40, 80);
        noisev(p, 7, 0, 0, 0, 0, 0, 1, NM_DRONE, 70, 50, 70, 60, 80), fx2(p, FX_PLATE, 100, 60, 10, 50), p->g[G_AUTOGATE] = 0x84;
        break;
    case 35:                                       /* BIRDS */
        noisev(p, 2, 110, 1, 50, 1, 70, 6, NM_FMAM, 0, 80, 70, 20, 60), noisev(p, 7, 100, 1, 70, 1, 90, 3, NM_FMAM, 0, 76, 60, 20, 60);
        fx2(p, FX_DELAY, 70, 50, 70, 40), p->g[G_FXR] = FX_PLATE, p->g[G_BLENDR] = 60, p->g[G_AUTOGATE] = 0x84;
        break;
    case 36:                                       /* CRICKETS */
        noisev(p, 2, 120, 1, 110, 1, 10, 2, NM_AM, 0, 90, 60, 20, 60), noisev(p, 7, 116, 1, 100, 1, 20, 3, NM_AM, 10, 86, 50, 20, 60);
        drone(p, 6, 36, CH_FIFTH, CW_SINE, 80, 30, 50, 110, 110), fx2(p, FX_PLATE, 108, 50, 30, 50), p->g[G_AUTOGATE] = 0xC4;
        break;
    case 37:                                       /* THUNDER */
        noisev(p, 2, 0, 0, 2, 0, 0, 1, NM_AM, 110, 30, 110, 60, 110), drone(p, 0, 12, CH_OCT, CW_SINE, 70, 60, 90, 110, 120);
        sub(p, 0, 80, 2), fx2(p, FX_PLATE, 124, 20, 40, 70), p->g[G_AUTOGATE] = 0x05;
        break;
    case 38:                                       /* RIVER */
        noisev(p, 2, 0, 0, 40, 1, 0, 1, NM_DRONE, 100, 60, 100, 60, 90), pflt(p, FM_LADDER, 92, 50, 50, 50);
        drone(p, 1, 26, CH_SUS2, CW_TRI, 76, 30, 60, 100, 110), p->g[G_AUTOGATE] = 0x06;
        fx2(p, FX_PLATE, 110, 50, 20, 50);
        break;
    case 39:                                       /* OCEAN */
        noisev(p, 7, 0, 0, 1, 0, 0, 1, NM_AM, 100, 50, 100, 100, 110), drone(p, 0, 24, CH_SUS2, CW_SINE, 90, 30, 80, 110, 110);
        drone(p, 1, 31, CH_FIFTH, CW_TRI, 80, 30, 70, 120, 110), fx2(p, FX_PLATE, 122, 30, 40, 70), p->g[G_AUTOGATE] = 0x83;
        break;
    case 40:                                       /* FOREST */
        noisev(p, 2, 104, 1, 40, 1, 60, 5, NM_FMAM, 0, 76, 50, 30, 60), noisev(p, 7, 0, 0, 4, 0, 0, 1, NM_AM, 70, 44, 80, 90, 100);
        drone(p, 1, 28, CH_SUS2, CW_TRI, 80, 30, 70, 100, 110), fx2(p, FX_PLATE, 116, 40, 30, 60), p->g[G_AUTOGATE] = 0x86;
        break;
    case 41:                                       /* CAVE DRIP */
        noisev(p, 2, 90, 1, 20, 0, 0, 1, NM_AM, 0, 90, 70, 0, 40), drone(p, 0, 24, CH_OCT, CW_SINE, 80, 20, 60, 110, 110);
        fx2(p, FX_RESO, 116, 70, 64, 60), p->g[G_FXR] = FX_DELAY, p->g[G_FXRX] = 80, p->g[G_FXRY] = 70, p->g[G_BLENDR] = 50;
        p->g[G_AUTOGATE] = 0x05;
        break;
    case 42:                                       /* STORM */
        noisev(p, 2, 0, 0, 8, 0, 0, 1, NM_AM, 120, 64, 110, 40, 90), noisev(p, 7, 10, 0, 3, 0, 40, 2, NM_FMAM, 90, 40, 90, 60, 100);
        drone(p, 0, 12, CH_FIFTH, CW_SAW, 60, 50, 70, 90, 110), fx2(p, FX_PLATE, 118, 30, 30, 55), p->g[G_AUTOGATE] = 0x85;
        break;
    case 43:                                       /* DUNES */
        noisev(p, 2, 0, 0, 2, 0, 0, 1, NM_AM, 80, 40, 90, 110, 110), drone(p, 1, 26, CH_MIN, CW_SAW, 64, 50, 80, 110, 110);
        p->g[G_TSET] = TS_RAST, fx2(p, FX_PLATE, 118, 30, 50, 60), p->g[G_AUTOGATE] = 0x06;
        break;
    case 44:                                       /* GLACIER */
        drone(p, 0, 36, CH_SUS2, CW_SINE, 100, 20, 90, 110, 120), drone(p, 6, 43, CH_FIFTH, CW_TRI, 100, 30, 70, 120, 120);
        fx2(p, FX_SHIMMER, 116, 60, 70, 65), p->g[G_AUTOGATE] = 0x41;
        break;
    case 45:                                       /* NIGHT */
        noisev(p, 2, 120, 1, 105, 1, 10, 2, NM_AM, 0, 90, 50, 20, 60), drone(p, 1, 26, CH_MIN7, CW_TRI, 70, 30, 80, 110, 110);
        fx2(p, FX_PLATE, 120, 30, 40, 65), p->g[G_AUTOGATE] = 0x06;
        break;
    case 46:                                       /* MONSOON */
        noisev(p, 2, 0, 0, 90, 1, 0, 1, NM_AM, 120, 80, 100, 30, 80), noisev(p, 7, 0, 0, 6, 0, 0, 1, NM_AM, 100, 50, 90, 60, 90);
        drone(p, 0, 26, CH_MIN, CW_SAW, 64, 40, 70, 100, 110), fx2(p, FX_PLATE, 116, 40, 20, 55), p->g[G_AUTOGATE] = 0x85;
        break;
    case 47:                                       /* AURORA */
        drone(p, 0, 31, CH_SUS2, CW_TRI, 90, 40, 90, 120, 120);
        drone(p, 6, 36, CH_FIFTH, CW_SINE, 100, 40, 70, 120, 120), fx2(p, FX_SHIMMER, 120, 60, 60, 60), p->g[G_AUTOGATE] = 0x41;
        break;
    /* SCI-FI */
    case 48:                                       /* SPACESHIP */
        drone(p, 0, 24, CH_FIFTH, CW_SAW, 70, 20, 90, 60, 100), p->v[0][CL_VOLT] = 80;
        noisev(p, 7, 30, 0, 60, 1, 30, 1, NM_DRONE, 40, 50, 60, 60, 90), pflt(p, FM_LADDER, 96, 60, 20, 30);
        fx2(p, FX_PLATE, 112, 40, 30, 50), p->g[G_AUTOGATE] = 0x81;
        break;
    case 49:                                       /* ALARM */
        noisev(p, 2, 90, 1, 60, 0, 100, 1, NM_FM, 0, 70, 70, 10, 50), drone(p, 0, 24, CH_OCT, CW_SQUARE, 60, 10, 60, 40, 90);
        fx2(p, FX_DELAY, 60, 50, 60, 40), p->g[G_AUTOGATE] = 0x05;
        break;
    case 50:                                       /* TRANSMISSION */
        noisev(p, 2, 70, 1, 90, 1, 80, 8, NM_FMAM, 20, 70, 70, 20, 60), p->v[2][NZ_SHAMT] = 60;
        drone(p, 6, 31, CH_FIFTH, CW_SINE, 90, 20, 60, 80, 100);
        fx2(p, FX_RING, 50, 40, 60, 40), p->g[G_FXR] = FX_DELAY, p->g[G_BLENDR] = 50, p->g[G_AUTOGATE] = 0x44;
        break;
    case 51:                                       /* ALIEN CHOIR */
        drone(p, 0, 36, CH_CLUSTER, CW_TRI, 80, 70, 90, 100, 110), drone(p, 1, 43, CH_CLUSTER, CW_SINE, 90, 80, 80, 110, 110);
        p->g[G_TSET] = TS_24TET, fx2(p, FX_SHIMMER, 110, 70, 50, 60), p->g[G_AUTOGATE] = 0x03;
        break;
    case 52:                                       /* TELEMETRY */
        noisev(p, 2, 80, 1, 80, 1, 60, 7, NM_FMAM, 0, 70, 60, 10, 50), p->v[2][NZ_SHAMT] = 80, p->v[2][NZ_SHCLK] = 3;
        drone(p, 0, 24, CH_FIFTH, CW_TRI, 80, 20, 70, 90, 110);
        pulser(p, 90, 0x1F, 2, 20, 90, 50, 120, 70), fx2(p, FX_DELAY, 50, 50, 60, 40), p->g[G_AUTOGATE] = 0x05;
        break;
    case 53:                                       /* BEACON */
        drone(p, 0, 48, CH_OCT, CW_SINE, 100, 0, 70, 10, 60), p->v[0][VP_GSRC] = GS_LFOA, p->g[G_LARATE] = 60;
        drone(p, 1, 24, CH_FIFTH, CW_TRI, 80, 20, 60, 100, 110);
        fx2(p, FX_DELAY, 100, 70, 50, 50), p->g[G_AUTOGATE] = 0x02;
        break;
    case 54:                                       /* LASER GRID */
        vco(p, 3, W_PULSE, 30, 5, 0, 0, 0, 0, 30, 60, 40, EG_LOOP), vco(p, 4, W_SAW, 64, 4, 500, 60, 0, 0, 40, 50, 40, EG_LOOP);
        p->v[4][VC_SYNC] = 1, p->g[G_ARP] = AR_UP, p->g[G_ARATE] = 100, p->g[G_AOCT] = 2, p->g[G_ALATCH] = 1;
        drone(p, 0, 24, CH_FIFTH, CW_SAW, 64, 30, 70, 80, 100), pflt(p, FM_LADDER, 100, 50, 40, 30);
        fx2(p, FX_DELAY, 60, 60, 60, 40), p->g[G_AUTOGATE] = 0x01;
        break;
    case 55:                                       /* HYPERDRIVE */
        drone(p, 0, 24, CH_FIFTH, CW_SAW, 70, 60, 90, 40, 90), p->v[0][CL_VOLT] = 100;
        noisev(p, 7, 0, 0, 100, 1, 0, 1, NM_AM, 70, 70, 60, 40, 80), fx2(p, FX_SHIMMER, 100, 70, 40, 50), p->g[G_AUTOGATE] = 0x81;
        break;
    case 56:                                       /* ROBOT */
        vco(p, 3, W_PULSE, 20, 4, 0, 40, 30, 0, 40, 40, 40, EG_NORM), vco(p, 4, W_SAWMORPH, 100, 3, 0, 80, 0, 0, 40, 40, 40, EG_NORM);
        noisev(p, 2, 60, 0, 20, 0, 50, 2, NM_FM, 0, 70, 60, 40, 80), drone(p, 0, 24, CH_FIFTH, CW_SQUARE, 64, 20, 70, 60, 100);
        fx2(p, FX_RING, 60, 30, 80, 50), p->g[G_FXR] = FX_CRUSH, p->g[G_FXRX] = 30, p->g[G_BLENDR] = 40, p->g[G_AUTOGATE] = 0x05;
        break;
    case 57:                                       /* SIGNAL LOSS */
        noisev(p, 2, 60, 1, 30, 1, 40, 3, NM_FMAM, 60, 70, 56, 10, 60), drone(p, 1, 31, CH_SUS2, CW_TRI, 76, 40, 70, 90, 110);
        fx2(p, FX_CRUSH, 50, 70, 60, 50), p->g[G_FXR] = FX_FREEZE, p->g[G_BLENDR] = 40, p->g[G_AUTOGATE] = 0x06;
        break;
    case 58:                                       /* NEBULA */
        drone(p, 1, 31, CH_MAJ7, CW_TRI, 90, 50, 90, 110, 120), drone(p, 5, 38, CH_SUS2, CW_SINE, 96, 50, 70, 120, 120);
        p->v[1][CL_MOD] = 0x1F, p->v[1][VP_MSRC] = MS_LFOB, p->v[1][VP_MAMT] = 12, fx2(p, FX_SHIMMER, 120, 60, 60, 70);
        p->g[G_AUTOGATE] = 0x22;
        break;
    case 59:                                       /* ORBIT */
        drone(p, 0, 24, CH_FIFTH, CW_SINE, 90, 40, 90, 90, 110), p->v[0][CL_MOD] = 0x1E, p->v[0][VP_MSRC] = MS_LFOA;
        p->v[0][VP_MAMT] = 30, p->g[G_LARATE] = 20, drone(p, 6, 31, CH_OCT, CW_TRI, 80, 30, 60, 100, 110);
        fx2(p, FX_PLATE, 118, 40, 40, 60), p->g[G_AUTOGATE] = 0x41;
        break;
    case 60:                                       /* REACTOR */
        drone(p, 0, 12, CH_FIFTH, CW_SQUARE, 60, 30, 100, 60, 100), sub(p, 0, 80, 2), p->v[0][CL_VOLT] = 70;
        pflt(p, FM_LADDER, 80, 70, 40, 60), fx2(p, FX_PLATE, 110, 30, 30, 50), p->g[G_AUTOGATE] = 0x01;
        break;
    case 61:                                       /* PROBE */
        noisev(p, 2, 100, 1, 20, 0, 50, 1, NM_FM, 0, 80, 60, 0, 80), p->v[2][VP_GSRC] = GS_LFOB, p->g[G_LBRATE] = 30;
        drone(p, 0, 26, CH_SUS2, CW_SINE, 90, 20, 70, 100, 110);
        fx2(p, FX_DELAY, 110, 70, 70, 55), p->g[G_AUTOGATE] = 0x01;
        break;
    case 62:                                       /* STATIC */
        noisev(p, 2, 0, 0, 120, 1, 0, 1, NM_AM, 110, 70, 60, 10, 60), p->v[2][NZ_SHAMT] = 100;
        drone(p, 0, 24, CH_MIN, CW_TRI, 70, 30, 70, 90, 110);
        fx2(p, FX_CRUSH, 40, 60, 40, 50), p->g[G_AUTOGATE] = 0x05;
        break;
    case 63:                                       /* AIRLOCK */
        noisev(p, 2, 0, 0, 1, 0, 0, 1, NM_AM, 110, 30, 100, 20, 100), drone(p, 0, 24, CH_OCT, CW_SINE, 80, 30, 60, 60, 110);
        pflt(p, FM_LADDER, 70, 80, 30, 80), fx2(p, FX_PLATE, 120, 30, 30, 60), p->g[G_AUTOGATE] = 0x05;
        break;
    /* ORGAN: organs and choirs */
    case 64:                                       /* CHAPEL */
        p->g[G_TSET] = TS_JUST, drone(p, 0, 24, CH_MAJ, CW_SQUARE, 64, 10, 90, 40, 80);
        drone(p, 1, 31, CH_FIFTH, CW_SQUARE, 66, 10, 80, 50, 80), fx2(p, FX_PLATE, 118, 40, 40, 55), p->g[G_AUTOGATE] = 0x03;
        break;
    case 65:                                       /* PIPE ORGAN */
        drone(p, 0, 24, CH_OCT, CW_SQUARE, 70, 6, 100, 30, 70), drone(p, 1, 31, CH_OCT, CW_SAW, 66, 6, 70, 30, 70);
        drone(p, 5, 36, CH_MAJ, CW_SQUARE, 70, 6, 70, 30, 70), sub(p, 0, 60, 2);
        fx2(p, FX_PLATE, 124, 30, 50, 65), p->g[G_AUTOGATE] = 0x23;
        break;
    case 66:                                       /* REED */
        drone(p, 0, 26, CH_FIFTH, CW_SQUARE, 76, 20, 90, 30, 70), pflt(p, FM_LADDER, 96, 40, 30, 20);
        fx2(p, FX_CHORUS, 40, 60, 30, 40), p->g[G_FXR] = FX_PLATE, p->g[G_BLENDR] = 50, p->g[G_AUTOGATE] = 0x01;
        break;
    case 67:                                       /* HARMONIUM */
        drone(p, 0, 26, CH_MAJ, CW_SAW, 68, 30, 90, 40, 80), drone(p, 1, 33, CH_FIFTH, CW_SAW, 66, 30, 70, 40, 80);
        fxlr(p, FX_CHORUS, 40, 70, 40, 50, FX_PLATE, 110, 40, 40, 55), p->g[G_AUTOGATE] = 0x03;
        break;
    case 68:                                       /* CHOIR AAH */
        drone(p, 0, 33, CH_MIN, CW_SAW, 74, 50, 90, 90, 100), drone(p, 1, 40, CH_SUS2, CW_SAW, 70, 50, 80, 100, 100);
        pflt(p, FM_LADDER, 100, 40, 20, 20), fxlr(p, FX_CHORUS, 30, 80, 40, 55, FX_PLATE, 120, 30, 50, 65);
        p->g[G_AUTOGATE] = 0x03;
        break;
    case 69:                                       /* CHOIR OOH */
        drone(p, 0, 33, CH_MAJ, CW_TRI, 86, 50, 90, 90, 100), drone(p, 1, 40, CH_FIFTH, CW_SINE, 90, 50, 80, 100, 100);
        fxlr(p, FX_CHORUS, 30, 80, 40, 55, FX_PLATE, 120, 30, 50, 65), p->g[G_AUTOGATE] = 0x03;
        break;
    case 70:                                       /* DRONE CHOIR */
        drone(p, 0, 24, CH_FIFTH, CW_TRI, 76, 60, 90, 100, 110), drone(p, 1, 28, CH_MIN, CW_SAW, 66, 60, 70, 110, 110);
        drone(p, 6, 31, CH_SUS2, CW_TRI, 76, 60, 70, 110, 110), fxlr(p, FX_CHORUS, 30, 80, 40, 50, FX_PLATE, 122, 30, 50, 70);
        p->g[G_AUTOGATE] = 0x43;
        break;
    case 71:                                       /* CALLIOPE */
        drone(p, 0, 36, CH_MAJ, CW_SQUARE, 84, 30, 80, 20, 60), p->v[0][VP_DMR] = 90;
        fx2(p, FX_SPRING, 90, 60, 60, 45), p->g[G_AUTOGATE] = 0x01;
        break;
    case 72:                                       /* MONKS */
        p->g[G_TSET] = TS_JUST, drone(p, 0, 19, CH_FIFTH, CW_SAW, 60, 20, 100, 90, 110), sub(p, 0, 60, 2);
        pflt(p, FM_LADDER, 90, 50, 10, 30), fx2(p, FX_PLATE, 124, 20, 60, 70), p->g[G_AUTOGATE] = 0x01;
        break;
    case 73:                                       /* CANTOR */
        p->g[G_TSET] = TS_RAST, drone(p, 0, 24, CH_FIFTH, CW_TRI, 76, 20, 90, 70, 100);
        vco(p, 3, W_SAWMORPH, 90, 4, 0, 0, 0, 40, 60, 100, 80, EG_NORM), p->g[G_VIBD] = 40, p->g[G_GLIDE] = 40;
        fx2(p, FX_PLATE, 120, 30, 50, 60), p->g[G_AUTOGATE] = 0x01;
        break;
    case 74:                                       /* VESPERS */
        drone(p, 0, 26, CH_MIN7, CW_SQUARE, 66, 20, 90, 70, 100), drone(p, 1, 33, CH_FIFTH, CW_TRI, 76, 20, 70, 80, 100);
        fx2(p, FX_PLATE, 122, 40, 60, 65), p->g[G_AUTOGATE] = 0x03;
        break;
    case 75:                                       /* LAUDS */
        drone(p, 0, 31, CH_MAJ7, CW_TRI, 84, 20, 90, 50, 90), drone(p, 1, 38, CH_SUS2, CW_SQUARE, 70, 20, 70, 60, 90);
        fx2(p, FX_PLATE, 116, 50, 40, 55), p->g[G_AUTOGATE] = 0x03;
        break;
    case 76:                                       /* TAMBURA */
        drone(p, 0, 26, CH_FIFTH, CW_SAW, 84, 20, 90, 10, 80), p->v[0][VP_DMR] = 100;
        drone(p, 1, 38, CH_OCT, CW_SAW, 80, 20, 70, 10, 80), fx2(p, FX_SPRING, 100, 50, 30, 50), p->g[G_AUTOGATE] = 0x03;
        break;
    case 77:                                       /* SRUTI BOX */
        p->g[G_TSET] = TS_JUST, drone(p, 0, 24, CH_FIFTH, CW_SQUARE, 70, 30, 90, 60, 90);
        p->v[0][VP_GSRC] = GS_LFOA, p->g[G_LARATE] = 40, p->g[G_LAWAVE] = 127;
        drone(p, 1, 36, CH_OCT, CW_TRI, 80, 20, 60, 100, 110);
        fx2(p, FX_PLATE, 110, 40, 30, 50), p->g[G_AUTOGATE] = 0x02;
        break;
    case 78:                                       /* BAGPIPE */
        drone(p, 0, 33, CH_FIFTH, CW_SQUARE, 84, 10, 90, 10, 50), drone(p, 1, 45, CH_OCT, CW_SAW, 80, 10, 60, 10, 50);
        vco(p, 3, W_PULSE, 40, 5, 0, 0, 0, 5, 60, 110, 40, EG_NORM), fx2(p, FX_PLATE, 100, 50, 20, 40), p->g[G_AUTOGATE] = 0x03;
        break;
    case 79:                                       /* ACCORDION */
        drone(p, 0, 33, CH_MAJ, CW_SQUARE, 76, 40, 80, 20, 60), p->v[0][VP_DMR] = 70;
        vco(p, 3, W_PULSE, 64, 4, 0, 0, 0, 10, 50, 100, 50, EG_NORM), vco(p, 4, W_PULSE, 40, 4, 12, 0, 0, 10, 50, 100, 50, EG_NORM);
        p->g[G_KEYS] = KM_SINGLE, fxlr(p, FX_CHORUS, 40, 60, 30, 45, FX_PLATE, 100, 50, 30, 45), p->g[G_AUTOGATE] = 0x01;
        break;
    /* PULSE: rhythm from the pulser, the LFOs and the sequencer */
    case 80:                                       /* HEARTBEAT */
        drone(p, 0, 24, CH_OCT, CW_SINE, 80, 0, 110, 0, 30), p->v[0][VP_GSRC] = GS_SEQ, pulser(p, 40, 0x05, 0, 0, 0, 0, 0, 0);
        drone(p, 6, 31, CH_FIFTH, CW_TRI, 70, 30, 50, 100, 110);
        fx2(p, FX_PLATE, 100, 40, 20, 40), p->g[G_AUTOGATE] = 0x40;
        break;
    case 81:                                       /* MOTORIK */
        drone(p, 0, 24, CH_FIFTH, CW_SAW, 76, 10, 90, 0, 30), drone(p, 1, 31, CH_FIFTH, CW_SAW, 70, 10, 70, 0, 30);
        p->v[0][VP_GSRC] = GS_SEQ, p->v[1][VP_GSRC] = GS_SEQ, pulser(p, 90, 0x1F, 1, 0, 60, 30, 90, 0);
        pflt(p, FM_LADDER, 96, 50, 60, 40), fx2(p, FX_DELAY, 60, 50, 60, 35), p->g[G_AUTOGATE] = 0x00;
        break;
    case 82:                                       /* TRAIN */
        noisev(p, 2, 30, 0, 70, 1, 20, 2, NM_AM, 70, 60, 90, 10, 50), drone(p, 0, 24, CH_FIFTH, CW_SQUARE, 60, 20, 70, 50, 90);
        fx2(p, FX_PLATE, 104, 40, 20, 40), p->g[G_AUTOGATE] = 0x05;
        break;
    case 83:                                       /* CLOCKWORK */
        noisev(p, 2, 100, 1, 80, 1, 0, 1, NM_AM, 0, 90, 60, 0, 30), vco(p, 3, W_SINE, 64, 5, 0, 0, 0, 0, 30, 0, 30, EG_NORM);
        drone(p, 1, 31, CH_FIFTH, CW_TRI, 76, 20, 60, 90, 110);
        p->g[G_ARP] = AR_UP, p->g[G_ARATE] = 0, p->g[G_ALATCH] = 1, pulser(p, 80, 0x1F, 2, 0, 0, 0, 0, 0);
        fx2(p, FX_DELAY, 50, 50, 70, 40), p->g[G_AUTOGATE] = 0x06;
        break;
    case 84:                                       /* MORSE */
        drone(p, 0, 48, CH_OCT, CW_SINE, 90, 0, 80, 0, 10), p->v[0][VP_GSRC] = GS_SEQ, pulser(p, 100, 0x0D, 2, 0, 0, 0, 0, 0);
        drone(p, 6, 24, CH_FIFTH, CW_TRI, 70, 30, 50, 100, 110);
        fx2(p, FX_DELAY, 70, 60, 60, 40), p->g[G_AUTOGATE] = 0x40;
        break;
    case 85:                                       /* PULSAR */
        drone(p, 0, 36, CH_FIFTH, CW_TRI, 90, 0, 90, 0, 40), p->v[0][VP_GSRC] = GS_LFOB, p->g[G_LBRATE] = 50;
        drone(p, 6, 24, CH_OCT, CW_SINE, 80, 30, 60, 100, 110);
        fx2(p, FX_SHIMMER, 100, 60, 40, 50), p->g[G_AUTOGATE] = 0x40;
        break;
    case 86:                                       /* TICKER */
        noisev(p, 2, 120, 1, 90, 1, 0, 1, NM_AM, 0, 100, 60, 0, 20), drone(p, 0, 26, CH_SUS2, CW_TRI, 76, 30, 70, 90, 110);
        fx2(p, FX_DELAY, 40, 40, 70, 40), p->g[G_AUTOGATE] = 0x05;
        break;
    case 87:                                       /* GALLOP */
        drone(p, 0, 24, CH_FIFTH, CW_SAW, 70, 10, 90, 0, 30), p->v[0][VP_GSRC] = GS_SEQ, pulser(p, 95, 0x0B, 0, 0, 0, 0, 0, 0);
        drone(p, 6, 31, CH_FIFTH, CW_TRI, 70, 30, 50, 100, 110);
        fx2(p, FX_PLATE, 100, 40, 20, 40), p->g[G_AUTOGATE] = 0x40;
        break;
    case 88:                                       /* DRIP */
        noisev(p, 2, 90, 1, 30, 0, 0, 1, NM_AM, 0, 90, 60, 0, 40), p->v[2][VP_GSRC] = GS_SEQ, pulser(p, 50, 0x15, 2, 0, 0, 0, 0, 0);
        drone(p, 0, 24, CH_OCT, CW_SINE, 80, 20, 60, 100, 110);
        fx2(p, FX_RESO, 110, 70, 64, 60), p->g[G_FXR] = FX_PLATE, p->g[G_BLENDR] = 50, p->g[G_AUTOGATE] = 0x01;
        break;
    case 89:                                       /* SONAR */
        drone(p, 0, 48, CH_OCT, CW_SINE, 100, 0, 80, 0, 60), p->v[0][VP_GSRC] = GS_LFOA, p->g[G_LARATE] = 70;
        drone(p, 1, 24, CH_FIFTH, CW_SINE, 90, 20, 60, 100, 110);
        fx2(p, FX_DELAY, 110, 80, 40, 60), p->g[G_AUTOGATE] = 0x02;
        break;
    case 90:                                       /* PING PONG */
        vco(p, 3, W_SINTRI, 30, 5, 0, 0, 0, 0, 40, 0, 40, EG_NORM), p->g[G_ARP] = AR_UPDOWN, p->g[G_ARATE] = 90, p->g[G_AOCT] = 2;
        p->g[G_ALATCH] = 1, drone(p, 0, 24, CH_FIFTH, CW_TRI, 80, 20, 60, 90, 110);
        fxlr(p, FX_DELAY, 60, 60, 60, 45, FX_DELAY, 80, 60, 60, 45), p->g[G_AUTOGATE] = 0x01;
        break;
    case 91:                                       /* TRANCE */
        drone(p, 0, 33, CH_MIN, CW_SAW, 80, 20, 90, 0, 30), drone(p, 1, 28, CH_SUS2, CW_SAW, 76, 20, 70, 0, 30);
        p->v[0][VP_GSRC] = GS_SEQ, p->v[1][VP_GSRC] = GS_LFOB, p->g[G_LBRATE] = 70, pulser(p, 100, 0x1F, 2, 0, 0, 0, 0, 0);
        pflt(p, FM_LADDER, 100, 60, 70, 50), fx2(p, FX_DELAY, 60, 50, 60, 40), p->g[G_AUTOGATE] = 0x00;
        break;
    case 92:                                       /* STEPPER */
        drone(p, 0, 24, CH_FIFTH, CW_SQUARE, 70, 10, 90, 0, 40), p->v[0][VP_GSRC] = GS_SEQ, p->v[0][VP_MSRC] = MS_SEQ;
        p->v[0][VP_MAMT] = 70, pulser(p, 70, 0x1F, 2, 0, 30, 60, 90, 120);
        drone(p, 6, 24, CH_OCT, CW_TRI, 70, 30, 50, 100, 110);
        fx2(p, FX_DELAY, 60, 50, 60, 40), p->g[G_AUTOGATE] = 0x40;
        break;
    case 93:                                       /* LOCOMOTIVE */
        noisev(p, 2, 20, 0, 60, 1, 30, 4, NM_AM, 80, 50, 90, 10, 50), drone(p, 0, 12, CH_FIFTH, CW_SAW, 64, 20, 80, 60, 100);
        sub(p, 0, 70, 2), fx2(p, FX_PLATE, 104, 30, 20, 40), p->g[G_AUTOGATE] = 0x05;
        break;
    case 94:                                       /* BREATH */
        noisev(p, 2, 0, 0, 0, 0, 0, 1, NM_DRONE, 90, 50, 90, 90, 100), p->v[2][VP_GSRC] = GS_LFOA, p->g[G_LARATE] = 35;
        drone(p, 0, 24, CH_OCT, CW_SINE, 90, 20, 60, 110, 110), fx2(p, FX_PLATE, 112, 40, 30, 55), p->g[G_AUTOGATE] = 0x01;
        break;
    case 95:                                       /* ECHO PULSE */
        drone(p, 0, 36, CH_MAJ, CW_TRI, 86, 0, 90, 0, 40), p->v[0][VP_GSRC] = GS_SEQ, pulser(p, 60, 0x11, 2, 0, 0, 0, 0, 0);
        drone(p, 6, 24, CH_FIFTH, CW_SINE, 80, 20, 50, 100, 110);
        fxlr(p, FX_DELAY, 90, 80, 60, 60, FX_DELAY, 100, 80, 60, 60), p->g[G_AUTOGATE] = 0x40;
        break;
    default:
        break;
    }
}

static void preset_make(uint32_t n, patch_t *p)
{
    uint32_t t = (n / PRESET_PAGE) % NTHEME, i = n % PRESET_PAGE;
    patch_init(p);
    if (t == 0u && i < 14u)
        preset_old(i, p);
    else if (t == 1u && i < 10u)
        preset_old(14u + i, p);
    else
        theme_more(t * PRESET_PAGE + i, p);
    {   /* (all in range) */
        uint32_t v, k;
        for (v = 0; v < NV; v++)
            for (k = 0; k < VP_N; k++)
                p->v[v][k] = VDESC[VTYPE[v]][k].label ? vp_clamp(v, k, p->v[v][k]) : 0;
        for (k = 0; k < G_COUNT; k++)
            p->g[k] = (int16_t)clamp(p->g[k], GDESC[k].min, GDESC[k].max);
    }
}
