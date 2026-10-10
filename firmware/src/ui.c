/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Controls. The screen is laid out as the panel: K1..K8 above the 8 pad columns, so column i is voice i
 * everywhere. Main screen: Ki = voice i's pitch, top pad i its gate. Every button opens one menu; a menu's
 * pages (< >) show 8 values in the 8 columns, the knob above each sets it. Dialogs: green pad OK, red pad
 * CANCEL. */
enum { MN_HOME, MN_VOICE, MN_ENV, MN_MOD, MN_MIX, MN_VOLUME, MN_TUNING, MN_LFO, MN_SEQ, MN_FX, MN_KEYS,
       MN_PRESETS, MN_SETUP, MN_N };
static const char *const MENU_NAME[MN_N] = {"", "VOICE", "ENV", "MOD", "MIX", "VOLUME", "TUNING", "LFO", "SEQ",
                                            "FX", "KEYS", "PRESETS", "SETUP"};
/* button ids (smk_input.h) */
enum { B_KNOBBANK, B_FADERBANK, B_LEFT, B_RIGHT, B_OCTDN, B_OCTUP, B_ARP, B_REPEAT, B_SCALE, B_CHORD, B_GLOBE,
       B_BT, B_PATCH, B_PARA, B_FX, B_SEQ, B_PLAY, B_REC };
static const int8_t BTN_MENU[18] = {-1, MN_VOLUME, -1, -1, -1, -1, MN_KEYS, MN_MOD, MN_TUNING, MN_ENV, MN_MIX,
                                    MN_LFO, MN_PRESETS, MN_VOICE, MN_FX, MN_SEQ, -1, -1};

enum { IK_NONE, IK_V, IK_G, IK_SET, IK_LOAD, IK_SAVE, IK_MORPH, IK_STEPREC };
typedef struct { uint8_t kind, id, voice; } item_t;
enum { SET_BRIGHT, SET_CONSOLE, SET_SPEAKER, SET_N };
enum { DLG_NONE, DLG_LOAD, DLG_SAVE, DLG_PRESET };
enum { MO_IDLE, MO_REC, MO_LOOP, MO_STOP };
#define MO_N 512u

static struct {
    uint8_t menu, page, preset, pre_sel;   /* preset: the one chosen; pre_sel: the one shown on PRESETS */
    int8_t oct;
    item_t it[8];
    char page_name[16];
    uint32_t enc_t[FM1_NENC];
    char msg[28];
    uint32_t msg_until;
    uint8_t dirty;
    uint8_t latched[NV], ignore_rel[NV], pad_gate[NV];
    uint32_t t_press[NV];
    uint64_t keys;
    uint8_t fad_last[4];
    uint32_t btn_t[FM1_NBTN];              /* buttons held since (ms | 1), 0 = up */
    uint8_t help, help_panel;
    uint32_t help_t;
    uint32_t tap_t;
    uint8_t dlg, dlg_arg;                  /* a dialog waiting for the green or red pad */
    uint8_t chaos_held, chaos_used;
    uint8_t morph_on, morph_slot, morph[NSCENE];
    uint8_t focus;                         /* the voice touched last (VOICE opens on it) */
    struct { uint8_t state; uint16_t n, i; uint32_t t0, len, last; } mo;   /* the motion loop */
} ui;
static struct { uint16_t t; uint8_t kind, a, c; int16_t b; } mo_ev[MO_N];   /* t in 10 ms */
enum { EV_GATE_ON = 1, EV_GATE_OFF, EV_V, EV_G };

static void keys_poll(void);
static void midi_poll(void);
static void big_show(uint32_t col, const char *label, const char *val);

static void ui_say(const char *a, const char *b)
{
    str_cpy(ui.msg, a, sizeof ui.msg);
    if (b) {
        uint32_t n = str_len(ui.msg);
        str_cpy(ui.msg + n, b, sizeof ui.msg - n);
    }
    ui.msg_until = fm1_ms + 1500u;
    ui.dirty = 1;
}

/* ------------------------------------------------------------- motion loop --- */
static void mo_add(uint32_t kind, uint32_t a, uint32_t c, int32_t b)
{
    if (ui.mo.state != MO_REC || ui.mo.n >= MO_N)
        return;
    mo_ev[ui.mo.n].t = (uint16_t)((fm1_ms - ui.mo.t0) / 10u);
    mo_ev[ui.mo.n].kind = (uint8_t)kind, mo_ev[ui.mo.n].a = (uint8_t)a, mo_ev[ui.mo.n].c = (uint8_t)c;
    mo_ev[ui.mo.n].b = (int16_t)b;
    ui.mo.n++;
}

/* ------------------------------------------------------------- the menus --- */
static void it_set(uint32_t k, uint32_t kind, uint32_t id, uint32_t voice)
{
    ui.it[k].kind = (uint8_t)kind, ui.it[k].id = (uint8_t)id, ui.it[k].voice = (uint8_t)voice;
}
static void row_v(uint32_t id)                          /* one parameter of all 8 voices */
{
    uint32_t v;
    for (v = 0; v < NV; v++)
        it_set(v, VDESC[VTYPE[v]][id].label ? IK_V : IK_NONE, id, v);
}
static void row_g(const uint8_t *ids)                   /* 8 global parameters (0xFF: none) */
{
    uint32_t k;
    for (k = 0; k < 8u; k++)
        it_set(k, ids[k] == 0xFF ? IK_NONE : IK_G, ids[k], 0);
}

/* the menu's page into ui.it / ui.page_name; returns the number of pages */
static uint32_t menu_build(void)
{
    static const uint8_t TUN[8] = {G_TSET, G_ROOT, G_SNAP, G_FINE, G_GLIDE, 0xFF, 0xFF, 0xFF};
    static const uint8_t LFO[8] = {G_LARATE, G_LAWAVE, G_LBRATE, G_LBWAVE, G_JXOFS, G_JYOFS, G_DRIFT, 0xFF};
    static const uint8_t SEQ[8] = {G_SEQ1, G_SEQ2, G_SEQ3, G_SEQ4, G_SEQ5, G_STAGES, G_PULSER, G_RUN};
    static const uint8_t FLT[8] = {G_FFL, G_FRL, G_FFR, G_FRR, G_FMODE, G_FLINK, G_FDRIVE, G_FCVAMT};
    static const uint8_t FXS[8] = {G_FXL, G_FXLX, G_FXLY, G_FXLZ, G_FXR, G_FXRX, G_FXRY, G_FXRZ};
    static const uint8_t FX3[8] = {G_BLEND, G_BLENDR, G_FCVSRC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static const uint8_t MIX[8] = {G_MASTER, G_DRIFT, G_WIDTH, G_BLEND, G_BLENDR, 0xFF, 0xFF, 0xFF};
    static const uint8_t KEY[8] = {G_KEYS, G_ARP, G_ARATE, G_AOCT, G_ALATCH, G_GLIDE, G_VIBD, G_VIBR};
    uint32_t n = 1, k, p = ui.page, v;
    const char *name = MENU_NAME[ui.menu];
    for (k = 0; k < 8u; k++)
        it_set(k, IK_NONE, 0, 0);
    switch (ui.menu) {
    case MN_VOICE:                                      /* a page a voice: its own eight */
        n = NV;
        v = p % NV;
        if (VTYPE[v] == VT_CLASSIC)
            for (k = 0; k < 8u; k++)
                it_set(k, IK_V, CL_TUNE1 + k, v);
        else if (VTYPE[v] == VT_NOISE)
            for (k = 0; k < 8u; k++)
                it_set(k, IK_V, NZ_PITCH + k, v);
        else {
            static const uint8_t VC[8] = {VC_WAVE, VC_SHAPE, VC_OCT, VC_TUNE, VC_FM, VC_SYNC, VC_EG, VP_TONE};
            for (k = 0; k < 8u; k++)
                it_set(k, (k == 5u && v == 3u) ? IK_NONE : IK_V, VC[k], v);
        }
        {
            static char nm[2];
            nm[0] = (char)('1' + v);
            name = nm;
        }
        break;
    case MN_ENV: {
        static const char *const N[5] = {"ATTACK", "RELEASE", "GATE", "DECAY", "SUSTAIN"};
        static const uint8_t ID[5] = {VP_ATT, VP_RLS, VP_GSRC, VC_DEC, VC_SUS};
        n = 6;
        if (p < 5u) {
            if (p < 3u)
                row_v(ID[p]);
            else
                it_set(3, IK_V, ID[p], 3), it_set(4, IK_V, ID[p], 4);
            name = N[p];
        } else {
            it_set(3, IK_V, VC_EG, 3), it_set(4, IK_V, VC_EG, 4);
            name = "VCO EG";
        }
        break;
    }
    case MN_MOD:
        n = 6;
        if (p >= 4u) {                                  /* the subharmonics (classic), the sub-oscillators (VCO) */
            for (v = 0; v < NV; v++)
                if (VTYPE[v] == VT_CLASSIC)
                    it_set(v, IK_V, p == 4u ? CL_SUB : CL_SUBDIV, v);
                else if (VTYPE[v] == VT_VCO && p == 4u)
                    it_set(v, IK_V, VC_SUB, v);
            name = p == 4u ? "SUB" : "SUB DIV";
            break;
        }
        if (p < 2u)
            row_v(p ? VP_MAMT : VP_MSRC), name = p ? "MOD AMT" : "MOD SRC";
        else
            it_set(2, IK_V, p == 2u ? NZ_SHAMT : NZ_SHCLK, 2), it_set(7, IK_V, p == 2u ? NZ_SHAMT : NZ_SHCLK, 7),
            name = p == 2u ? "S&H AMT" : "S&H CLK";
        break;
    case MN_MIX:
        n = 3;
        if (p < 2u)
            row_v(p ? VP_TONE : VP_PAN), name = p ? "TONE" : "PAN";
        else
            row_g(MIX), name = "MASTER";
        break;
    case MN_VOLUME:
        row_v(VP_VOL);
        name = "VOLUME";
        break;
    case MN_TUNING:
        n = 2;
        if (p)
            row_v(VP_PITCH), name = "PITCH";
        else
            row_g(TUN), name = "TUNING";
        break;
    case MN_LFO:
        row_g(LFO);
        name = "LFO A B";
        break;
    case MN_SEQ:
        row_g(SEQ);
        name = "STEPS";
        break;
    case MN_FX:
        n = 3;
        row_g(p == 0u ? FLT : p == 1u ? FXS : FX3);
        name = p == 0u ? "FILTER" : p == 1u ? "FX L  R" : "BLEND";
        break;
    case MN_KEYS:
        n = 2;
        if (p)
            it_set(0, IK_STEPREC, 0, 0), name = "STEP REC";
        else
            row_g(KEY), name = "KEYS";
        break;
    case MN_PRESETS:                                    /* 0: the presets; 1 LOAD, 2 SAVE, 3 MORPH: the 8 slots */
        n = 4;
        for (k = 0; p && k < NSCENE; k++)
            it_set(k, p == 1u ? IK_LOAD : p == 2u ? IK_SAVE : IK_MORPH, 0, k);
        name = p == 0u ? "PRESETS" : p == 1u ? "LOAD" : p == 2u ? "SAVE" : "MORPH";
        break;
    case MN_SETUP:
        it_set(0, IK_SET, SET_BRIGHT, 0), it_set(1, IK_SET, SET_CONSOLE, 0), it_set(2, IK_SET, SET_SPEAKER, 0);
        name = "SETUP";
        break;
    default:
        break;
    }
    if (ui.page >= n)
        ui.page = 0;
    str_cpy(ui.page_name, name, sizeof ui.page_name);
    return n;
}

static const pdesc_t *it_desc(const item_t *it)
{
    static const char *const N_OUT[2] = {"LINE", "SPEAKER"};
    static const pdesc_t SET[SET_N] = {{"LIGHT", 1, 8, 4, F_NUM, 0}, {"CONSOLE", 0, 1, 1, F_ONOFF, 0},
                                       {"OUTPUT", 0, 1, 0, F_ENUM, N_OUT}};
    static const pdesc_t SLOT = {"SLOT", 0, 127, 0, F_NUM, 0};
    switch (it->kind) {
    case IK_V: return &VDESC[VTYPE[it->voice]][it->id];
    case IK_G: return &GDESC[it->id];
    case IK_SET: return &SET[it->id];
    case IK_LOAD: case IK_SAVE: case IK_MORPH: case IK_STEPREC: return &SLOT;
    default: return 0;
    }
}

static int32_t it_get(const item_t *it)
{
    switch (it->kind) {
    case IK_V: return pt.v[it->voice][it->id];
    case IK_G: return pt.g[it->id];
    case IK_SET: return it->id == SET_BRIGHT ? settings.bright : it->id == SET_CONSOLE ? settings.console : settings.speaker;
    case IK_MORPH: return ui.morph[it->voice];
    case IK_LOAD: case IK_SAVE: return (scene_used >> it->voice) & 1u ? 127 : 0;
    case IK_STEPREC: return kb.seqlen * 8;
    default: return 0;
    }
}

static void it_label(const item_t *it, char *b, uint32_t n)
{
    const pdesc_t *d = it_desc(it);
    if (it->kind >= IK_LOAD && it->kind <= IK_MORPH) {
        str_cpy(b, it->kind == IK_LOAD ? "LOAD " : it->kind == IK_SAVE ? "SAVE " : "MORPH ", n);
        b[str_len(b) + 1u < n ? str_len(b) + 1u : 0] = 0;
        b[str_len(b)] = (char)('1' + it->voice);
        return;
    }
    if (it->kind == IK_STEPREC) {
        str_cpy(b, "STEP REC", n);
        return;
    }
    str_cpy(b, d ? d->label : "", n);
}

static void cents_text(char *b, uint32_t n, int32_t c)   /* cents above MIDI 0 -> "E3+12" */
{
    int32_t m = (c + 50) / 100, dc = c - m * 100;
    char t[8];
    str_cpy(b, NOTE_NAMES[((m % 12) + 12) % 12], n);
    fmt_int(t, m / 12 - 1);
    str_cpy(b + str_len(b), t, n - str_len(b));
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
    if (it->kind == IK_LOAD || it->kind == IK_SAVE) {
        str_cpy(b, v ? "USED" : "FREE", n);
        return;
    }
    if (it->kind == IK_MORPH || it->kind == IK_STEPREC) {
        if (it->kind == IK_STEPREC) {
            str_cpy(b, kb.rec ? "REC " : "", n);
            fmt_int(b + str_len(b), kb.seqlen);
        } else
            fmt_int(b, v);
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
    case F_BITS: {
        uint32_t k;
        for (k = 0; k < 5u && k + 1u < n; k++)
            b[k] = (v >> k) & 1 ? '#' : '-';
        b[k] = 0;
        break;
    }
    default:
        fmt_int(b, v);
    }
}

static int it_binary(const item_t *it)                   /* on / off or two choices: set on the pads */
{
    const pdesc_t *d;
    if (it->kind != IK_V && it->kind != IK_G && it->kind != IK_SET)
        return 0;
    d = it_desc(it);
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

static void autogate(void)                              /* a preset's voices start (VCOs: only by keys) */
{
    uint32_t v, bits = (uint32_t)pt.g[G_AUTOGATE];
    for (v = 0; v < NV; v++) {
        uint32_t on = VTYPE[v] != VT_VCO && ((bits >> v) & 1u);
        ui.latched[v] = ui.pad_gate[v] = (uint8_t)on;
        ui.ignore_rel[v] = 0;
    }
    kb.padhold[0] = kb.padhold[1] = 0;
}
static void preset_load(uint32_t n)
{
    preset_make(n % NPRESET, &pt);
    ui.preset = (uint8_t)(n % NPRESET);
    pulser_inc = 0;
    md.restart = 1;
    autogate();
    ui.morph_on = 0;
    ui.dirty = 1;
}

/* MORPH k: from the sound as it was (A) into scene k; numbers glide, switches flip half way */
static patch_t morph_a, morph_b;
static void morph_turn(uint32_t slot, int32_t s)
{
    uint32_t v, k, m;
    if (!ui.morph_on || ui.morph_slot != slot) {
        if (scene_load_to(slot, &morph_b)) {
            ui_say("EMPTY", 0);
            return;
        }
        if (!ui.morph_on)
            morph_a = pt;
        ui.morph_on = 1;
        ui.morph_slot = (uint8_t)slot;
        for (k = 0; k < NSCENE; k++)
            ui.morph[k] = 0;
    }
    ui.morph[slot] = (uint8_t)clamp(ui.morph[slot] + s * 3, 0, 127);
    m = ui.morph[slot];
    for (v = 0; v < NV; v++)
        for (k = 0; k < VP_N; k++) {
            const pdesc_t *d = &VDESC[VTYPE[v]][k];
            int32_t a = morph_a.v[v][k], b = morph_b.v[v][k];
            pt.v[v][k] = (int16_t)(d->fmt == F_ENUM || d->fmt == F_ONOFF || d->fmt == F_BITS ? (m < 64u ? a : b)
                                                                                             : a + (b - a) * (int32_t)m / 127);
        }
    for (k = 0; k < G_COUNT; k++) {
        const pdesc_t *d = &GDESC[k];
        int32_t a = morph_a.g[k], b = morph_b.g[k];
        pt.g[k] = (int16_t)(d->fmt == F_ENUM || d->fmt == F_ONOFF || d->fmt == F_BITS ? (m < 64u ? a : b)
                                                                                      : a + (b - a) * (int32_t)m / 127);
    }
}

static void dlg_open(uint32_t kind, uint32_t arg)
{
    ui.dlg = (uint8_t)kind;
    ui.dlg_arg = (uint8_t)arg;
    ui.dirty = 1;
}

static void it_turn(const item_t *it, uint32_t e, int32_t s)
{
    const pdesc_t *d = it_desc(it);
    int32_t v;
    ui.dirty = 1;
    switch (it->kind) {
    case IK_SET:
        if (it->id == SET_BRIGHT)
            settings.bright = (uint8_t)clamp(settings.bright + s, 1, 8);
        else
            settings.console = s > 0;
        return;
    case IK_LOAD:
        if ((scene_used >> it->voice) & 1u)
            dlg_open(DLG_LOAD, it->voice);
        else
            ui_say("EMPTY", 0);
        return;
    case IK_SAVE:
        dlg_open(DLG_SAVE, it->voice);
        return;
    case IK_MORPH:
        morph_turn(it->voice, s);
        return;
    case IK_STEPREC:                                    /* right: record (anew), left: stop */
        if (s > 0) {
            kb.seqlen = 0;
            kb.rec = 1;
        } else
            kb.rec = 0;
        return;
    case IK_V:
        v = pt.v[it->voice][it->id];
        if (d->fmt == F_TUNE && pt.g[G_SNAP]) {          /* to the next note of the tuning set */
            int32_t lo = GEN_LO[it->id - CL_TUNE1] * 100, c = lo + v, k;
            for (k = 0; k < (s > 0 ? s : -s); k++)
                c = tun_next((uint32_t)pt.g[G_TSET], (uint32_t)pt.g[G_ROOT], c, s > 0 ? 1 : -1);
            v = vp_clamp(it->voice, it->id, c - lo);
        } else
            v = vp_clamp(it->voice, it->id, v + accel(e, s, d->max - d->min));
        pt.v[it->voice][it->id] = (int16_t)v;
        mo_add(EV_V, it->voice, it->id, v);
        ui.morph_on = 0;
        return;
    case IK_G:
        v = (int16_t)clamp(pt.g[it->id] + accel(e, s, d->max - d->min), d->min, d->max);
        pt.g[it->id] = (int16_t)v;
        if (it->id == G_PULSER)
            pulser_inc = 0;                             /* the knob takes over from the tapped tempo */
        mo_add(EV_G, 0, it->id, v);
        ui.morph_on = 0;
        return;
    }
}

/* ------------------------------------------------------------- main screen: Ki = pitch of voice i --- */
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
    ui.focus = (uint8_t)v;
}

/* ------------------------------------------------------------- gates --- */
static void gate_press(uint32_t v)
{
    ui.focus = (uint8_t)v;
    if (VTYPE[v] == VT_VCO) {                           /* 4 / 5: hold the last key's note (keys only) */
        if (!kb.played) {
            ui_say("PLAY A KEY", 0);
            return;
        }
        kb.padhold[v - 3u] ^= 1u;
        return;
    }
    mo_add(EV_GATE_ON, v, 0, 0);
    if (ui.latched[v]) {                                /* a tap on a held drone ends it */
        ui.latched[v] = 0;
        ui.pad_gate[v] = 0;
        ui.ignore_rel[v] = 1;
        mo_add(EV_GATE_OFF, v, 0, 0);
        return;
    }
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

/* CHAOS (pad 16): the classic voices (or, pad 16 held + a top pad, that voice) re-rolled within the set */
static void chaos(uint32_t v0, uint32_t v1)
{
    uint32_t v, g, set = (uint32_t)pt.g[G_TSET], root = (uint32_t)pt.g[G_ROOT];
    static uint32_t seed = 0x2545F491u;
    seed ^= fm1_ms;
    for (v = v0; v < v1; v++) {
        int16_t *p = pt.v[v];
        if (VTYPE[v] == VT_CLASSIC)
            for (g = 0; g < 5u; g++) {
                int32_t lo = GEN_LO[g] * 100, span = (GEN_HI[g] - GEN_LO[g]) * 100;
                int32_t c = tun_next(set, root, lo + (int32_t)(xorshift(&seed) % (uint32_t)span) - 1, 1);
                p[CL_TUNE1 + g] = vp_clamp(v, CL_TUNE1 + g, c - lo);
            }
        else if (VTYPE[v] == VT_NOISE && v1 - v0 == 1u) {
            p[NZ_PITCH] = (int16_t)(20 + xorshift(&seed) % 100u);
            p[NZ_RATE] = (int16_t)(xorshift(&seed) % 110u);
            p[NZ_FMAMT] = (int16_t)(xorshift(&seed) % 128u);
            p[NZ_DIV] = (int16_t)(1 + xorshift(&seed) % 8u);
            p[NZ_MODE] = (int16_t)(xorshift(&seed) % 4u);
        } else if (VTYPE[v] == VT_VCO && v1 - v0 == 1u) {
            p[VC_WAVE] = (int16_t)(xorshift(&seed) % 6u);
            p[VC_SHAPE] = (int16_t)(xorshift(&seed) % 128u);
        }
    }
    ui_say("CHAOS!", 0);
}

/* HOLD ALL (pad 15): everything sounding stays on; again: all off */
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

/* the PULSER pad: a tap starts it, taps in a row set its tempo, held 0.6 s stops it */
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

static void dlg_answer(int ok)
{
    uint32_t a = ui.dlg_arg, kind = ui.dlg;
    char d[2] = {(char)('1' + a), 0};
    ui.dlg = DLG_NONE;
    ui.dirty = 1;
    if (!ok)
        return;
    if (kind == DLG_LOAD) {
        int rc = scene_load(a);
        ui_say(rc ? "EMPTY " : "LOADED ", d);
        if (!rc)
            settings.scene = (uint8_t)(a + 1u);
    } else if (kind == DLG_SAVE) {
        ui_say(scene_save(a) ? "SAVE FAILED " : "SAVED ", d);
    } else if (kind == DLG_PRESET) {
        preset_load(a);
        ui.menu = MN_HOME;
    }
}

static void pads_poll(void)
{
    int32_t e;
    while ((e = smk_pad_take()) >= 0) {
        uint32_t pad = (uint32_t)e >> 8, vel = (uint32_t)e & 255u;
        if (ui.dlg) {                                   /* a question: green OK (16), red CANCEL (9) */
            if (vel && (pad == 15u || pad == 8u))
                dlg_answer(pad == 15u);
            continue;
        }
        if (ui.menu != MN_HOME && !(ui.menu == MN_PRESETS && ui.page == 0u) && it_binary(&ui.it[pad & 7u])) {
            const item_t *it = &ui.it[pad & 7u];      /* on / off, two choices: the column's pads */
            const pdesc_t *d = it_desc(it);
            int32_t nv;
            if (!vel)
                continue;
            if (d->fmt == F_ONOFF)
                nv = pad < 8u ? !it_get(it) : it_get(it);
            else
                nv = pad < 8u ? d->min : d->max;
            if (it->kind == IK_V)
                pt.v[it->voice][it->id] = (int16_t)nv, mo_add(EV_V, it->voice, it->id, nv);
            else if (it->kind == IK_G)
                pt.g[it->id] = (int16_t)nv, mo_add(EV_G, 0, it->id, nv);
            else if (it->id == SET_CONSOLE)
                settings.console = (uint8_t)nv;
            else
                settings.speaker = (uint8_t)nv;
            {
                char lb[16], vb[16];
                it_label(it, lb, sizeof lb);
                it_text(it, vb, sizeof vb);
                big_show(pad & 7u, lb, vb);
            }
            continue;
        }
        if (ui.menu == MN_PRESETS && ui.page == 0u) {   /* the preset picker: pads 1..14, 15 cancel, 16 OK */
            if (!vel)
                continue;
            if (pad < NPRESET) {
                ui.pre_sel = (uint8_t)pad;
                preset_load(pad);                       /* (heard at once) */
            } else if (pad == 15u || pad == 14u) {
                ui.menu = MN_HOME;
                ui.dirty = 1;
            }
            continue;
        }
        if (pad < 8u) {                                 /* top row: the voices' gates */
            if (vel && ui.chaos_held) {
                chaos(pad, pad + 1u);
                ui.chaos_used = 1;
            } else if (vel) {
                gate_vel[pad] = (uint8_t)vel;
                gate_press(pad);
            } else
                gate_release(pad);
        } else if (pad < 13u) {                         /* 9..13: the sequencer's step gates */
            if (vel)
                pt.g[G_SGATES] ^= (int16_t)(1 << (pad - 8u));
            ui.dirty = 1;
        } else if (pad == 13u) {                        /* 14: PULSER */
            if (vel)
                tap_press();
            else if (fm1_ms - ui.tap_t >= 600u)
                pt.g[G_RUN] = 0;
        } else if (pad == 14u) {                        /* 15: HOLD ALL */
            if (vel)
                hold_all();
        } else {                                        /* 16: CHAOS (held + a top pad: that voice) */
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

/* ------------------------------------------------------------- faders: the effects and the filter --- */
static void faders_poll(void)
{
    static const char *const NAME[4] = {"FX L", "FX R", "SPACE", "FILTER"};
    uint32_t i;
    if (fm1_in.frames < 200u)
        return;
    for (i = 0; i < 4u; i++) {
        uint32_t v = fm1_in.fader[i];
        char b[8];
        if (ui.fad_last[i] == 0xFF || v == ui.fad_last[i]) {   /* takes over once it moves */
            ui.fad_last[i] = (uint8_t)v;
            continue;
        }
        ui.fad_last[i] = (uint8_t)v;
        if (i == 0u)
            pt.g[G_BLEND] = (int16_t)v;
        else if (i == 1u)
            pt.g[G_BLENDR] = (int16_t)v;
        else if (i == 2u)
            pt.g[G_FXLX] = pt.g[G_FXRX] = (int16_t)v;   /* the reverbs' decay, the delays' time */
        else
            pt.g[G_FFL] = pt.g[G_FFR] = (int16_t)v;     /* both filters (127: open) */
        mo_add(EV_G, 0, i == 0u ? G_BLEND : i == 1u ? G_BLENDR : i == 2u ? G_FXLX : G_FFL, (int32_t)v);
        fmt_int(b, (int32_t)v);
        big_show(8, NAME[i], b);
    }
}

/* ------------------------------------------------------------- buttons --- */
static void menu_go(uint32_t m)
{
    if (ui.menu == MN_SETUP && m != MN_SETUP)
        settings_save();
    if (ui.menu == m && m != MN_HOME)                   /* its button again: back home */
        m = MN_HOME;
    ui.menu = (uint8_t)m;
    ui.page = m == MN_VOICE ? ui.focus : 0;
    if (m == MN_PRESETS)
        ui.pre_sel = ui.preset;
    ui.dirty = 1;
}

static void mo_toggle(void)                             /* SEQ REC: record -> loop -> stop -> loop ... */
{
    if (ui.mo.state == MO_IDLE || (ui.mo.state == MO_STOP && !ui.mo.n)) {
        ui.mo.state = MO_REC;
        ui.mo.n = 0;
        ui.mo.t0 = fm1_ms;
        ui_say("REC", 0);
    } else if (ui.mo.state == MO_REC) {
        ui.mo.len = fm1_ms - ui.mo.t0;
        ui.mo.state = ui.mo.n && ui.mo.len >= 200u ? MO_LOOP : MO_IDLE;
        ui.mo.t0 = fm1_ms;
        ui.mo.i = 0;
        ui.mo.last = 0;
        ui_say(ui.mo.state == MO_LOOP ? "LOOP" : "NOTHING", 0);
    } else if (ui.mo.state == MO_LOOP) {
        ui.mo.state = MO_STOP;
        ui_say("LOOP STOP", 0);
    } else {
        ui.mo.state = MO_LOOP;
        ui.mo.t0 = fm1_ms;
        ui.mo.i = 0;
        ui.mo.last = 0;
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

static void buttons_poll(void)
{
    uint32_t rel, pr = fm1_input_edges(&rel), b;
    for (b = 0; b < FM1_NBTN; b++) {
        if ((pr >> b) & 1u)
            ui.btn_t[b] = fm1_ms | 1u;
        if (!((pr >> b) & 1u))
            continue;
        if (ui.help && b != B_KNOBBANK)
            continue;
        switch (b) {
        case B_LEFT:
        case B_RIGHT: {
            int32_t d = b == B_RIGHT ? 1 : -1;
            if (ui.menu == MN_HOME)                     /* home: the next preset (asks) */
                dlg_open(DLG_PRESET, (ui.preset + NPRESET + (uint32_t)d) % NPRESET);
            else if (ui.menu == MN_PRESETS && ui.page == 0u)
                ui.pre_sel = (uint8_t)((ui.pre_sel + NPRESET + (uint32_t)d) % NPRESET), preset_load(ui.pre_sel);
            else {
                uint32_t n = menu_build();
                ui.page = (uint8_t)((ui.page + n + (uint32_t)d) % n);
                if (ui.menu == MN_VOICE)
                    ui.focus = ui.page;
            }
            ui.dirty = 1;
            break;
        }
        case B_OCTDN:
            if (ui.oct > -3) ui.oct--;
            ui_say(ui.oct ? "TRANSPOSE " : "TRANSPOSE 0", ui.oct < 0 ? "-" : ui.oct ? "+" : 0);
            break;
        case B_OCTUP:
            if (ui.oct < 3) ui.oct++;
            ui_say(ui.oct ? "TRANSPOSE " : "TRANSPOSE 0", ui.oct < 0 ? "-" : ui.oct ? "+" : 0);
            break;
        case B_PLAY:                                    /* the PULSER: sequencer and the keys' arpeggio */
            pt.g[G_RUN] = (int16_t)!pt.g[G_RUN];
            if (pt.g[G_RUN])
                md.restart = 1;
            ui_say(pt.g[G_RUN] ? "PULSER RUN" : "PULSER STOP", 0);
            break;
        case B_REC:
            break;                                      /* (on release: a tap; held: a new recording) */
        case B_PATCH:
        case B_KNOBBANK:
            break;                                      /* (on release / hold) */
        default:
            if (BTN_MENU[b] >= 0)
                menu_go((uint32_t)BTN_MENU[b]);
        }
    }
    /* KNOB BANK: tap = home, held = HELP; PATCH: tap = PRESETS, held = SETUP; SEQ REC: tap = record / loop /
     * stop, held = record anew */
    if ((rel >> B_KNOBBANK) & 1u) {
        if (!ui.help)
            menu_go(MN_HOME);
        ui.help = 0;
        ui.dirty = 1;
    }
    if (ui.btn_t[B_KNOBBANK] && !ui.help && (fm1_in.buttons >> B_KNOBBANK) & 1u && fm1_ms - ui.btn_t[B_KNOBBANK] >= 500u) {
        ui.help = 1;
        ui.help_panel = 0;
        ui.help_t = fm1_ms;
    }
    if ((rel >> B_PATCH) & 1u && ui.btn_t[B_PATCH]) {
        if (fm1_ms - ui.btn_t[B_PATCH] < 500u)
            menu_go(MN_PRESETS);
        ui.btn_t[B_PATCH] = 0;
    }
    if (ui.btn_t[B_PATCH] && (fm1_in.buttons >> B_PATCH) & 1u && fm1_ms - ui.btn_t[B_PATCH] >= 500u) {
        ui.btn_t[B_PATCH] = 0;
        menu_go(MN_SETUP);
    }
    if ((rel >> B_REC) & 1u && ui.btn_t[B_REC]) {
        mo_toggle();
        ui.btn_t[B_REC] = 0;
    }
    if (ui.btn_t[B_REC] && (fm1_in.buttons >> B_REC) & 1u && fm1_ms - ui.btn_t[B_REC] >= 700u) {
        ui.btn_t[B_REC] = 0;
        ui.mo.state = MO_IDLE;
        mo_toggle();                                    /* held: a new recording */
    }
}

/* ------------------------------------------------------------- knobs --- */
static void knobs_poll(void)
{
    uint32_t k;
    int32_t s;
    if (ui.menu != MN_HOME)
        menu_build();
    for (k = 0; k < 8u; k++) {
        if ((s = fm1_enc_take(k)) == 0)
            continue;
        if (ui.help) {                                  /* (HELP: any knob turns its panels) */
            ui.help_panel = (uint8_t)((ui.help_panel + (s > 0 ? 1u : 5u)) % 6u);
            ui.help_t = fm1_ms;
            continue;
        }
        if (ui.dlg)
            continue;
        if (ui.menu == MN_HOME) {
            pitch_turn(k, s);
            continue;
        }
        if (ui.menu == MN_PRESETS && ui.page == 0u) {   /* any knob: through the presets */
            ui.pre_sel = (uint8_t)((ui.pre_sel + NPRESET * 4u + (uint32_t)s) % NPRESET);
            preset_load(ui.pre_sel);
            continue;
        }
        if (ui.it[k].kind != IK_NONE && !it_binary(&ui.it[k])) {   /* (two choices: the pads) */
            char lb[16], vb[16];
            it_turn(&ui.it[k], k, s);
            it_label(&ui.it[k], lb, sizeof lb);
            it_text(&ui.it[k], vb, sizeof vb);
            big_show(k, lb, vb);
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
    if (ui.dlg) {                                       /* a question: only red and green */
        for (v = 0; v < 16u; v++)
            padled_set(v, 0, 0, 0);
        PAD(8, 255, 0, 0, 256);
        PAD(15, 0, 255, 0, 256);
        return;
    }
    if (ui.menu == MN_PRESETS && ui.page == 0u) {       /* the picker: 14 presets, red cancel, green OK */
        for (v = 0; v < NPRESET; v++) {
            const uint8_t *c = VCOL[v & 7u];
            PAD(v, c[0], c[1], c[2], v == ui.pre_sel ? 256 : 30);
        }
        PAD(14, 255, 0, 0, 256);
        PAD(15, 0, 255, 0, 256);
        return;
    }
    for (v = 0; v < NV; v++) {
        int32_t l = vs[v].level, k = vs[v].stage ? 120 + (l >> 8) : 10;   /* idle: a dim glow; sounding: bright */
        if (VTYPE[v] == VT_VCO && kb.padhold[v - 3u])
            k = k < 160 ? 160 : k;
        k = k > 256 ? 256 : k;
        PAD(v, VCOL[v][0], VCOL[v][1], VCOL[v][2], k);
    }
    for (b = 0; b < 5u; b++) {                          /* 9..13: steps (lit: gate on, white: playhead) */
        uint32_t on = (pt.g[G_SGATES] >> b) & 1, here = pt.g[G_RUN] && md.step == b;
        if (b >= 3u + (uint32_t)pt.g[G_STAGES])
            padled_set(8u + b, 0, 0, 0);
        else if (here)
            PAD(8u + b, 220, 220, 240, 256);
        else
            PAD(8u + b, on ? 120 : 10, on ? 20 : 4, on ? 255 : 20, 256);
    }
    {   /* 14: the pulser's beat; 15 HOLD ALL (white); 16 CHAOS (a rainbow-ish pink) */
        uint32_t beat = pt.g[G_RUN] && md.pp < 0x40000000u;
        PAD(13, beat ? 255 : 30, beat ? 255 : 20, 10, 256);
        PAD(14, 120, 120, 140, 256);
        PAD(15, 255, 60, 200, ui.chaos_held ? 256 : 120);
    }
    if (ui.menu != MN_HOME)                             /* a menu's two-choice columns: the choice lit */
        for (v = 0; v < 8u; v++) {
            const item_t *it = &ui.it[v];
            const pdesc_t *d;
            int on;
            if (!it_binary(it))
                continue;
            d = it_desc(it);
            on = d->fmt == F_ONOFF ? it_get(it) != 0 : it_get(it) == d->min;
            PAD(v, VCOL[v][0], VCOL[v][1], VCOL[v][2], on ? 256 : 12);
            if (d->fmt == F_ENUM)
                PAD(8u + v, VCOL[v][0], VCOL[v][1], VCOL[v][2], on ? 12 : 256);
            else
                padled_set(8u + v, 0, 0, 0);
        }
    for (b = 0; b < FM1_NBTN; b++)
        fm1_led_key(b, BTN_MENU[b] >= 0 && BTN_MENU[b] == (int8_t)ui.menu);
    fm1_led_key(B_KNOBBANK, ui.help);
    fm1_led_key(B_PATCH, ui.menu == MN_PRESETS || ui.menu == MN_SETUP);
    fm1_led_key(B_PLAY, pt.g[G_RUN] != 0);
    fm1_led_key(B_REC, ui.mo.state == MO_REC ? (fm1_ms >> 8) & 1u : ui.mo.state == MO_LOOP);
    fm1_led_key(B_OCTDN, ui.oct < 0);
    fm1_led_key(B_OCTUP, ui.oct > 0);
#undef PAD
}

static void ui_init(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        ui.fad_last[i] = 0xFF;
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
    mo_play();
    speaker = settings.speaker;
    for (v = 0; v < NV; v++)
        gate_in[v] = VTYPE[v] == VT_VCO ? 0 : ui.pad_gate[v];
}

/* ------------------------------------------------------------- keys --- */
static void keys_poll(void)
{
    uint64_t now = fm1_in.keys, ch = now ^ ui.keys;
    uint32_t k;
    ui.keys = now;
    if (fm1_in.frames < 100u)                           /* (the first readings settle) */
        return;
    for (k = 0; ch && k < 37u; k++)
        if ((ch >> k) & 1u) {
            int32_t key = (int32_t)k + 12 * ui.oct;
            if ((now >> k) & 1u)
                kb_press(key, k >= 18u, fm1_in.key_vel[k]);
            else
                kb_release(key);
        }
}

/* USB MIDI in: notes play voices 4 + 5 as the keys do */
static void midi_poll(void)
{
    while (mi_r != mi_w) {
        uint32_t pkt, cin, n, vel;
        RING_PUBLISH();
        pkt = midi_in_q[mi_r % MQ];
        RING_PUBLISH();
        mi_r++;
        cin = pkt & 15u, n = (pkt >> 16) & 127u, vel = (pkt >> 24) & 127u;
        if (cin == 9u && vel)
            kb_press((int32_t)n - 36, n >= 54u, vel);
        else if (cin == 8u || cin == 9u)
            kb_release((int32_t)n - 36);
    }
}
