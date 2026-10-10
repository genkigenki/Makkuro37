/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Factory presets in two themes (pages of 14 on the pads): DRONES (dark textures) and ABRAXAS
 * (after the Horn of Abraxas set). Each the power-on sound with changes; AUTO: the voices it wakes. */
#define NPRESET 24u
#define PRESET_PAGE 14u
static const char *const THEME_NAME[2] = {"DRONES", "ABRAXAS"};
static const char *const PRESET_NAME[NPRESET] = {
    "LAYERED BED", "BLADE RUN", "UNSTABLE", "MICRO CLUSTER", "NOISE ORGANIC", "EERIE SWELL", "EVOLVING",
    "DEEP SPACE", "MONASTERY", "GAMELAN", "MAQAM", "SIRENS", "PULSE", "INIT",
    "MANDY DRONE", "CORRODED BELL", "DEEP VOID", "MACHINERY", "RITUAL PAD", "STRING GHOST", "STAR WIND",
    "SCI-FI LAB", "QUARTERS", "CATHEDRAL"};

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

static void preset_make(uint32_t n, patch_t *p)
{
    patch_init(p);
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
        voice(p, 0, CW_SAW, 70, 30, 20, 84), voice(p, 6, CW_SAW, 64, 36, 26, 76);
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
        voice(p, 2, CW_SQUARE, 70, 0, 50, 76), voice(p, 7, CW_SQUARE, 60, 0, 50, 60);
        p->v[0][VP_GSRC] = GS_SEQ, p->v[0][VP_VOL] = 76;
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
    {   /* (all in range) */
        uint32_t v, k;
        for (v = 0; v < NV; v++)
            for (k = 0; k < VP_N; k++)
                p->v[v][k] = VDESC[VTYPE[v]][k].label ? vp_clamp(v, k, p->v[v][k]) : 0;
        for (k = 0; k < G_COUNT; k++)
            p->g[k] = (int16_t)clamp(p->g[k], GDESC[k].min, GDESC[k].max);
    }
}
