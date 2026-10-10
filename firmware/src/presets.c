/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Factory presets: the power-on sound with changes. AUTO: the voices that start when a preset is chosen. */
#define NPRESET 14u
static const char *const PRESET_NAME[NPRESET] = {
    "DEEP SPACE", "BLADE RUN", "MONASTERY", "GAMELAN", "MAQAM", "SIRENS", "SURF+WIND", "DARK MATTER",
    "STAR WIND", "CATHEDRAL", "PULSE", "SCI-FI LAB", "QUARTERS", "INIT"};

static void chord(patch_t *p, uint32_t v, int32_t m0, int32_t m1, int32_t m2, int32_t m3, int32_t m4)   /* MIDI notes */
{
    const int32_t m[5] = {m0, m1, m2, m3, m4};
    uint32_t g;
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

static void preset_make(uint32_t n, patch_t *p)
{
    patch_init(p);
    switch (n) {
    case 0:                                        /* DEEP SPACE: low minor drones swelling, a long plate */
        chord(p, 0, 21, 40, 52, 67, 88);
        chord(p, 1, 28, 43, 59, 71, 86);
        env(p, 0, 90, 100), env(p, 1, 100, 100);
        p->v[0][VP_TONE] = p->v[1][VP_TONE] = 70;
        fx2(p, FX_PLATE, 118, 50, 40, 70);
        p->g[G_DRIFT] = 60;
        p->g[G_AUTOGATE] = 0x03;
        break;
    case 1:                                        /* BLADE RUN: wide detuned minor 9, slow swells, VCO lead */
        chord(p, 0, 21, 40, 60, 71, 83);
        chord(p, 5, 33, 45, 55, 64, 79);
        env(p, 0, 105, 110), env(p, 5, 110, 110);
        p->v[0][VP_TONE] = p->v[5][VP_TONE] = 76;
        p->v[3][VC_WAVE] = W_SAW, p->v[4][VC_WAVE] = W_SAW, p->v[4][VC_TUNE] = 12;
        p->v[3][VP_ATT] = p->v[4][VP_ATT] = 50, p->v[3][VP_RLS] = p->v[4][VP_RLS] = 90;
        p->g[G_GLIDE] = 60, p->g[G_VIBD] = 24, p->g[G_VIBR] = 40, p->g[G_KEYS] = KM_SINGLE;
        p->g[G_WIDTH] = 40, p->g[G_DRIFT] = 70;
        fx2(p, FX_PLATE, 120, 40, 60, 75);
        p->g[G_AUTOGATE] = 0x21;
        break;
    case 2:                                        /* MONASTERY: just fifths and octaves, low, a breath of wind */
        p->g[G_TSET] = TS_JUST;
        chord(p, 0, 24, 43, 55, 67, 79);
        chord(p, 1, 31, 36, 60, 72, -1);
        env(p, 0, 80, 100), env(p, 1, 95, 100);
        p->v[0][VP_TONE] = 62, p->v[1][VP_TONE] = 66;
        p->v[7][NZ_PITCH] = 0, p->v[7][NZ_NOISE] = 70, p->v[7][NZ_MODE] = NM_AM, p->v[7][NZ_RATE] = 4;
        p->v[7][VP_VOL] = 60, p->v[7][VP_TONE] = 50;
        fx2(p, FX_PLATE, 124, 30, 70, 80);
        p->g[G_AUTOGATE] = 0x83;
        break;
    case 3:                                        /* GAMELAN: pelog, the keys arpeggiate, a spring */
        p->g[G_TSET] = TS_PELOG;
        chord(p, 0, 24, 43, 55, 67, 79);
        p->v[0][VP_VOL] = 70;
        p->v[3][VC_WAVE] = W_SINTRI, p->v[3][VC_SHAPE] = 30, p->v[4][VC_WAVE] = W_SINE;
        p->v[3][VP_ATT] = p->v[4][VP_ATT] = 0, p->v[3][VC_DEC] = p->v[4][VC_DEC] = 55, p->v[3][VC_SUS] = p->v[4][VC_SUS] = 20;
        p->v[3][VP_RLS] = p->v[4][VP_RLS] = 70;
        p->g[G_ARP] = AR_UPDOWN, p->g[G_ARATE] = 78, p->g[G_AOCT] = 2, p->g[G_ALATCH] = 1;
        fx2(p, FX_SPRING, 100, 50, 40, 55);
        p->g[G_AUTOGATE] = 0x01;
        break;
    case 4:                                        /* MAQAM: Rast, a C-G drone, a singing VCO */
        p->g[G_TSET] = TS_RAST;
        chord(p, 0, 24, 43, 55, -1, -1);
        chord(p, 1, 36, 52, 60, 64, -1);
        env(p, 0, 70, 90), env(p, 1, 80, 90);
        p->v[3][VC_WAVE] = W_SAWMORPH, p->v[3][VC_SHAPE] = 90, p->v[3][VP_ATT] = 30, p->v[3][VP_RLS] = 70;
        p->g[G_VIBD] = 40, p->g[G_VIBR] = 55, p->g[G_GLIDE] = 40, p->g[G_KEYS] = KM_SINGLE;
        fx2(p, FX_PLATE, 105, 60, 30, 55);
        p->g[G_AUTOGATE] = 0x03;
        break;
    case 5:                                        /* SIRENS */
        p->v[2][NZ_PITCH] = 80, p->v[2][NZ_RATE] = 30, p->v[2][NZ_FMAMT] = 90, p->v[2][NZ_DIV] = 1, p->v[2][NZ_MODE] = NM_FM;
        p->v[7][NZ_PITCH] = 100, p->v[7][NZ_RANGE] = 1, p->v[7][NZ_RATE] = 60, p->v[7][NZ_FMAMT] = 70;
        p->v[7][NZ_DIV] = 5, p->v[7][NZ_MODE] = NM_FMAM, p->v[7][NZ_NOISE] = 0, p->v[7][VP_VOL] = 70;
        env(p, 2, 40, 80), env(p, 7, 40, 80);
        fx2(p, FX_DELAY, 90, 70, 60, 45);
        p->g[G_AUTOGATE] = 0x84;
        break;
    case 6:                                        /* SURF + WIND */
        p->v[2][NZ_PITCH] = 0, p->v[2][NZ_NOISE] = 100, p->v[2][NZ_MODE] = NM_AM, p->v[2][NZ_RATE] = 2, p->v[2][VP_TONE] = 60;
        p->v[7][NZ_PITCH] = 0, p->v[7][NZ_NOISE] = 90, p->v[7][NZ_MODE] = NM_DRONE, p->v[7][VP_TONE] = 40;
        p->v[7][VP_MSRC] = MS_LFOA, p->v[7][VP_MAMT] = 0;
        env(p, 2, 90, 100), env(p, 7, 100, 100);
        fx2(p, FX_PLATE, 115, 40, 20, 60);
        p->g[G_AUTOGATE] = 0x84;
        break;
    case 7:                                        /* DARK MATTER: low FM drones through a resonant ladder */
        chord(p, 0, 24, 36, 55, -1, -1);
        chord(p, 6, 26, 41, 53, 65, -1);
        p->v[0][CL_VOLT] = 92, p->v[6][CL_VOLT] = 80;
        env(p, 0, 70, 100), env(p, 6, 90, 100);
        p->g[G_FFL] = 70, p->g[G_FFR] = 66, p->g[G_FRL] = 70, p->g[G_FRR] = 76, p->g[G_FDRIVE] = 40;
        p->g[G_FCVAMT] = 40;
        fx2(p, FX_PLATE, 110, 30, 50, 50);
        p->g[G_AUTOGATE] = 0x41;
        break;
    case 8:                                        /* STAR WIND: slendro, generators wobbling with LFO A */
        p->g[G_TSET] = TS_SLENDRO;
        chord(p, 1, 24, 41, 55, 67, 81);
        chord(p, 5, 28, 45, 52, 71, 84);
        p->v[1][CL_MOD] = 0x1C, p->v[1][VP_MSRC] = MS_LFOA, p->v[1][VP_MAMT] = 28;
        p->v[5][CL_MOD] = 0x0E, p->v[5][VP_MSRC] = MS_DRIFT, p->v[5][VP_MAMT] = 40;
        env(p, 1, 85, 100), env(p, 5, 95, 100);
        p->g[G_LARATE] = 30;
        fx2(p, FX_SHIMMER, 100, 50, 60, 50);
        p->g[G_AUTOGATE] = 0x22;
        break;
    case 9:                                        /* CATHEDRAL: all four chords, a huge plate */
        env(p, 0, 80, 110), env(p, 1, 90, 110), env(p, 5, 100, 110), env(p, 6, 110, 110);
        fx2(p, FX_PLATE, 126, 30, 80, 85);
        p->g[G_MASTER] = 92;
        p->g[G_AUTOGATE] = 0x63;
        break;
    case 10:                                       /* PULSE: the sequencer gates two drones, a delay */
        p->v[0][VP_GSRC] = p->v[1][VP_GSRC] = GS_SEQ;
        env(p, 0, 20, 50), env(p, 1, 30, 60);
        p->g[G_RUN] = 1, p->g[G_PULSER] = 60, p->g[G_SGATES] = 0x1B;
        p->v[3][VP_MSRC] = MS_SEQ, p->v[3][VP_MAMT] = 60;
        fx2(p, FX_DELAY, 70, 60, 50, 40);
        p->g[G_AUTOGATE] = 0x00;
        break;
    case 11:                                       /* SCI-FI LAB: looping morph VCOs, S&H noise, ring */
        p->v[3][VC_WAVE] = W_SAWMORPH, p->v[3][VC_EG] = EG_LOOP, p->v[3][VC_DEC] = 50, p->v[3][VP_ATT] = 20;
        p->v[3][VP_MSRC] = MS_LFOB, p->v[3][VP_MAMT] = 30;
        p->v[2][NZ_PITCH] = 60, p->v[2][NZ_SHAMT] = 90, p->v[2][NZ_SHCLK] = 2, p->v[2][NZ_MODE] = NM_AM;
        p->v[2][NZ_RATE] = 80, p->v[2][NZ_MULT] = 1, p->v[2][VP_VOL] = 70;
        p->g[G_LBRATE] = 70;
        p->g[G_FXL] = FX_RING, p->g[G_FXLX] = 40, p->g[G_FXLY] = 60, p->g[G_BLEND] = 50;
        p->g[G_FXR] = FX_PLATE, p->g[G_FXRX] = 100, p->g[G_BLENDR] = 50;
        p->g[G_AUTOGATE] = 0x04;
        break;
    case 12:                                       /* QUARTERS: 24-TET clusters, shimmer */
        p->g[G_TSET] = TS_24TET;
        chord(p, 0, 24, 43, 55, 67, 79);
        p->v[0][CL_TUNE1 + 2] += 50, p->v[0][CL_TUNE1 + 4] += 50;
        chord(p, 6, 31, 38, 62, 74, 86);
        p->v[6][CL_TUNE1 + 1] += 50, p->v[6][CL_TUNE1 + 3] += 50;
        env(p, 0, 90, 100), env(p, 6, 100, 100);
        fx2(p, FX_SHIMMER, 110, 60, 50, 60);
        p->g[G_AUTOGATE] = 0x41;
        break;
    default:
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
