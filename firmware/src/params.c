/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Parameter descriptors (range, default, how a value reads) and the power-on sound. */
enum { F_NUM, F_ENUM, F_ONOFF, F_PAN, F_TUNE, F_CENTS, F_BITS, F_SIGNED };
typedef struct {
    const char *label;
    int16_t min, max, def;
    uint8_t fmt;
    const char *const *names;
} pdesc_t;

static const char *const N_MSRC[MS_N] = {"OFF", "LFO A", "LFO B", "JOY X", "JOY Y", "SEQ", "S&H 3", "S&H 8", "DRIFT"};
static const char *const N_GSRC[GS_N] = {"PAD", "SEQ", "LFO A", "LFO B", "CASCADE"};
static const char *const N_RANGE[2] = {"LO", "HI"};
static const char *const N_MULT[2] = {"X1", "X10"};
static const char *const N_NMODE[4] = {"DRONE", "FM", "AM", "FM+AM"};
static const char *const N_SHCLK[4] = {"MOD", "LFO A", "LFO B", "PULSER"};
static const char *const N_WAVE[6] = {"SAW", "PULSE", "TRI", "SINE", "SAW<>", "SIN<>TRI"};
static const char *const N_OCT[8] = {"LO", "64'", "32'", "16'", "8'", "4'", "2'", "1'"};
static const char *const N_EG[3] = {"GATE", "HOLD", "LOOP"};
static const char *const N_TSET[TS_N] = {"12-TET", "JUST", "PYTHAG", "24-TET", "RAST", "SLENDRO", "PELOG"};
static const char *const N_KEYS[3] = {"SINGLE", "DUO", "SPLIT"};
static const char *const N_ARP[6] = {"OFF", "UP", "DOWN", "UP-DN", "RANDOM", "SEQ"};
static const char *const N_STAGES[3] = {"3", "4", "5"};
static const char *const N_FX[FX_N] = {"OFF", "ROOM", "DELAY", "SHIMMER", "CHORUS", "PHASER", "CRUSH", "RING", "FREEZE",
                                       "SPRING", "PLATE", "RESO"};
static const char *const N_FMODE[4] = {"LADDER", "POLIVOKS", "BAND", "DIST"};
#define NOTE12 NOTE_NAMES

/* per voice type; F_TUNE: cents above the generator's lowest note (min / max by generator) */
static const pdesc_t VDESC[3][VP_N] = {
    [VT_CLASSIC] = {
        {"TUNE 1", 0, 3100, 1200, F_TUNE, 0}, {"TUNE 2", 0, 2000, 800, F_TUNE, 0},
        {"TUNE 3", 0, 2100, 200, F_TUNE, 0}, {"TUNE 4", 0, 2000, 300, F_TUNE, 0},
        {"TUNE 5", 0, 1900, 500, F_TUNE, 0}, {"VOLT", 0, 127, 0, F_NUM, 0},
        {"MUTE", 0, 31, 0, F_BITS, 0}, {"MOD", 0, 31, 0, F_BITS, 0}, {"SUB", 0, 127, 0, F_NUM, 0},
        {"SUB DIV", 2, 8, 3, F_NUM, 0},
        {"ATTACK", 0, 127, 50, F_NUM, 0}, {"RELEASE", 0, 127, 64, F_NUM, 0},
        {"MOD SRC", 0, MS_N - 1, MS_DRIFT, F_ENUM, N_MSRC}, {"MOD AMT", 0, 127, 0, F_NUM, 0},
        {"GATE", 0, GS_N - 1, GS_PAD, F_ENUM, N_GSRC}, {"VOLUME", 0, 127, 96, F_NUM, 0},
        {"PAN", 0, 127, 64, F_PAN, 0}, {"TONE", 0, 127, 84, F_NUM, 0},
        {"PITCH", -2400, 2400, 0, F_CENTS, 0}},
    [VT_NOISE] = {
        {"PITCH", 0, 127, 70, F_NUM, 0}, {"RANGE", 0, 1, 0, F_ENUM, N_RANGE},
        {"RATE", 0, 127, 40, F_NUM, 0}, {"RATE X", 0, 1, 0, F_ENUM, N_MULT},
        {"FM AMT", 0, 127, 60, F_NUM, 0}, {"DIVIDER", 1, 8, 1, F_NUM, 0},
        {"MODE", 0, 3, NM_FM, F_ENUM, N_NMODE}, {"NOISE", 0, 127, 0, F_NUM, 0},
        {"S&H AMT", 0, 127, 0, F_NUM, 0}, {"S&H CLK", 0, 3, 0, F_ENUM, N_SHCLK},
        {"ATTACK", 0, 127, 50, F_NUM, 0}, {"RELEASE", 0, 127, 64, F_NUM, 0},
        {"MOD SRC", 0, MS_N - 1, MS_NONE, F_ENUM, N_MSRC}, {"MOD AMT", 0, 127, 0, F_NUM, 0},
        {"GATE", 0, GS_N - 1, GS_PAD, F_ENUM, N_GSRC}, {"VOLUME", 0, 127, 80, F_NUM, 0},
        {"PAN", 0, 127, 64, F_PAN, 0}, {"TONE", 0, 127, 76, F_NUM, 0},
        {"PITCH", -2400, 2400, 0, F_CENTS, 0}},
    [VT_VCO] = {
        {"WAVE", 0, 5, W_SAW, F_ENUM, N_WAVE}, {"SHAPE", 0, 127, 64, F_NUM, 0},
        {"OCTAVE", 0, 7, 4, F_ENUM, N_OCT}, {"TUNE", -600, 600, 0, F_CENTS, 0},
        {"FM", 0, 127, 0, F_NUM, 0}, {"SYNC", 0, 1, 0, F_ONOFF, 0},
        {"DECAY", 0, 127, 70, F_NUM, 0}, {"SUSTAIN", 0, 127, 100, F_NUM, 0},
        {"EG", 0, 2, EG_NORM, F_ENUM, N_EG}, {"SUB", 0, 127, 0, F_NUM, 0},
        {"ATTACK", 0, 127, 10, F_NUM, 0}, {"RELEASE", 0, 127, 60, F_NUM, 0},
        {"MOD SRC", 0, MS_N - 1, MS_NONE, F_ENUM, N_MSRC}, {"MOD AMT", 0, 127, 0, F_NUM, 0},
        {"GATE", 0, GS_N - 1, GS_PAD, F_ENUM, N_GSRC}, {"VOLUME", 0, 127, 96, F_NUM, 0},
        {"PAN", 0, 127, 64, F_PAN, 0}, {"TONE", 0, 127, 100, F_NUM, 0},
        {"PITCH", -2400, 2400, 0, F_CENTS, 0}},
};

static const pdesc_t GDESC[G_COUNT] = {
    [G_MASTER] = {"MASTER", 0, 127, 100, F_NUM, 0},
    [G_TSET] = {"TUNING", 0, TS_N - 1, TS_12TET, F_ENUM, N_TSET},
    [G_ROOT] = {"ROOT", 0, 11, 0, F_ENUM, NOTE12},
    [G_SNAP] = {"SNAP", 0, 1, 1, F_ONOFF, 0},
    [G_FINE] = {"FINE", -100, 100, 0, F_SIGNED, 0},
    [G_GLIDE] = {"GLIDE", 0, 127, 20, F_NUM, 0},
    [G_KEYS] = {"KEYS", 0, 2, 1, F_ENUM, N_KEYS},
    [G_LARATE] = {"A RATE", 0, 127, 40, F_NUM, 0},
    [G_LAWAVE] = {"A WAVE", 0, 127, 127, F_NUM, 0},
    [G_LBRATE] = {"B RATE", 0, 127, 60, F_NUM, 0},
    [G_LBWAVE] = {"B WAVE", 0, 127, 127, F_NUM, 0},
    [G_JXOFS] = {"X OFFS", -64, 64, 0, F_SIGNED, 0},
    [G_JYOFS] = {"Y OFFS", -64, 64, 0, F_SIGNED, 0},
    [G_SEQ1] = {"STEP 1", 0, 127, 0, F_NUM, 0},
    [G_SEQ2] = {"STEP 2", 0, 127, 32, F_NUM, 0},
    [G_SEQ3] = {"STEP 3", 0, 127, 64, F_NUM, 0},
    [G_SEQ4] = {"STEP 4", 0, 127, 96, F_NUM, 0},
    [G_SEQ5] = {"STEP 5", 0, 127, 127, F_NUM, 0},
    [G_SGATES] = {"GATES", 0, 31, 31, F_BITS, 0},
    [G_STAGES] = {"STAGES", 0, 2, 2, F_ENUM, N_STAGES},
    [G_PULSER] = {"PULSER", 0, 127, 50, F_NUM, 0},
    [G_RUN] = {"RUN", 0, 1, 0, F_ONOFF, 0},
    [G_FFL] = {"FREQ L", 0, 127, 127, F_NUM, 0},           /* open: the mod wheel closes it (a dark swell) */
    [G_FRL] = {"RES L", 0, 127, 0, F_NUM, 0},
    [G_FFR] = {"FREQ R", 0, 127, 127, F_NUM, 0},
    [G_FRR] = {"RES R", 0, 127, 0, F_NUM, 0},
    [G_FLINK] = {"LINK", 0, 1, 0, F_ONOFF, 0},
    [G_FDRIVE] = {"DRIVE", 0, 127, 0, F_NUM, 0},
    [G_FCVSRC] = {"CV SRC", 0, MS_N - 1, MS_JOYY, F_ENUM, N_MSRC},
    [G_FCVAMT] = {"CV AMT", -64, 64, -48, F_SIGNED, 0},
    [G_FXL] = {"FX L", 0, FX_N - 1, FX_PLATE, F_ENUM, N_FX},
    [G_FXR] = {"FX R", 0, FX_N - 1, FX_PLATE, F_ENUM, N_FX},
    [G_FXLX] = {"L X", 0, 127, 84, F_NUM, 0},
    [G_FXLY] = {"L Y", 0, 127, 60, F_NUM, 0},
    [G_FXLZ] = {"L Z", 0, 127, 50, F_NUM, 0},
    [G_FXRX] = {"R X", 0, 127, 84, F_NUM, 0},
    [G_FXRY] = {"R Y", 0, 127, 60, F_NUM, 0},
    [G_FXRZ] = {"R Z", 0, 127, 50, F_NUM, 0},
    [G_BLEND] = {"BLEND L", 0, 127, 48, F_NUM, 0},
    [G_DRIFT] = {"ANALOG", 0, 127, 40, F_NUM, 0},
    [G_WIDTH] = {"WIDTH", 0, 60, 24, F_NUM, 0},
    [G_FMODE] = {"FILTER", 0, 3, 0, F_ENUM, N_FMODE},
    [G_ARP] = {"ARP", 0, 5, AR_OFF, F_ENUM, N_ARP},
    [G_ARATE] = {"RATE", 0, 127, 70, F_NUM, 0},
    [G_AOCT] = {"OCTAVES", 1, 3, 1, F_NUM, 0},
    [G_ALATCH] = {"LATCH", 0, 1, 0, F_ONOFF, 0},
    [G_VIBD] = {"VIBRATO", 0, 127, 0, F_NUM, 0},
    [G_VIBR] = {"VIB RATE", 0, 127, 50, F_NUM, 0},
    [G_BLENDR] = {"BLEND R", 0, 127, 48, F_NUM, 0},
    [G_AUTOGATE] = {"AUTO", 0, 255, 0, F_NUM, 0},
    [G_TAPE] = {"TAPE", 0, 127, 24, F_NUM, 0},
};

/* the power-on sound: four chords on the classic voices, a siren and a wind, two VCOs */
static void patch_init(patch_t *p)
{
    static const int16_t CHORD[4][5] = {{1200, 800, 200, 300, 500},     /* C1 G2 E3 G4 C6 */
                                        {900, 500, 700, 800, 900},      /* A0 E2 A3 C5 E6 */
                                        {1700, 1300, 300, 500, 500},    /* F1 C3 F3 A4 C6 */
                                        {1900, 1500, 500, 700, 700}};   /* G1 D3 G3 B4 D6 */
    static const uint8_t PAN[NV] = {40, 88, 30, 56, 72, 24, 104, 98};
    uint32_t v, k, c = 0;
    for (v = 0; v < NV; v++) {
        const pdesc_t *d = VDESC[VTYPE[v]];
        for (k = 0; k < VP_N; k++)
            p->v[v][k] = d[k].def;
        p->v[v][VP_PAN] = PAN[v];
        if (VTYPE[v] == VT_CLASSIC) {
            for (k = 0; k < 5u; k++)
                p->v[v][CL_TUNE1 + k] = CHORD[c][k];
            c++;
        }
    }
    p->v[7][NZ_PITCH] = 0;                     /* voice 8: wind (noise, slow AM) */
    p->v[7][NZ_NOISE] = 110;
    p->v[7][NZ_MODE] = NM_AM;
    p->v[7][NZ_RATE] = 10;
    p->v[4][VC_WAVE] = W_PULSE;                /* voice 5: a pulse a few cents above voice 4 */
    p->v[4][VC_TUNE] = 7;
    for (k = 0; k < G_COUNT; k++)
        p->g[k] = GDESC[k].def;
}

static int16_t vp_clamp(uint32_t v, uint32_t k, int32_t x)
{
    const pdesc_t *d = &VDESC[VTYPE[v]][k];
    return (int16_t)clamp(x, d->min, d->max);
}
