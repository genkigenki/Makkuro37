/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Controls: one logic for everything. The screen is laid out as the panel, K1..K8 above the 8 pad columns, so
 * column i is voice i everywhere.
 *   HOME: Ki = voice i's pitch, top pad i its gate; bottom pads KB (keyboard), P (pulser), H (hold all), C (chaos).
 *   KB: select voices, then the keys tune them (keys held together: spread over them); a key while a voice pad
 *   is held does the same.
 *   A button opens its menu at page 1; < > turn the pages; the same button again, or PATCH, goes home. On every
 *   page knob Ki sets column i; a value with two choices (or on / off) sits on the column's top / bottom pad.
 *   KNOB BANK: PRESETS (held: HELP; there K1-K8 are filters). PATCH: HOME (held: SETUP).
 *   FADER BANK: the faders' banks (< > or the top pads; the knobs stay the voices' pitch).
 * One table (PAGES / MENUS below) drives the menus, the pads, HELP and the docs. */
enum { MN_HOME, MN_SOUND, MN_ENV, MN_MOTION, MN_TUNING, MN_FX, MN_SEQ, MN_KEYS, MN_MIX, MN_SETUP, MN_PRESETS, MN_FADERS, MN_N };
/* button ids (smk_input.h) */
enum { B_KNOBBANK, B_FADERBANK, B_LEFT, B_RIGHT, B_OCTDN, B_OCTUP, B_ARP, B_REPEAT, B_SCALE, B_CHORD, B_GLOBE,
       B_BT, B_PATCH, B_PARA, B_FX, B_SEQ, B_PLAY, B_REC };

/* ------------------------------------------------------------- the table --- */
#define NO 0xFFu
enum { PG_ROW, PG_GLOB, PG_VOICE };                    /* a row of one voice parameter / 8 globals / one voice */
enum { X_LIGHT = 0xE0, X_CONSOLE, X_OUTPUT, X_LOOP, X_STEPREC };   /* (PG_GLOB: settings and pad actions) */
typedef struct {
    const char *name;
    uint8_t kind;
    uint8_t id[8];       /* ROW: the parameter by voice type (classic, noise, VCO); GLOB: the 8 columns; VOICE: v */
    uint8_t alt;         /* ROW: the top pad switches the knob to this one (DETUNE: its RATE) */
    uint8_t bits;        /* GLOB: columns 1..5's top pads toggle bit i of this global (the sequencer's steps) */
    const char *help;    /* two lines (\n) under the page: what it does, what the pads do (VOICE: by type) */
} page_t;
#define ROW(n, c, z, o, h) {n, PG_ROW, {c, z, o, NO, NO, NO, NO, NO}, NO, NO, h}
#define ROW3(n, k, h) ROW(n, k, k, k, h)
#define VOICE(n, v) {n, PG_VOICE, {v}, NO, NO, 0}
static const page_t PAGES[] = {
    /* SOUND (PARA) */
    {"WAVE", PG_ROW, {VP_WAVE, VP_WAVE, VC_WAVE, NO, NO, NO, NO, NO}, NO, NO,
     "EACH VOICE'S WAVE\nNOISE: A WIND AT ITS PITCH"},
    VOICE("VOICE 1", 0), VOICE("VOICE 2", 1), VOICE("VOICE 3", 2), VOICE("VOICE 4", 3),
    VOICE("VOICE 5", 4), VOICE("VOICE 6", 5), VOICE("VOICE 7", 6), VOICE("VOICE 8", 7),
    /* ENVELOPE (CHORD) */
    ROW3("ATTACK", VP_ATT, "HOW SLOWLY A VOICE SWELLS IN\nLEFT: AT ONCE, RIGHT: SLOW"),
    ROW3("RELEASE", VP_RLS, "HOW LONG IT FADES WHEN OFF\nLEFT: SHORT, RIGHT: LONG"),
    ROW3("GATE", VP_GSRC, "WHO PLAYS IT ONCE IT IS ON:\nPAD, SEQ, LFO, CASCADE"),
    ROW("DECAY", NO, NO, VC_DEC, "KEY VOICES 4 5: THE FALL\nAFTER THE ATTACK"),
    ROW("SUSTAIN", NO, NO, VC_SUS, "KEY VOICES 4 5: THE LEVEL\nWHILE A KEY IS HELD"),
    ROW("VCO EG", NO, NO, VC_EG, "GATE: AS THE KEY  HOLD: STAYS\nLOOP: PULSES BY ITSELF"),
    /* MOTION (NOTE REPEAT) */
    {"DETUNE", PG_ROW, {VP_DMD, VP_DMD, VP_DMD, NO, NO, NO, NO, NO}, VP_DMR, NO,
     "A SLOW PITCH WOBBLE\nTOP PAD: THE KNOB SETS ITS RATE"},
    ROW3("MOD SRC", VP_MSRC, "WHAT BENDS THE VOICE'S PITCH\nLFO, WHEELS, SEQ, S&H, DRIFT"),
    ROW3("MOD AMT", VP_MAMT, "HOW FAR THE MOD SOURCE\nBENDS IT"),
    ROW("SUB", CL_SUB, NO, VC_SUB, "A DEEP TONE UNDER THE VOICE\nHOW LOUD"),
    ROW("SUB DIV", CL_SUBDIV, NO, NO, "HOW DEEP THE SUB IS:\nTHE PITCH / 2 .. / 8"),
    ROW("S&H AMT", NO, NZ_SHAMT, NO, "NOISE VOICES: RANDOM STEPS\nIN PITCH, HOW FAR"),
    ROW("S&H CLK", NO, NZ_SHCLK, NO, "WHAT TIMES THE RANDOM STEPS"),
    {"LFO", PG_GLOB, {G_LARATE, G_LAWAVE, G_LBRATE, G_LBWAVE, NO, NO, NO, NO}, NO, NO,
     "TWO SLOW WAVES, A AND B:\nFOR MOD, GATE AND FILTER"},
    /* TUNING (SCALE) */
    {"TUNING", PG_GLOB, {G_TSET, G_ROOT, G_SNAP, G_FINE, NO, NO, NO, NO}, NO, NO,
     "THE SCALE KNOBS AND KEYS USE\nSNAP OFF: FREE PITCH"},
    /* EFFECTS (FX) */
    {"FX L", PG_GLOB, {G_FXL, G_FXLX, G_FXLY, G_FXLZ, G_BLEND, NO, NO, NO}, NO, NO,
     "LEFT EFFECT AND ITS X Y Z\nBLEND: HOW MUCH OF IT"},
    {"FX R", PG_GLOB, {G_FXR, G_FXRX, G_FXRY, G_FXRZ, G_BLENDR, NO, NO, NO}, NO, NO,
     "RIGHT EFFECT AND ITS X Y Z\nBLEND: HOW MUCH OF IT"},
    {"FILTER", PG_GLOB, {G_FMODE, G_FFL, G_FRL, G_FDRIVE, G_FMD, G_FMR, G_FCVSRC, G_FCVAMT}, NO, NO,
     "ONE FILTER ON ALL. MOVE: ITS\nLFO. CV: THE MOD WHEEL"},
    /* SEQUENCER (SEQ) */
    {"STEPS", PG_GLOB, {G_SEQ1, G_SEQ2, G_SEQ3, G_SEQ4, G_SEQ5, G_STAGES, G_PULSER, G_RUN}, NO, G_SGATES,
     "5 STEPS. TOP PAD: STEP ON\nPULSER: TEMPO  RUN: PADS"},
    {"LOOP", PG_GLOB, {X_LOOP, NO, NO, NO, NO, NO, NO, NO}, NO, NO,
     "RECORDS KNOBS AND PADS\nPAD 1 TOP: REC/LOOP  BOT: STOP"},
    /* KEYS (ARP) */
    {"KEYS", PG_GLOB, {G_KEYS, G_ARP, G_ARATE, G_AOCT, G_ALATCH, G_GLIDE, G_VIBD, G_VIBR}, NO, NO,
     "HOW THE KEYS PLAY 4 AND 5:\nARP, LATCH, GLIDE, VIBRATO"},
    {"STEP REC", PG_GLOB, {X_STEPREC, NO, NO, NO, NO, NO, NO, NO}, NO, NO,
     "PAD 1 TOP, THEN KEYS: STEPS\nFOR ARP SEQ. BOTTOM: STOP"},
    /* MIX (GLOBE) */
    ROW3("VOLUME", VP_VOL, "EACH VOICE'S LOUDNESS"),
    ROW3("PAN", VP_PAN, "EACH VOICE LEFT OR RIGHT"),
    ROW3("TONE", VP_TONE, "EACH VOICE DARK OR BRIGHT"),
    {"OUTPUT", PG_GLOB, {G_MASTER, G_WIDTH, G_TAPE, G_DRIFT, NO, NO, NO, NO}, NO, NO,
     "MASTER, STEREO WIDTH, TAPE\nWOBBLE, ANALOG DRIFT"},
    /* SETUP (PATCH held): the device, not the sound */
    {"SETUP", PG_GLOB, {X_LIGHT, X_CONSOLE, X_OUTPUT, NO, NO, NO, NO, NO}, NO, NO,
     "PAD LIGHT, USB CONSOLE,\nOUTPUT: LINE OR SPEAKER"},
    {"JOYSTICK", PG_GLOB, {G_JXOFS, G_JYOFS, NO, NO, NO, NO, NO, NO}, NO, NO,
     "THE WHEELS' CENTRE: TRIM IF\nPITCH OR MOD DRIFTS"},
};
/* the voice pages: by its type */
static const char *const VHELP[3] = {"5 TONES, ONE CHORD. VOLT: GRIT\nTOP PAD: TONE ON  BOT: MOD",
                                     "A TONE A SQUARE SHAKES:\nMODE FM / AM, NOISE: AIR",
                                     "THE KEYS' OSCILLATOR\nSHAPE BENDS THE WAVE"};
typedef struct { const char *name, *button; uint8_t btn, first, n; } menu_t;
static const menu_t MENUS[MN_N] = {
    [MN_SOUND] = {"SOUND", "PARA", B_PARA, 0, 9},
    [MN_ENV] = {"ENVELOPE", "CHORD", B_CHORD, 9, 6},
    [MN_MOTION] = {"MOTION", "REPEAT", B_REPEAT, 15, 8},
    [MN_TUNING] = {"TUNING", "SCALE", B_SCALE, 23, 1},
    [MN_FX] = {"EFFECTS", "FX", B_FX, 24, 3},
    [MN_SEQ] = {"SEQUENCER", "SEQ", B_SEQ, 27, 2},
    [MN_KEYS] = {"KEYS", "ARP", B_ARP, 29, 2},
    [MN_MIX] = {"MIX", "GLOBE", B_GLOBE, 31, 4},
    [MN_SETUP] = {"SETUP", "PATCH+", B_PATCH, 35, 2},
};
_Static_assert(sizeof PAGES / sizeof PAGES[0] == 37u, "MENUS: the pages");
/* the fader banks (FADER BANK, < > or the top pads): shortcuts, four menu parameters each; a global (a pair: L
 * and R) or one voice parameter on a group of voices */
typedef struct { const char *name; uint8_t g, id, b; } fdest_t;   /* g: globals id and b (NO: none); else id on mask b */
typedef struct { const char *name; fdest_t f[4]; } fbank_t;
#define FG(n, a, b) {n, 1, a, b}
#define FV(n, k, m) {n, 0, k, m}
static const fbank_t FBANK[] = {
    {"FILTER", {FG("CUTOFF", G_FFL, NO), FG("RESO", G_FRL, NO), FG("MOVE", G_FMD, NO), FG("DRIVE", G_FDRIVE, NO)}},
    {"SPACE", {FG("SPACE", G_BLEND, G_BLENDR), FG("FX X", G_FXLX, G_FXRX), FG("FX Y", G_FXLY, G_FXRY),
               FG("TAPE", G_TAPE, NO)}},
    {"VOICES", {FV("1+2", VP_VOL, 0x03), FV("3+8", VP_VOL, 0x84), FV("4+5", VP_VOL, 0x18), FV("6+7", VP_VOL, 0x60)}},
    {"TIMBRE", {FV("VOLT", CL_VOLT, 0x63), FV("TONE", VP_TONE, 0xFF), FV("NZ FM", NZ_FMAMT, 0x84),
                FV("SHAPE", VC_SHAPE, 0x18)}},
    {"MOTION", {FV("DETUNE", VP_DMD, 0xFF), FV("DT RATE", VP_DMR, 0xFF), FV("MOD AMT", VP_MAMT, 0xFF),
                FG("ANALOG", G_DRIFT, NO)}},
    {"SWELL", {FV("ATTACK", VP_ATT, 0xFF), FV("RELEASE", VP_RLS, 0xFF), FG("GLIDE", G_GLIDE, NO),
               FG("VIBRATO", G_VIBD, NO)}},
};
#define NFBANK (sizeof FBANK / sizeof FBANK[0])

/* a column, as built from the table: the knob's parameter and what its pads do */
enum { IK_NONE, IK_V, IK_G, IK_X };
typedef struct {
    uint8_t kind, id, voice;
    uint8_t alt;                           /* the top pad switches the knob to this parameter */
    uint8_t pbit, pid, pinv;               /* top pad: bit pbit of parameter pid (pinv: lit = clear); NO: none */
    uint8_t bid;                           /* bottom pad: bit pbit of this one (a generator's MOD) */
} item_t;
enum { SET_BRIGHT, SET_CONSOLE, SET_SPEAKER };
enum { DLG_NONE, DLG_SAVE };
enum { MO_IDLE, MO_REC, MO_LOOP, MO_STOP };
#define MO_N 512u
#define NPICK (NPRESET + NUSER)            /* the presets, then the user's (theme MINE) */

static struct {
    uint8_t menu, page, preset, pre_sel;   /* preset: the one playing; pre_sel: its theme page on PRESETS */
    int8_t oct;
    item_t it[8];
    uint32_t enc_t[FM1_NENC];
    char msg[28];
    uint32_t msg_until;
    uint8_t dirty;
    uint8_t latched[NV], ignore_rel[NV], pad_gate[NV], hush[NV];
    uint32_t t_press[NV], t_off[NV];   /* hush: off at once (a second tap while it fades) */
    uint64_t keys;
    uint8_t fad_last[4], fbank;
    uint32_t btn_t[FM1_NBTN];              /* buttons held since (ms | 1), 0 = up */
    uint8_t help, help_panel;
    uint32_t help_t;
    uint32_t tap_t;
    uint8_t dlg, dlg_arg;                  /* SAVE over a used slot: the green or the red pad */
    uint8_t chaos_held, chaos_used;
    uint8_t kbm, nsel, pdown;              /* KB: on, the voices selected; the voice pads held now */
    int8_t kbk[NV];                        /* (the keys held now, low to high) */
    uint8_t kbn;
    uint8_t czhit[NV];                     /* (a creature hops: CHAOS rolled it) */
    struct { uint8_t mask, n; uint32_t t, until; int32_t hist[NV][3]; } cz;   /* CHAOS: 3 rolls, 170 ms apart */
    uint8_t save_pad;                      /* MINE: the pad held (+1), since save_t */
    uint32_t save_t;
    uint8_t altmask;                       /* columns whose knob edits their second parameter (DETUNE: RATE) */
    uint8_t free, hold;                    /* FREE-PLAY: the keys wake the drones (SEQ PLAY); HOLD (SEQ REC) */
    int8_t fkey[NV];                       /* FREE-PLAY: the key each voice plays (-128: none) */
    uint8_t forder[NV];                    /* (the order they woke in: the oldest goes first) */
    struct { uint8_t state; uint16_t n, i; uint32_t t0, len, last; } mo;   /* the motion loop */
} ui;
static struct { uint16_t t; uint8_t kind, a, c; int16_t b; } mo_ev[MO_N];   /* t in 10 ms */
enum { EV_GATE_ON = 1, EV_GATE_OFF, EV_V, EV_G };

static void keys_poll(void);
static void midi_poll(void);
static void faders_sync(void);
static void fbank_go(uint32_t n);
static void mo_toggle(void);
static void free_play(uint32_t on);
static void free_release_all(void);
static void big_show(uint32_t col, const char *label, const char *val);
static void pitch_turn(uint32_t v, int32_t s);

static void ui_say(const char *a, const char *b)
{
    str_cpy(ui.msg, a, sizeof ui.msg);
    if (b)
        str_cat(ui.msg, b, sizeof ui.msg);
    ui.msg_until = fm1_ms + 1500u;
    ui.dirty = 1;
}

static void mo_add(uint32_t kind, uint32_t a, uint32_t c, int32_t b)   /* the motion loop records */
{
    if (ui.mo.state != MO_REC || ui.mo.n >= MO_N)
        return;
    mo_ev[ui.mo.n].t = (uint16_t)((fm1_ms - ui.mo.t0) / 10u);
    mo_ev[ui.mo.n].kind = (uint8_t)kind, mo_ev[ui.mo.n].a = (uint8_t)a, mo_ev[ui.mo.n].c = (uint8_t)c;
    mo_ev[ui.mo.n].b = (int16_t)b;
    ui.mo.n++;
}

/* ------------------------------------------------------------- a menu page into ui.it --- */
static const page_t *page_cur(void) { return &PAGES[MENUS[ui.menu].first + ui.page]; }

static void menu_build(void)
{
    static const uint8_t VC[8] = {VC_SHAPE, VC_OCT, VC_TUNE, VC_FM, VC_SYNC, NO, NO, NO};   /* (WAVE, SUB: their rows) */
    const page_t *pg;
    uint32_t k, v;
    for (k = 0; k < 8u; k++) {
        item_t *it = &ui.it[k];
        it->kind = IK_NONE, it->id = 0, it->voice = (uint8_t)k, it->alt = it->pid = it->bid = NO, it->pbit = 0;
        it->pinv = 0;
    }
    if (ui.menu == MN_HOME || ui.menu == MN_PRESETS || ui.menu == MN_FADERS)
        return;
    if (ui.page >= MENUS[ui.menu].n)
        ui.page = 0;
    pg = page_cur();
    for (k = 0; k < 8u; k++) {
        item_t *it = &ui.it[k];
        uint32_t id;
        switch (pg->kind) {
        case PG_ROW:
            id = pg->id[VTYPE[k] == VT_CLASSIC ? 0 : VTYPE[k] == VT_NOISE ? 1 : 2];
            if (id != NO)
                it->kind = IK_V, it->id = (uint8_t)id, it->alt = pg->alt;
            break;
        case PG_GLOB:
            id = pg->id[k];
            if (id == NO)
                break;
            it->kind = id >= X_LIGHT ? IK_X : IK_G, it->id = (uint8_t)id;
            if (pg->bits != NO && k < 5u)
                it->pid = pg->bits, it->pbit = (uint8_t)k;
            break;
        default:                                        /* one voice: its own eight */
            v = pg->id[0];
            it->voice = (uint8_t)v;
            if (VTYPE[v] == VT_CLASSIC) {               /* 5 tunes (top pad: sounding, bottom: modulated), VOLT */
                if (k < 5u)
                    it->kind = IK_V, it->id = (uint8_t)(CL_TUNE1 + k), it->pid = CL_MUTE, it->pinv = 1,
                    it->bid = CL_MOD, it->pbit = (uint8_t)k;
                else if (k == 5u)
                    it->kind = IK_V, it->id = CL_VOLT;
            } else if (VTYPE[v] == VT_NOISE)
                it->kind = IK_V, it->id = (uint8_t)(NZ_PITCH + k);
            else if (VC[k] != NO && !(VC[k] == VC_SYNC && v == 3u))
                it->kind = IK_V, it->id = VC[k];
        }
        if (it->alt != NO && ((ui.altmask >> k) & 1u))
            it->id = it->alt;
    }
}

static const pdesc_t *it_desc(const item_t *it)
{
    static const char *const N_OUT[2] = {"LINE", "SPEAKER"};
    static const char *const N_LOOP[2] = {"REC", "STOP"};
    static const pdesc_t X[5] = {{"LIGHT", 1, 8, 4, F_NUM, 0}, {"CONSOLE", 0, 1, 1, F_ONOFF, 0},
                                 {"OUTPUT", 0, 1, 0, F_ENUM, N_OUT}, {"LOOP", 0, 1, 0, F_ENUM, N_LOOP},
                                 {"STEP REC", 0, 1, 0, F_ENUM, N_LOOP}};
    switch (it->kind) {
    case IK_V: return &VDESC[VTYPE[it->voice]][it->id];
    case IK_G: return &GDESC[it->id];
    case IK_X: return &X[it->id - X_LIGHT];
    default: return 0;
    }
}

static int32_t it_get(const item_t *it)
{
    switch (it->kind) {
    case IK_V: return pt.v[it->voice][it->id];
    case IK_G: return pt.g[it->id];
    case IK_X:
        switch (it->id) {
        case X_LIGHT: return settings.bright;
        case X_CONSOLE: return settings.console;
        case X_OUTPUT: return settings.speaker;
        case X_LOOP: return ui.mo.state == MO_REC || ui.mo.state == MO_LOOP ? 0 : 1;
        default: return kb.rec ? 0 : 1;
        }
    default: return 0;
    }
}

static void it_label(const item_t *it, char *b, uint32_t n)
{
    const pdesc_t *d = it_desc(it);
    str_cpy(b, d ? d->label : "", n);
}

static void cents_text(char *b, uint32_t n, int32_t c)   /* cents above MIDI 0 -> "E3+12" */
{
    int32_t m = (c + 50) / 100, dc = c - m * 100;
    char t[8];
    str_cpy(b, NOTE_NAMES[((m % 12) + 12) % 12], n);
    fmt_int(t, m / 12 - 1);
    str_cat(b, t, n);
    if (dc) {
        uint32_t k = str_len(b);
        b[k] = dc > 0 ? '+' : '-';
        fmt_int(b + k + 1u, dc > 0 ? dc : -dc);
    }
}

static void it_text(const item_t *it, char *b, uint32_t n)
{
    const pdesc_t *d = it_desc(it);
    int32_t v = it_get(it);
    if (it->kind == IK_X && (it->id == X_LOOP || it->id == X_STEPREC)) {
        static const char *const ST[4] = {"-", "REC", "LOOP", "STOP"};
        if (it->id == X_LOOP)
            str_cpy(b, ST[ui.mo.state & 3u], n);
        else {
            str_cpy(b, kb.rec ? "REC " : "", n);
            fmt_int(b + str_len(b), kb.seqlen);
        }
        return;
    }
    if (!d) {
        b[0] = 0;
        return;
    }
    switch (d->fmt) {
    case F_ENUM:
        str_cpy(b, d->names[clamp(v, d->min, d->max) - d->min], n);
        break;
    case F_ONOFF:
        str_cpy(b, v ? "ON" : "OFF", n);
        break;
    case F_PAN:
        if (v == 64)
            str_cpy(b, "C", n);
        else {
            b[0] = v < 64 ? 'L' : 'R';
            fmt_int(b + 1, v < 64 ? 64 - v : v - 64);
        }
        break;
    case F_TUNE:
        cents_text(b, n, GEN_LO[it->id - CL_TUNE1] * 100 + v);
        break;
    case F_CENTS:
    case F_SIGNED:
        if (v > 0) {
            b[0] = '+';
            fmt_int(b + 1, v);
        } else
            fmt_int(b, v);
        break;
    default:
        fmt_int(b, v);
    }
}

static int it_onpads(const item_t *it)                   /* on / off or two choices: set on the column's pads */
{
    const pdesc_t *d = it_desc(it);
    return d && (d->fmt == F_ONOFF || (d->fmt == F_ENUM && d->max - d->min == 1));
}

/* ------------------------------------------------------------- editing --- */
static int32_t accel(uint32_t e, int32_t s, int32_t span)
{
    uint32_t dt = fm1_ms - ui.enc_t[e];
    ui.enc_t[e] = fm1_ms;
    if (span > 300 && dt < 60u)
        return s * (dt < 25u ? 25 : 6);
    if (span > 64 && dt < 40u)
        return s * 3;
    return s;
}

static void it_set(const item_t *it, int32_t v)          /* a new value (clamped), recorded by the motion loop */
{
    const pdesc_t *d = it_desc(it);
    v = clamp(v, d->min, d->max);
    if (it->kind == IK_V) {
        pt.v[it->voice][it->id] = (int16_t)v;
        mo_add(EV_V, it->voice, it->id, v);
    } else if (it->kind == IK_G) {
        pt.g[it->id] = (int16_t)v;
        if (it->id == G_PULSER)
            pulser_inc = 0;                             /* the knob takes over from the tapped tempo */
        mo_add(EV_G, 0, it->id, v);
    } else if (it->id == X_LIGHT)
        settings.bright = (uint8_t)v;
    else if (it->id == X_CONSOLE)
        settings.console = (uint8_t)v;
    else if (it->id == X_OUTPUT)
        settings.speaker = (uint8_t)v;
    ui.dirty = 1;
}

static void it_turn(const item_t *it, uint32_t e, int32_t s)
{
    const pdesc_t *d = it_desc(it);
    int32_t v = it_get(it);
    if (it->kind == IK_V && d->fmt == F_TUNE && pt.g[G_SNAP]) {   /* to the next note of the tuning set */
        int32_t lo = GEN_LO[it->id - CL_TUNE1] * 100, c = lo + v, k;
        for (k = 0; k < (s > 0 ? s : -s); k++)
            c = tun_next((uint32_t)pt.g[G_TSET], (uint32_t)pt.g[G_ROOT], c, s > 0 ? 1 : -1);
        it_set(it, c - lo);
    } else
        it_set(it, v + accel(e, s, d->max - d->min));
}

static void show_item(uint32_t col, const item_t *it)    /* its name and value, big */
{
    char lb[16], vb[16];
    it_label(it, lb, sizeof lb);
    it_text(it, vb, sizeof vb);
    big_show(col, lb, vb);
}

/* a column's pad: the two choices, a bit (a step, a generator), the knob's second parameter, or an action */
static void column_pad(uint32_t k, uint32_t top)
{
    item_t *it = &ui.it[k];
    if (it->kind == IK_NONE)
        return;
    if (it->kind == IK_X && (it->id == X_LOOP || it->id == X_STEPREC)) {
        if (it->id == X_LOOP) {                         /* top: record / loop / stop / loop; bottom: stop */
            if (top)
                mo_toggle();
            else
                ui.mo.state = ui.mo.state == MO_IDLE ? MO_IDLE : MO_STOP;
        } else if (top) {                               /* top: record anew; bottom: stop */
            kb.seqlen = 0;
            kb.rec = 1;
        } else
            kb.rec = 0;
        show_item(k, it);
        return;
    }
    if (it_onpads(it)) {
        const pdesc_t *d = it_desc(it);
        it_set(it, d->fmt == F_ONOFF ? (top ? !it_get(it) : it_get(it)) : top ? d->min : d->max);
        show_item(k, it);
        return;
    }
    if (it->alt != NO && top) {                         /* the knob: its first or its second parameter */
        ui.altmask ^= (uint8_t)(1u << k);
        menu_build();
        show_item(k, it);
        return;
    }
    if ((top && it->pid != NO) || (!top && it->bid != NO)) {
        uint32_t id = top ? it->pid : it->bid;
        int16_t *p = it->kind == IK_G ? &pt.g[id] : &pt.v[it->voice][id];
        *p ^= (int16_t)(1 << it->pbit);
        if (it->kind == IK_G)
            mo_add(EV_G, 0, id, *p);
        else
            mo_add(EV_V, it->voice, id, *p);
        ui.dirty = 1;
    }
}

/* ------------------------------------------------------------- presets --- */
static void autogate(void)                              /* a preset's voices start (VCOs: only by keys) */
{
    uint32_t v, bits = (uint32_t)pt.g[G_AUTOGATE];
    for (v = 0; v < NV; v++) {                          /* (a voice with its own gate source: on, it plays it) */
        uint32_t on = VTYPE[v] != VT_VCO && (((bits >> v) & 1u) || pt.v[v][VP_GSRC] != GS_PAD);
        ui.latched[v] = ui.pad_gate[v] = (uint8_t)on;
        ui.ignore_rel[v] = ui.hush[v] = 0;
    }
    kb.padhold[0] = kb.padhold[1] = 0;
}
static int preset_load(uint32_t n)                      /* 0..95 the presets, 96..111 the user's; 0 = loaded */
{
    n %= NPICK;
    if (n < NPRESET)
        preset_make(n, &pt);
    else if (user_load_to(n - NPRESET, &pt)) {
        ui_say("EMPTY", 0);
        return -1;
    } else
        settings.user = (uint8_t)(n - NPRESET + 1u);
    faders_sync();                                      /* (what the faders hold stays theirs) */
    ui.preset = (uint8_t)n;
    pulser_inc = 0;
    md.restart = 1;
    autogate();
    ui.dirty = 1;
    return 0;
}
static void preset_name(uint32_t n, char *b, uint32_t sz)
{
    if (n < NPRESET)
        str_cpy(b, PRESET_NAME[n], sz);
    else {
        str_cpy(b, (user_used >> (n - NPRESET)) & 1u ? "MINE " : "EMPTY ", sz);
        fmt_int(b + str_len(b), (int32_t)(n - NPRESET + 1u));
    }
}

static void dlg_open(uint32_t kind, uint32_t arg)
{
    ui.dlg = (uint8_t)kind;
    ui.dlg_arg = (uint8_t)arg;
    ui.dirty = 1;
}
static void user_store(uint32_t slot)                   /* the sound into MINE slot (0..15), its drones awake */
{
    uint32_t v, awake = 0;
    char d[4];
    for (v = 0; v < NV; v++)
        awake |= (uint32_t)ui.latched[v] << v;
    pt.g[G_AUTOGATE] = (int16_t)awake;
    fmt_int(d, (int32_t)slot + 1);
    if (user_save(slot))
        ui_say("SAVE FAILED ", d);
    else {
        ui.preset = (uint8_t)(NPRESET + slot);
        settings.user = (uint8_t)(slot + 1u);
        ui_say("SAVED MINE ", d);
    }
}
static void dlg_answer(int ok)
{
    uint32_t kind = ui.dlg;
    ui.dlg = DLG_NONE;
    ui.dirty = 1;
    if (ok && kind == DLG_SAVE)
        user_store(ui.dlg_arg);
}

/* the picker: a pad = a preset of the theme shown, heard at once; on MINE a tap loads, held 1 s saves */
static void picker_pad(uint32_t pad, uint32_t vel)
{
    uint32_t n = (ui.pre_sel / PRESET_PAGE) * PRESET_PAGE + pad;
    if (n >= NPRESET) {
        if (vel) {
            ui.save_pad = (uint8_t)(pad + 1u);
            ui.save_t = fm1_ms;
        } else if (ui.save_pad == pad + 1u) {           /* a tap: load (empty: how to save) */
            ui.save_pad = 0;
            if ((user_used >> (n - NPRESET)) & 1u)
                preset_load(n);
            else
                ui_say("HOLD TO SAVE", 0);
        }
        return;
    }
    if (vel)
        preset_load(n);
}
static void picker_tick(void)                           /* a MINE pad held 1 s: save (over a used one: asks) */
{
    uint32_t slot;
    if (!ui.save_pad || fm1_ms - ui.save_t < 1000u)
        return;
    slot = ui.save_pad - 1u;
    ui.save_pad = 0;
    if ((user_used >> slot) & 1u)
        dlg_open(DLG_SAVE, slot);
    else
        user_store(slot);
}

/* ------------------------------------------------------------- HOME: Ki = pitch of voice i --- */
static int32_t voice_base(uint32_t v)                   /* the pitch PITCH moves: lowest generator, tone, key */
{
    const int16_t *p = pt.v[v];
    if (VTYPE[v] == VT_CLASSIC) {
        uint32_t g;
        for (g = 0; g < 5u; g++)
            if (!((p[CL_MUTE] >> g) & 1))
                return GEN_LO[g] * 100 + p[CL_TUNE1 + g];
        return GEN_LO[0] * 100 + p[CL_TUNE1];
    }
    if (VTYPE[v] == VT_NOISE) {
        int32_t lo = p[NZ_RANGE] ? 4800 : 1200, hi = p[NZ_RANGE] ? 10000 : 6000;
        return lo + p[NZ_PITCH] * (hi - lo) / 127;
    }
    return kb.cents[v - 3u] + OCT_CENTS[p[VC_OCT] & 7] + p[VC_TUNE];
}
static void pitch_turn(uint32_t v, int32_t s)
{
    int32_t base = voice_base(v), c = base + pt.v[v][VP_PITCH], k;
    char b[16], lb[8] = "VOICE ";
    if (pt.g[G_SNAP])
        for (k = 0; k < (s > 0 ? s : -s); k++)
            c = tun_next((uint32_t)pt.g[G_TSET], (uint32_t)pt.g[G_ROOT], c, s > 0 ? 1 : -1);
    else
        c += accel(v, s, 2000) * 5;
    pt.v[v][VP_PITCH] = vp_clamp(v, VP_PITCH, c - base);
    mo_add(EV_V, v, VP_PITCH, pt.v[v][VP_PITCH]);
    lb[6] = (char)('1' + v), lb[7] = 0;
    cents_text(b, sizeof b, base + pt.v[v][VP_PITCH]);
    big_show(v, lb, b);
}

static void gate_press(uint32_t v)
{
    if (VTYPE[v] == VT_VCO) {                           /* 4 / 5: hold the last key's note */
        kb.padhold[v - 3u] ^= 1u;
        return;
    }
    if (ui.latched[v]) {                                /* a tap on a held drone ends it (its release) */
        ui.latched[v] = 0;
        ui.pad_gate[v] = 0;
        ui.ignore_rel[v] = 1;
        ui.t_off[v] = fm1_ms;
        mo_add(EV_GATE_OFF, v, 0, 0);
        return;
    }
    if (vs[v].stage && fm1_ms - ui.t_off[v] < 600u) {  /* tapped again while it fades: silent at once */
        ui.hush[v] = 1;
        ui.ignore_rel[v] = 1;
        return;
    }
    mo_add(EV_GATE_ON, v, 0, 0);
    ui.hush[v] = 0;
    ui.pad_gate[v] = 1;
    ui.t_press[v] = fm1_ms;
}
static void gate_release(uint32_t v)
{
    if (VTYPE[v] == VT_VCO)
        return;
    if (ui.ignore_rel[v]) {
        ui.ignore_rel[v] = 0;
        return;
    }
    if (fm1_ms - ui.t_press[v] < 350u)
        ui.latched[v] = 1;                              /* a tap: HOLD */
    else {
        ui.pad_gate[v] = 0;                             /* held: momentary */
        mo_add(EV_GATE_OFF, v, 0, 0);
    }
}

/* a voice to a key's note: a VCO exactly, a drone the same note nearest its own register */
static void voice_note(uint32_t v, int32_t key)
{
    int32_t c = kb_cents(key);
    if (VTYPE[v] == VT_VCO) {
        kb.cents[v - 3u] = c - OCT_CENTS[pt.v[v][VC_OCT] & 7] - pt.v[v][VC_TUNE];
        pt.v[v][VP_PITCH] = 0;
    } else {
        int32_t d = c - voice_base(v);
        d = ((d + 600) % 1200 + 1200) % 1200 - 600;
        pt.v[v][VP_PITCH] = vp_clamp(v, VP_PITCH, d);
    }
    mo_add(EV_V, v, VP_PITCH, pt.v[v][VP_PITCH]);
}
static void voice_wake(uint32_t v, uint32_t vel)
{
    if (VTYPE[v] == VT_VCO) {
        kb.padhold[v - 3u] = 1;
        return;
    }
    gate_vel[v] = (uint8_t)vel;
    ui.latched[v] = ui.pad_gate[v] = 1;
    ui.hush[v] = 0;
}
static int32_t voice_note_now(uint32_t v)               /* the nearest note it plays, cents */
{
    return (voice_base(v) + pt.v[v][VP_PITCH] + 50) / 100 * 100;
}

/* CHAOS (pad C): every voice (or, C held + a top pad, that voice: woken, all its sound) rolls three notes within
 * the set, 170 ms apart, and keeps the third */
static void chaos_roll(void)
{
    uint32_t v, g, set = (uint32_t)pt.g[G_TSET], root = (uint32_t)pt.g[G_ROOT];
    uint32_t one = (ui.cz.mask & (ui.cz.mask - 1u)) == 0u;
    static uint32_t seed = 0x2545F491u;
    seed ^= fm1_ms;
    for (v = 0; v < NV; v++) {
        int16_t *p = pt.v[v];
        if (!((ui.cz.mask >> v) & 1u))
            continue;
        if (VTYPE[v] == VT_CLASSIC)
            for (g = 0; g < 5u; g++) {
                int32_t lo = GEN_LO[g] * 100, span = (GEN_HI[g] - GEN_LO[g]) * 100;
                int32_t c = tun_next(set, root, lo + (int32_t)(xorshift(&seed) % (uint32_t)span) - 1, 1);
                p[CL_TUNE1 + g] = vp_clamp(v, CL_TUNE1 + g, c - lo);
            }
        else if (VTYPE[v] == VT_NOISE) {
            p[NZ_PITCH] = (int16_t)(20 + xorshift(&seed) % 100u);
            if (one) {
                p[NZ_RATE] = (int16_t)(xorshift(&seed) % 110u);
                p[NZ_FMAMT] = (int16_t)(xorshift(&seed) % 128u);
                p[NZ_DIV] = (int16_t)(1 + xorshift(&seed) % 8u);
                p[NZ_MODE] = (int16_t)(xorshift(&seed) % 4u);
            }
        } else {
            voice_note(v, (int32_t)(xorshift(&seed) % 25u));
            if (one) {
                p[VC_WAVE] = (int16_t)(xorshift(&seed) % 6u);
                p[VC_SHAPE] = (int16_t)(xorshift(&seed) % 128u);
            }
        }
        ui.cz.hist[v][ui.cz.n] = voice_note_now(v);
        ui.czhit[v]++;
    }
    ui.cz.n++;
    ui.cz.t = fm1_ms;
    ui.cz.until = fm1_ms + 1800u;
    ui.dirty = 1;
}
static void chaos(uint32_t v0, uint32_t v1)
{
    uint32_t v;
    ui.cz.mask = (uint8_t)(((1u << v1) - 1u) & ~((1u << v0) - 1u));
    ui.cz.n = 0;
    if (v1 - v0 == 1u) {
        voice_wake(v0, 100);
        ui.ignore_rel[v0] = 1;                          /* (its pad's release: stays on) */
    }
    for (v = 0; v < NV; v++)
        ui.cz.hist[v][0] = ui.cz.hist[v][1] = ui.cz.hist[v][2] = voice_note_now(v);
    chaos_roll();
}
static void chaos_tick(void)
{
    if (ui.cz.n && ui.cz.n < 3u && fm1_ms - ui.cz.t >= 170u)
        chaos_roll();
}

/* HOLD ALL (pad H): everything sounding stays on; again: all off */
static void hold_all(void)
{
    uint32_t v, any = 0;
    for (v = 0; v < NV; v++)
        any |= VTYPE[v] != VT_VCO && vs[v].stage && !ui.latched[v];
    for (v = 0; v < NV; v++) {
        uint32_t on = any && vs[v].stage && VTYPE[v] != VT_VCO;
        ui.latched[v] = ui.pad_gate[v] = (uint8_t)on;
        ui.ignore_rel[v] = 0;
    }
    if (!any)
        kb.padhold[0] = kb.padhold[1] = 0;
    ui_say(any ? "HOLD ALL" : "ALL OFF", 0);
}

/* PULSER (pad P): a tap starts it, taps in a row set its tempo, held 0.6 s stops it */
static void tap_press(void)
{
    uint32_t dt = fm1_ms - ui.tap_t;
    if (pt.g[G_RUN] && dt >= 60u && dt <= 3000u) {
        pulser_inc = 0xFFFFFFFFu / (dt * 1378u / 1000u);
        md.restart = 1;
        ui_say("TAP", 0);
    } else if (!pt.g[G_RUN]) {
        pt.g[G_RUN] = 1;
        md.restart = 1;
    }
    ui.tap_t = fm1_ms;
    ui.dirty = 1;
}

static void home_pad(uint32_t pad, uint32_t vel)
{
    if (pad < 8u && ui.kbm) {                           /* KB: a top pad selects its voice (or lets it go) */
        if (vel)
            ui.nsel ^= (uint8_t)(1u << pad);
        ui.dirty = 1;
    } else if (pad < 8u) {                              /* top row: the voices' gates */
        if (vel)
            ui.pdown |= (uint8_t)(1u << pad);
        else
            ui.pdown &= (uint8_t)~(1u << pad);
        if (vel && ui.chaos_held) {
            chaos(pad, pad + 1u);
            ui.chaos_used = 1;
        } else if (vel) {
            gate_vel[pad] = (uint8_t)vel;
            gate_press(pad);
        } else
            gate_release(pad);
    } else if (pad == 8u) {                             /* KB: select voices, then the keys tune them */
        if (vel) {
            ui.kbm ^= 1u;
            ui.nsel = 0;
            ui.dirty = 1;
        }
    } else if (pad == 13u) {                            /* P: PULSER */
        if (vel)
            tap_press();
        else if (fm1_ms - ui.tap_t >= 600u)
            pt.g[G_RUN] = 0;
    } else if (pad == 14u) {                            /* H: HOLD ALL */
        if (vel)
            hold_all();
    } else if (pad == 15u) {                            /* C: CHAOS (held + a top pad: that voice) */
        if (vel) {
            ui.chaos_held = 1;
            ui.chaos_used = 0;
        } else {
            if (!ui.chaos_used)
                chaos(0, NV);
            ui.chaos_held = 0;
        }
    }
}

static void pads_poll(void)
{
    int32_t e;
    while ((e = smk_pad_take()) >= 0) {
        uint32_t pad = (uint32_t)e >> 8, vel = (uint32_t)e & 255u;
        if (ui.dlg) {                                   /* a question: green OK (16), red CANCEL (15) */
            if (vel && (pad == 15u || pad == 14u))
                dlg_answer(pad == 15u);
        } else if (ui.menu == MN_HOME)
            home_pad(pad, vel);
        else if (ui.menu == MN_PRESETS)
            picker_pad(pad, vel);
        else if (ui.menu == MN_FADERS) {                /* a top pad: that bank */
            if (vel && pad < NFBANK)
                fbank_go(pad);
        } else if (vel)
            column_pad(pad & 7u, pad < 8u);
    }
}

/* ------------------------------------------------------------- wheels --- */
/* the pitch and mod wheels, self-calibrating (SLOOP's smk_wheels): X springs back (bend), Y 0..32767 */
static void wheels_poll(void)
{
    enum { DEAD = 12, SPAN0 = 80, MOD_LO = 485, MOD_HI = 615 };
    static int32_t pc, plo, phi, mlo, mhi, rest;
    int32_t p = fm1_in.wheel[0], m = fm1_in.wheel[1], d, x;
    if (fm1_in.frames < 300u)
        return;
    if (!pc) {
        pc = p;
        plo = p - SPAN0;
        phi = p + SPAN0;
        mlo = m < MOD_LO ? m : MOD_LO;
        mhi = m > MOD_HI ? m : MOD_HI;
    }
    plo = p < plo ? p : plo;
    phi = p > phi ? p : phi;
    mlo = m < mlo ? m : mlo;
    mhi = m > mhi ? m : mhi;
    d = p - pc;
    if (d > -DEAD && d < DEAD) {
        x = 0;
        if (++rest > 200) {
            pc += d > 0 ? 1 : d < 0 ? -1 : 0;
            rest = 0;
        }
    } else {
        rest = 0;
        x = d > 0 ? (d - DEAD) * 32767 / (phi - pc - DEAD > 1 ? phi - pc - DEAD : 1)
                  : (d + DEAD) * 32767 / (pc - plo - DEAD > 1 ? pc - plo - DEAD : 1);
    }
    joy_x = clamp(x, -32767, 32767);
    joy_y = mhi - m <= DEAD ? 0 : clamp((mhi - m - DEAD) * 32767 / (mhi - mlo - DEAD > 1 ? mhi - mlo - DEAD : 1), 0, 32767);
}

/* ------------------------------------------------------------- faders --- */
/* The bank's four, absolute: a fader sets its parameter when it moves (a new bank: nothing jumps); at power-on and
 * after a preset the bank in use takes the faders as they stand. */
static uint8_t fad_ready;
static uint32_t fd_voice(const fdest_t *f)              /* (a group: its first voice) */
{
    uint32_t v = 0;
    while (!((f->b >> v) & 1u))
        v++;
    return v;
}
static const pdesc_t *fd_desc(const fdest_t *f)
{
    return f->g ? &GDESC[f->id] : &VDESC[VTYPE[fd_voice(f)]][f->id];
}
static int32_t fd_get(const fdest_t *f)
{
    return f->g ? pt.g[f->id] : pt.v[fd_voice(f)][f->id];
}
static void fader_set(uint32_t i, uint32_t x)           /* x 0..127 over the parameter's range */
{
    const fdest_t *f = &FBANK[ui.fbank].f[i];
    const pdesc_t *d = fd_desc(f);
    int32_t val = d->min + (int32_t)x * (d->max - d->min) / 127;
    uint32_t v;
    if (f->g) {
        pt.g[f->id] = (int16_t)val;
        mo_add(EV_G, 0, f->id, val);
        if (f->b != NO) {
            pt.g[f->b] = (int16_t)val;
            mo_add(EV_G, 0, f->b, val);
        }
    } else
        for (v = 0; v < NV; v++)
            if ((f->b >> v) & 1u) {
                pt.v[v][f->id] = (int16_t)val;
                mo_add(EV_V, v, f->id, val);
            }
}
static void faders_sync(void)                           /* all four, as they stand */
{
    uint32_t i;
    if (!fad_ready)
        return;
    for (i = 0; i < 4u; i++)
        fader_set(i, fm1_in.fader[i]);
}
static void faders_poll(void)
{
    uint32_t i;
    if (fm1_in.frames < 200u)                           /* (the first readings settle) */
        return;
    if (!fad_ready) {
        fad_ready = 1;
        for (i = 0; i < 4u; i++)
            ui.fad_last[i] = fm1_in.fader[i];
        faders_sync();
        return;
    }
    for (i = 0; i < 4u; i++) {
        uint32_t v = fm1_in.fader[i];
        char b[8];
        if (v == ui.fad_last[i])
            continue;
        ui.fad_last[i] = (uint8_t)v;
        fader_set(i, v);
        ui.dirty = 1;
        if (ui.menu != MN_FADERS) {                     /* (the FADERS screen shows them itself) */
            fmt_int(b, fd_get(&FBANK[ui.fbank].f[i]));
            big_show(8, FBANK[ui.fbank].f[i].name, b);
        }
    }
}
static void fbank_go(uint32_t n)
{
    ui.fbank = (uint8_t)(n % NFBANK);
    ui.dirty = 1;
}

/* ------------------------------------------------------------- the motion loop --- */
static void mo_toggle(void)                             /* record -> loop -> stop -> loop ... */
{
    if (ui.mo.state == MO_IDLE || (ui.mo.state == MO_STOP && !ui.mo.n)) {
        ui.mo.state = MO_REC;
        ui.mo.n = 0;
        ui.mo.t0 = fm1_ms;
    } else if (ui.mo.state == MO_REC) {
        ui.mo.len = fm1_ms - ui.mo.t0;
        ui.mo.state = ui.mo.n && ui.mo.len >= 200u ? MO_LOOP : MO_IDLE;
        ui.mo.t0 = fm1_ms;
        ui.mo.i = 0;
    } else if (ui.mo.state == MO_LOOP)
        ui.mo.state = MO_STOP;
    else {
        ui.mo.state = MO_LOOP;
        ui.mo.t0 = fm1_ms;
        ui.mo.i = 0;
    }
}
static void mo_play(void)                               /* the loop's events as they come round */
{
    uint32_t now;
    if (ui.mo.state != MO_LOOP || !ui.mo.len)
        return;
    now = (fm1_ms - ui.mo.t0) / 10u;
    if (now * 10u >= ui.mo.len) {                       /* round again */
        ui.mo.t0 += ui.mo.len;
        ui.mo.i = 0;
        now = (fm1_ms - ui.mo.t0) / 10u;
    }
    while (ui.mo.i < ui.mo.n && mo_ev[ui.mo.i].t <= now) {
        const typeof(mo_ev[0]) *e = &mo_ev[ui.mo.i++];
        if (e->kind == EV_GATE_ON)
            ui.pad_gate[e->a] = 1, ui.latched[e->a] = 1;
        else if (e->kind == EV_GATE_OFF)
            ui.pad_gate[e->a] = 0, ui.latched[e->a] = 0;
        else if (e->kind == EV_V)
            pt.v[e->a][e->c] = vp_clamp(e->a, e->c, e->b);
        else if (e->kind == EV_G)
            pt.g[e->c] = (int16_t)clamp(e->b, GDESC[e->c].min, GDESC[e->c].max);
        ui.dirty = 1;
    }
}

/* ------------------------------------------------------------- buttons --- */
static void menu_go(uint32_t m)
{
    if (ui.menu == m)                                   /* its button again: home */
        m = MN_HOME;
    if (ui.menu == MN_SETUP && m != MN_SETUP)
        settings_save();
    ui.menu = (uint8_t)m;
    ui.page = 0;
    ui.altmask = 0;
    ui.save_pad = 0;
    ui.kbm = ui.nsel = ui.pdown = 0;
    if (m == MN_PRESETS)
        ui.pre_sel = ui.preset;
    menu_build();
    ui.dirty = 1;
}

static void page_turn(int32_t d)                        /* < >: pages (PRESETS: themes; HOME: the next preset) */
{
    if (ui.menu == MN_HOME) {
        uint32_t n = ui.preset < NPRESET ? (ui.preset + NPRESET + (uint32_t)d) % NPRESET : 0u;
        char b[20];
        preset_load(n);
        preset_name(n, b, sizeof b);
        ui_say(b, 0);
    } else if (ui.menu == MN_FADERS) {
        fbank_go(ui.fbank + NFBANK + (uint32_t)d);
    } else if (ui.menu == MN_PRESETS) {
        uint32_t np = NTHEME + 1u, th = (ui.pre_sel / PRESET_PAGE + np + (uint32_t)d) % np;
        ui.pre_sel = (uint8_t)(ui.preset / PRESET_PAGE == th ? ui.preset : th * PRESET_PAGE);
        ui.save_pad = 0;
        if (th < NTHEME && ui.preset / PRESET_PAGE != th)
            preset_load(ui.pre_sel);                    /* (a theme's first preset, heard at once) */
    } else {
        uint32_t n = MENUS[ui.menu].n;
        ui.page = (uint8_t)((ui.page + n + (uint32_t)d) % n);
        ui.altmask = 0;
        menu_build();
    }
    ui.dirty = 1;
}

static void buttons_poll(void)
{
    uint32_t rel, pr = fm1_input_edges(&rel), b, m;
    for (b = 0; b < FM1_NBTN; b++) {
        if (!((pr >> b) & 1u))
            continue;
        ui.btn_t[b] = fm1_ms | 1u;
        if (ui.help && b != B_KNOBBANK)
            continue;
        switch (b) {
        case B_LEFT:
        case B_RIGHT:
            page_turn(b == B_RIGHT ? 1 : -1);
            break;
        case B_OCTDN:
        case B_OCTUP:
            ui.oct = (int8_t)clamp(ui.oct + (b == B_OCTUP ? 1 : -1), -3, 3);
            ui_say(ui.oct ? "TRANSPOSE " : "TRANSPOSE 0", ui.oct < 0 ? "-" : ui.oct ? "+" : 0);
            break;
        case B_FADERBANK:                               /* the faders' banks */
            menu_go(MN_FADERS);
            break;
        case B_PLAY:                                    /* FREE-PLAY: the keys wake the drones */
            free_play(!ui.free);
            break;
        case B_REC:                                     /* HOLD (FREE-PLAY): notes stay; a key again lets go */
            ui.hold ^= 1u;
            if (!ui.hold)
                free_release_all();
            ui_say(ui.hold ? "HOLD" : "HOLD OFF", 0);
            break;
        default:                                        /* a menu's button (PATCH, KNOB BANK: on release / hold) */
            for (m = MN_SOUND; m < MN_N; m++)
                if (MENUS[m].name && MENUS[m].btn == b && m != MN_SETUP)
                    menu_go(m);
        }
    }
    /* KNOB BANK: tap = PRESETS, held = HELP; PATCH: tap = HOME, held = SETUP */
    if ((rel >> B_KNOBBANK) & 1u && ui.btn_t[B_KNOBBANK]) {
        if (!ui.help)
            menu_go(MN_PRESETS);
        ui.help = 0;
        ui.btn_t[B_KNOBBANK] = 0;
        ui.dirty = 1;
    }
    if (ui.btn_t[B_KNOBBANK] && !ui.help && (fm1_in.buttons >> B_KNOBBANK) & 1u && (int32_t)(fm1_ms - ui.btn_t[B_KNOBBANK]) >= 500) {
        ui.help = 1;
        ui.help_panel = 0;
        ui.help_t = fm1_ms;
    }
    if ((rel >> B_PATCH) & 1u && ui.btn_t[B_PATCH]) {
        ui.btn_t[B_PATCH] = 0;
        menu_go(MN_HOME);
    }
    if (ui.btn_t[B_PATCH] && (fm1_in.buttons >> B_PATCH) & 1u && (int32_t)(fm1_ms - ui.btn_t[B_PATCH]) >= 500) {
        ui.btn_t[B_PATCH] = 0;
        menu_go(MN_SETUP);
    }
}

/* ------------------------------------------------------------- knobs --- */
/* PRESETS: K1-K8 are filters to hear the presets through: K1-K4 a filter type each (turned: that type, its
 * cutoff), K5-K8 its resonance, drive, movement and the movement's rate */
static const struct { const char *name; uint8_t mode, id; } LENS[8] = {
    {"LADDER", FM_LADDER, G_FFL}, {"POLIVOKS", FM_POLIVOKS, G_FFL}, {"BAND", FM_BAND, G_FFL}, {"DIST", FM_DIST, G_FFL},
    {"RESO", NO, G_FRL}, {"DRIVE", NO, G_FDRIVE}, {"MOVE", NO, G_FMD}, {"M RATE", NO, G_FMR}};
static void lens_turn(uint32_t k, int32_t s)
{
    item_t it = {IK_G, LENS[k].id, 0, NO, 0, NO, 0, NO};
    char b[8];
    if (LENS[k].mode != NO && pt.g[G_FMODE] != LENS[k].mode) {
        pt.g[G_FMODE] = LENS[k].mode;
        mo_add(EV_G, 0, G_FMODE, LENS[k].mode);
    }
    it_turn(&it, k, s);
    fmt_int(b, it_get(&it));
    big_show(k, LENS[k].name, b);
}

static void knobs_poll(void)
{
    uint32_t k;
    int32_t s;
    for (k = 0; k < 8u; k++) {
        if ((s = fm1_enc_take(k)) == 0)
            continue;
        if (ui.help) {                                  /* (HELP: any knob turns its panels) */
            ui.help_panel = (uint8_t)(ui.help_panel + (s > 0 ? 1 : -1));
            ui.help_t = fm1_ms;
            continue;
        }
        if (ui.dlg)
            continue;
        if (ui.menu == MN_PRESETS)
            lens_turn(k, s);
        else if (ui.menu == MN_HOME || ui.menu == MN_FADERS)
            pitch_turn(k, s);
        else if (ui.it[k].kind != IK_NONE && !it_onpads(&ui.it[k])) {
            it_turn(&ui.it[k], k, s);
            show_item(k, &ui.it[k]);
        }
    }
}

/* ------------------------------------------------------------- LEDs --- */
/* eight distinct colours, one per column: the creature, its pad, its column in every menu */
static const uint8_t VCOL[NV][3] = {{255, 64, 64}, {255, 150, 20}, {255, 230, 30}, {70, 230, 70},
                                    {20, 220, 255}, {70, 120, 255}, {170, 90, 255}, {255, 90, 210}};
static void ui_leds(void)
{
    uint32_t v, b, sc = 4u + settings.bright * 12u;     /* pad brightness of 256 */
#define PAD(p, r, g, bl, k) padled_set(p, (uint8_t)((r) * (k) * sc >> 16), (uint8_t)((g) * (k) * sc >> 16), \
                                       (uint8_t)((bl) * (k) * sc >> 16))
#define PADC(p, c, k) PAD(p, VCOL[c][0], VCOL[c][1], VCOL[c][2], k)
    for (v = 0; v < 16u; v++)
        padled_set(v, 0, 0, 0);
    if (ui.dlg) {                                       /* a question: only red and green */
        PAD(14, 255, 0, 0, 256);
        PAD(15, 0, 255, 0, 256);
    } else if (ui.menu == MN_HOME && ui.kbm) {          /* KB: the selected white; none yet: all blink */
        uint32_t bl = (fm1_ms >> 8) & 1u;
        for (v = 0; v < NV; v++)
            if ((ui.nsel >> v) & 1u)
                PAD(v, 255, 255, 255, 256);
            else
                PADC(v, v, ui.nsel || bl ? 40 : 230);
        PAD(8, 60, 220, 255, bl ? 256 : 120);
    } else if (ui.menu == MN_HOME) {
        PAD(8, 60, 220, 255, 60);                       /* KB */
        for (v = 0; v < NV; v++) {
            int32_t l = vs[v].level, k = ui.pad_gate[v] ? 120 + (l >> 8) : vs[v].stage ? 10 + (l >> 9) : 10;   /* on: bright; fading: dim */
            if (VTYPE[v] == VT_VCO && kb.padhold[v - 3u])
                k = k < 160 ? 160 : k;
            PADC(v, v, k > 256 ? 256 : k);
        }
        b = pt.g[G_RUN] && md.pp < 0x40000000u;         /* P: the pulser's beat; H: white; C: pink */
        PAD(13, b ? 255 : 30, b ? 255 : 20, 10, 256);
        PAD(14, 120, 120, 140, 256);
        PAD(15, 255, 60, 200, ui.chaos_held ? 256 : 120);
    } else if (ui.menu == MN_PRESETS) {                 /* the picker: 16 presets, the one playing white */
        uint32_t base = (ui.pre_sel / PRESET_PAGE) * PRESET_PAGE;
        for (v = 0; v < 16u; v++) {
            uint32_t n = base + v, has = n < NPRESET || ((user_used >> (n - NPRESET)) & 1u);
            if (n == ui.preset)
                PAD(v, 255, 255, 255, 256);
            else
                PADC(v, v & 7u, has ? 150 : 14);
        }
    } else if (ui.menu == MN_FADERS) {                  /* the banks, the one in use white */
        for (v = 0; v < NFBANK; v++)
            if (v == ui.fbank)
                PAD(v, 255, 255, 255, 256);
            else
                PADC(v, v, 120);
    } else
        for (v = 0; v < 8u; v++) {                      /* a menu: the pads that do something, lit by their value */
            const item_t *it = &ui.it[v];
            if (it->kind == IK_NONE)
                continue;
            if (it_onpads(it)) {
                const pdesc_t *d = it_desc(it);
                int on = d->fmt == F_ONOFF ? it_get(it) != 0 : it_get(it) == d->min;
                PADC(v, v, on ? 256 : 14);
                if (d->fmt == F_ENUM)
                    PADC(8u + v, v, on ? 14 : 256);
            } else if (it->alt != NO)                   /* white: the knob edits the second (RATE) */
                PAD(v, (ui.altmask >> v) & 1u ? 255 : VCOL[v][0], (ui.altmask >> v) & 1u ? 255 : VCOL[v][1],
                    (ui.altmask >> v) & 1u ? 255 : VCOL[v][2], 220);
            else {
                if (it->pid != NO) {
                    int32_t x = it->kind == IK_G ? pt.g[it->pid] : pt.v[it->voice][it->pid];
                    int on = (int)(((uint32_t)x >> it->pbit) & 1u) ^ it->pinv;
                    if (it->pid == G_SGATES && pt.g[G_RUN] && md.step == it->pbit)
                        PAD(v, 255, 255, 255, 256);     /* (the step playing) */
                    else
                        PADC(v, v, on ? 256 : 14);
                }
                if (it->bid != NO)
                    PADC(8u + v, v, ((uint32_t)pt.v[it->voice][it->bid] >> it->pbit) & 1u ? 256 : 14);
            }
        }
    for (b = MN_SOUND; b < MN_N; b++)
        if (MENUS[b].name && b != MN_SETUP)
            fm1_led_key(MENUS[b].btn, ui.menu == b);
    fm1_led_key(B_KNOBBANK, ui.help || ui.menu == MN_PRESETS);
    fm1_led_key(B_PATCH, ui.menu == MN_SETUP);
    fm1_led_key(B_PLAY, ui.free);
    fm1_led_key(B_REC, ui.hold);
    fm1_led_key(B_FADERBANK, ui.menu == MN_FADERS);
    fm1_led_key(B_OCTDN, ui.oct < 0);
    fm1_led_key(B_OCTUP, ui.oct > 0);
#undef PADC
#undef PAD
}

static void ui_init(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        ui.fad_last[i] = 0xFF;
    for (i = 0; i < NV; i++)
        ui.fkey[i] = -128, ui.forder[i] = (uint8_t)i;
    kb.cents[0] = kb_cents(12);                         /* the VCOs' note before a key: C3, G3 */
    kb.cents[1] = kb_cents(19);
    kb.played = 1;
    ui.menu = MN_PRESETS;                               /* power-on: pick a preset */
    ui.page = 0;
    ui.pre_sel = ui.preset;
    ui.dirty = 1;
}

static void ui_input(void)
{
    uint32_t v;
    pads_poll();
    keys_poll();
    wheels_poll();
    buttons_poll();
    knobs_poll();
    faders_poll();
    picker_tick();
    chaos_tick();
    mo_play();
    speaker = settings.speaker;
    for (v = 0; v < NV; v++)
        gate_in[v] = ui.pad_gate[v] ? 1u : ui.hush[v] ? 2u : 0u;
    free_vco = ui.free;
}

/* ------------------------------------------------------------- FREE-PLAY --- */
/* the keys wake the drones one after another, each tuned to its key; HOLD: they stay, the key again lets go */
static void free_on(uint32_t v, int32_t key, uint32_t vel)
{
    ui.fkey[v] = (int8_t)key;
    voice_note(v, key);
    gate_vel[v] = (uint8_t)vel;
    ui.pad_gate[v] = ui.latched[v] = 1;
}
static void free_off(uint32_t v)
{
    ui.fkey[v] = -128;
    ui.pad_gate[v] = ui.latched[v] = 0;
}
static void free_release_all(void)
{
    uint32_t v;
    for (v = 0; v < NV; v++)
        if (ui.fkey[v] != -128)
            free_off(v);
}
static void free_play(uint32_t on)
{
    uint32_t v;
    ui.free = (uint8_t)on;
    for (v = 0; v < NV; v++)
        ui.fkey[v] = -128;
    if (on)
        for (v = 0; v < NV; v++)
            ui.pad_gate[v] = ui.latched[v] = 0;
    ui_say(on ? "FREE PLAY" : "SEPARATE", 0);
}
static void free_key(int32_t key, uint32_t down, uint32_t vel)
{
    uint32_t v, n = 0, i;
    for (v = 0; v < NV; v++)
        if (ui.fkey[v] == key) {
            if (down ? ui.hold : !ui.hold)              /* HOLD: a held key again lets go */
                free_off(v);
            return;
        }
    if (!down)
        return;
    for (v = 0; v < NV && ui.fkey[v] != -128; v++)      /* the first one asleep; else the oldest */
        ;
    if (v == NV)
        v = ui.forder[0];
    for (i = 0; i < NV; i++)                            /* (the order: v to the end) */
        if (ui.forder[i] != v)
            ui.forder[n++] = ui.forder[i];
    ui.forder[NV - 1u] = (uint8_t)v;
    free_on(v, key, vel);
}

/* ------------------------------------------------------------- keys --- */
/* the keys held now (low to high): one tunes every voice of m, several are spread over them in turn */
static void kb_held(int32_t key, uint32_t down)
{
    uint32_t i, j = 0;
    for (i = 0; i < ui.kbn; i++)
        if (ui.kbk[i] != key)
            ui.kbk[j++] = ui.kbk[i];
    ui.kbn = (uint8_t)j;
    if (!down || ui.kbn >= NV)
        return;
    for (i = ui.kbn; i > 0u && ui.kbk[i - 1u] > key; i--)
        ui.kbk[i] = ui.kbk[i - 1u];
    ui.kbk[i] = (int8_t)key;
    ui.kbn++;
}
static void kb_spread(uint32_t m, uint32_t vel)
{
    uint32_t v, j = 0;
    char b[8];
    for (v = 0; v < NV; v++)
        if ((m >> v) & 1u) {
            voice_note(v, ui.kbk[j++ % ui.kbn]);
            voice_wake(v, vel);
            ui.ignore_rel[v] = !ui.kbm;                 /* (the held pad stays on) */
        }
    cents_text(b, sizeof b, (kb_cents(ui.kbk[ui.kbn - 1u]) + 50) / 100 * 100);
    big_show(8, ui.kbn > 1u ? "SPREAD" : "NOTE", b);
}

/* a key: tunes the voices selected (KB) or held (a voice pad), else FREE-PLAY or the VCOs 4 + 5 */
static void key_event(int32_t key, uint32_t down, uint32_t vel, uint32_t side)
{
    uint32_t m = ui.menu == MN_HOME ? (ui.kbm ? ui.nsel : ui.pdown) : 0u;
    kb_held(key, down);
    if (m) {
        if (down && ui.kbn)
            kb_spread(m, vel);
        else if (!down)
            kb_release(key);                            /* (a key pressed before) */
    } else if (ui.free)
        free_key(key, down, vel);
    else if (down)
        kb_press(key, side, vel);
    else
        kb_release(key);
}
static void keys_poll(void)
{
    uint64_t now = fm1_in.keys, ch = now ^ ui.keys;
    uint32_t k;
    ui.keys = now;
    if (fm1_in.frames < 100u)                           /* (the first readings settle) */
        return;
    for (k = 0; ch && k < 37u; k++)
        if ((ch >> k) & 1u)
            key_event((int32_t)k + 12 * ui.oct, (now >> k) & 1u, fm1_in.key_vel[k], k >= 18u);
}

/* USB MIDI in: notes as the keys */
static void midi_poll(void)
{
    while (mi_r != mi_w) {
        uint32_t pkt, cin, n, vel;
        RING_PUBLISH();
        pkt = midi_in_q[mi_r % MQ];
        RING_PUBLISH();
        mi_r++;
        cin = pkt & 15u, n = (pkt >> 16) & 127u, vel = (pkt >> 24) & 127u;
        if (cin == 8u || cin == 9u)
            key_event((int32_t)n - 36, cin == 9u && vel, vel, n >= 54u);
    }
}
