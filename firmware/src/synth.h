/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Makkuro37 data model: 8 voices, their parameters, the global ones. */
#include <stdint.h>
#define NV 8
enum { VT_CLASSIC, VT_NOISE, VT_VCO };
static const uint8_t VTYPE[NV] = {VT_CLASSIC, VT_CLASSIC, VT_NOISE, VT_VCO, VT_VCO, VT_CLASSIC, VT_CLASSIC, VT_NOISE};

/* per-voice parameters: T0..T9 by type, then the common ones */
enum { VP_T0, VP_T1, VP_T2, VP_T3, VP_T4, VP_T5, VP_T6, VP_T7, VP_T8, VP_T9,
       VP_ATT, VP_RLS, VP_MSRC, VP_MAMT, VP_GSRC, VP_VOL, VP_PAN, VP_N };
/* classic: five saws (cents above the generator's lowest note), VOLT, MUTE / MOD bit masks */
enum { CL_TUNE1 = VP_T0, CL_VOLT = VP_T5, CL_MUTE = VP_T6, CL_MOD = VP_T7 };
/* noise ("Papa Srapa"): tone + square modulator */
enum { NZ_PITCH = VP_T0, NZ_RANGE, NZ_RATE, NZ_MULT, NZ_FMAMT, NZ_DIV, NZ_MODE, NZ_NOISE, NZ_SHAMT, NZ_SHCLK };
enum { NM_DRONE, NM_FM, NM_AM, NM_FMAM };
/* VCO: AS3340-like, six waves */
enum { VC_WAVE = VP_T0, VC_SHAPE, VC_OCT, VC_TUNE, VC_FM, VC_SYNC, VC_DEC, VC_SUS, VC_EG };
enum { W_SAW, W_PULSE, W_TRI, W_SINE, W_SAWMORPH, W_SINTRI };
enum { EG_NORM, EG_HOLD, EG_LOOP };

/* modulation sources (VP_MSRC, filter CV, ...) and gate sources (VP_GSRC) */
enum { MS_NONE, MS_LFOA, MS_LFOB, MS_JOYX, MS_JOYY, MS_SEQ, MS_SH3, MS_SH8, MS_DRIFT, MS_N };
enum { GS_PAD, GS_SEQ, GS_LFOA, GS_LFOB, GS_N };

enum {                          /* global parameters */
    G_MASTER, G_TSET, G_ROOT, G_SNAP, G_FINE, G_GLIDE, G_KEYS,
    G_LARATE, G_LAWAVE, G_LBRATE, G_LBWAVE, G_JXOFS, G_JYOFS,
    G_SEQ1, G_SEQ2, G_SEQ3, G_SEQ4, G_SEQ5, G_SGATES, G_STAGES, G_PULSER, G_RUN,
    G_FFL, G_FRL, G_FFR, G_FRR, G_FLINK, G_FDRIVE, G_FCVSRC, G_FCVAMT,
    G_FXL, G_FXR, G_FXLX, G_FXLY, G_FXLZ, G_FXRX, G_FXRY, G_FXRZ, G_BLEND,
    G_COUNT
};
enum { FX_OFF, FX_REVERB, FX_DELAY, FX_SHIMMER, FX_CHORUS, FX_PHASER, FX_CRUSH, FX_RING, FX_FREEZE, FX_N };

typedef struct {                /* a scene: what the sound is */
    int16_t v[NV][VP_N];
    int16_t g[G_COUNT];
} patch_t;

typedef struct {                /* run-time state of a voice (audio side) */
    uint32_t ph[5], inc[5];
    int32_t last[5];
    int32_t env;                /* Q24 */
    uint8_t stage;              /* 0 idle, 1 attack, 2 decay / sustain, 3 release */
    uint8_t gate;               /* last gate seen */
    uint8_t hits;               /* gate triggers so far (the screen's hammers) */
    int32_t gl, gr;             /* gains now (Q15), ramped per sample */
    int32_t level;              /* output level for the screen and the pads, Q15 */
    /* noise voice */
    uint32_t mph, minc;
    uint32_t nz;
    int32_t lag, am, sh;
    uint8_t cnt;
    int32_t kv[6];              /* per-tick constants of the render (by type) */
    /* VCO */
    int32_t pitch;              /* cents x 16, gliding */
    uint8_t trig;               /* key triggers seen */
} vstate_t;

/* tuning sets */
enum { TS_12TET, TS_JUST, TS_PYTH, TS_24TET, TS_RAST, TS_SLENDRO, TS_PELOG, TS_N };
