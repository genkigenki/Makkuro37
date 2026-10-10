/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Controls: pads (gates), keys (voices 4 + 5), knobs, faders, buttons, wheels -> the patch.
 * Pages show 4 values at a time on K5..K8; the page's button again (or K2) turns to the next four. */
enum { PG_VOICE, PG_MUTEMOD, PG_ENV, PG_MIX, PG_TUNING, PG_LFO, PG_SEQ, PG_FILTER, PG_FX, PG_SCENES, PG_N };
static const char *const PAGE_NAME[PG_N] = {"VOICE", "MUTE/MOD", "ENV", "MIX", "TUNING", "LFO", "SEQ", "FILTER",
                                             "FX", "SCENES"};
/* button ids (smk_input.h) */
enum { B_KNOBBANK, B_FADERBANK, B_LEFT, B_RIGHT, B_OCTDN, B_OCTUP, B_ARP, B_REPEAT, B_SCALE, B_CHORD, B_GLOBE,
       B_BT, B_PATCH, B_PARA, B_FX, B_SEQ, B_PLAY, B_REC };
static const int8_t BTN_PAGE[18] = {PG_FILTER, -1, -1, -1, -1, -1, PG_VOICE, PG_MUTEMOD, PG_TUNING, PG_ENV, PG_MIX,
                                    PG_VOICE, PG_SCENES, PG_VOICE, PG_FX, PG_SEQ, -1, PG_LFO};

enum { IK_NONE, IK_V, IK_VBIT, IK_G, IK_GBIT, IK_SET, IK_ACT };
typedef struct { uint8_t kind, id, bit; } item_t;
#define MAXIT 16u
enum { ACT_SLOT, ACT_LOAD, ACT_SAVE, ACT_INIT };
enum { SET_BRIGHT, SET_CONSOLE, SET_BOOT, SET_N };

static struct {
    uint8_t page, sub, voice, fbank, settings;
    int8_t oct;
    uint8_t slot;                          /* SCENES: the slot (0..7) */
    uint8_t arm;                           /* SCENES: action armed (ACT_ + 1), turn again to do it */
    uint32_t arm_until;
    item_t it[MAXIT];
    uint8_t nit;
    uint32_t enc_t[FM1_NENC];
    char msg[28];
    uint32_t msg_until;
    uint8_t dirty;                         /* the page area needs a redraw */
    uint8_t latched[NV], ignore_rel[NV], pad_gate[NV];
    uint32_t t_press[NV];
    uint64_t keys;                         /* keybed state seen last */
    uint8_t keyq[8], nkeys;                /* held keys, oldest first */
    uint8_t fad_last[4];
    uint32_t patch_down;                   /* PATCH held since (ms), 0 = up */
} ui;

static void keys_poll(void);
static void midi_poll(void);

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

/* ------------------------------------------------------------ the pages --- */
static void it_add(uint8_t kind, uint8_t id, uint8_t bit)
{
    if (ui.nit < MAXIT) {
        ui.it[ui.nit].kind = kind;
        ui.it[ui.nit].id = id;
        ui.it[ui.nit].bit = bit;
        ui.nit++;
    }
}
static void it_v(uint8_t id) { it_add(IK_V, id, 0); }
static void it_g(uint8_t id) { it_add(IK_G, id, 0); }

static void page_build(void)
{
    uint32_t t = VTYPE[ui.voice], k;
    ui.nit = 0;
    if (ui.settings) {
        for (k = 0; k < SET_N; k++)
            it_add(IK_SET, (uint8_t)k, 0);
        return;
    }
    switch (ui.page) {
    case PG_VOICE:
        if (t == VT_CLASSIC) {
            for (k = 0; k < 6u; k++)
                it_v((uint8_t)(CL_TUNE1 + k));
        } else if (t == VT_NOISE) {
            for (k = 0; k < 8u; k++)
                it_v((uint8_t)(NZ_PITCH + k));
        } else {
            it_v(VC_WAVE), it_v(VC_SHAPE), it_v(VC_OCT), it_v(VC_TUNE), it_v(VC_FM);
            if (ui.voice == 4u)
                it_v(VC_SYNC);
        }
        break;
    case PG_MUTEMOD:
        if (t == VT_CLASSIC) {
            for (k = 0; k < 5u; k++)
                it_add(IK_VBIT, CL_MUTE, (uint8_t)k);
            for (k = 0; k < 5u; k++)
                it_add(IK_VBIT, CL_MOD, (uint8_t)k);
        } else if (t == VT_NOISE) {
            it_v(NZ_SHAMT), it_v(NZ_SHCLK);
        }
        it_v(VP_MSRC), it_v(VP_MAMT);
        break;
    case PG_ENV:
        it_v(VP_ATT);
        if (t == VT_VCO)
            it_v(VC_DEC), it_v(VC_SUS);
        it_v(VP_RLS);
        if (t == VT_VCO)
            it_v(VC_EG);
        it_v(VP_GSRC);
        break;
    case PG_MIX:
        it_v(VP_VOL), it_v(VP_PAN), it_g(G_MASTER), it_g(G_BLEND);
        break;
    case PG_TUNING:
        it_g(G_TSET), it_g(G_ROOT), it_g(G_SNAP), it_g(G_FINE), it_g(G_GLIDE), it_g(G_KEYS);
        break;
    case PG_LFO:
        for (k = G_LARATE; k <= G_JYOFS; k++)
            it_g((uint8_t)k);
        break;
    case PG_SEQ:
        for (k = G_SEQ1; k <= G_SEQ5; k++)
            it_g((uint8_t)k);
        it_g(G_STAGES), it_g(G_PULSER), it_g(G_RUN);
        break;
    case PG_FILTER:
        for (k = G_FFL; k <= G_FCVAMT; k++)
            it_g((uint8_t)k);
        break;
    case PG_FX:
        it_g(G_FXL), it_g(G_FXR), it_g(G_BLEND), it_add(IK_NONE, 0, 0);
        for (k = G_FXLX; k <= G_FXRZ; k++) {
            it_g((uint8_t)k);
            if (k == G_FXLZ || k == G_FXRZ)
                it_add(IK_NONE, 0, 0);
        }
        break;
    case PG_SCENES:
        for (k = 0; k < 4u; k++)
            it_add(IK_ACT, (uint8_t)k, 0);
        break;
    }
    if (ui.sub * 4u >= ui.nit)
        ui.sub = 0;
}
static uint32_t page_subs(void) { return ui.nit ? (ui.nit + 3u) / 4u : 1u; }

static const pdesc_t *it_desc(const item_t *it)
{
    static const pdesc_t BIT = {0, 0, 1, 0, F_ONOFF, 0};
    static const pdesc_t SET[SET_N] = {{"PAD LIGHT", 1, 8, 4, F_NUM, 0}, {"CONSOLE", 0, 1, 1, F_ONOFF, 0},
                                       {"BOOT", 0, 8, 0, F_NUM, 0}};
    switch (it->kind) {
    case IK_V: return &VDESC[VTYPE[ui.voice]][it->id];
    case IK_G: return &GDESC[it->id];
    case IK_VBIT: case IK_GBIT: return &BIT;
    case IK_SET: return &SET[it->id];
    default: return 0;
    }
}

static int32_t it_get(const item_t *it)
{
    switch (it->kind) {
    case IK_V: return pt.v[ui.voice][it->id];
    case IK_G: return pt.g[it->id];
    case IK_VBIT: return (pt.v[ui.voice][it->id] >> it->bit) & 1;
    case IK_GBIT: return (pt.g[it->id] >> it->bit) & 1;
    case IK_SET: return it->id == SET_BRIGHT ? settings.bright : it->id == SET_CONSOLE ? settings.console
                                                                                         : settings.boot_scene;
    case IK_ACT: return it->id == ACT_SLOT ? ui.slot : 0;
    default: return 0;
    }
}

static void it_label(const item_t *it, char *b, uint32_t n)
{
    static const char *const ACT[4] = {"SLOT", "LOAD", "SAVE", "INIT"};
    const pdesc_t *d = it_desc(it);
    if (it->kind == IK_VBIT) {
        uint32_t k;
        str_cpy(b, it->id == CL_MUTE ? "GEN " : "MOD ", n);
        k = str_len(b);
        if (k + 1u < n) {
            b[k] = (char)('1' + it->bit);
            b[k + 1u] = 0;
        }
        return;
    }
    if (it->kind == IK_ACT) {
        str_cpy(b, ACT[it->id], n);
        return;
    }
    str_cpy(b, d ? d->label : "", n);
}

static void it_text(const item_t *it, char *b, uint32_t n)
{
    const pdesc_t *d = it_desc(it);
    int32_t v = it_get(it);
    if (it->kind == IK_VBIT) {                              /* MUTE bits read as ON / OFF (generator sounding) */
        str_cpy(b, it->id == CL_MUTE ? (v ? "OFF" : "ON") : (v ? "ON" : "OFF"), n);
        return;
    }
    if (it->kind == IK_ACT) {
        if (it->id == ACT_SLOT) {
            b[0] = (char)('1' + ui.slot);
            str_cpy(b + 1, (scene_used >> ui.slot) & 1u ? " USED" : " FREE", n - 1u);
        } else
            str_cpy(b, ui.arm == it->id + 1u ? "AGAIN" : ">", n);
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
        if (v == 64) {
            str_cpy(b, "C", n);
        } else {
            b[0] = v < 64 ? 'L' : 'R';
            fmt_int(b + 1, v < 64 ? 64 - v : v - 64);
        }
        break;
    case F_TUNE: {                                          /* the generator's note and cents */
        int32_t c = GEN_LO[it->id - CL_TUNE1] * 100 + v, m = (c + 50) / 100, dc = c - m * 100;
        char t[8];
        str_cpy(b, NOTE_NAMES[m % 12], n);
        fmt_int(t, m / 12 - 1);
        str_cpy(b + str_len(b), t, n - str_len(b));
        if (dc) {
            uint32_t k = str_len(b);
            b[k] = dc > 0 ? '+' : '-';
            fmt_int(b + k + 1u, dc > 0 ? dc : -dc);
        }
        break;
    }
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
        if (it->kind == IK_SET && it->id == SET_BOOT && !v)
            str_cpy(b, "INIT", n);
        else
            fmt_int(b, v);
    }
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

static void scene_act(uint32_t a, int32_t s)
{
    static const char *const NAME[4] = {"", "LOAD ", "SAVE ", "INIT"};
    char d[2] = {(char)('1' + ui.slot), 0};
    if (s <= 0)
        return;
    if (ui.arm != a + 1u || (int32_t)(fm1_ms - ui.arm_until) > 0) {
        ui.arm = (uint8_t)(a + 1u);
        ui.arm_until = fm1_ms + 2500u;
        ui_say(NAME[a], a == ACT_INIT ? "? TURN AGAIN" : "? TURN AGAIN");
        return;
    }
    ui.arm = 0;
    if (a == ACT_LOAD) {
        int rc = scene_load(ui.slot);
        ui_say(rc ? "EMPTY " : "LOADED ", d);
        if (!rc && settings.scene != ui.slot + 1u) {
            settings.scene = (uint8_t)(ui.slot + 1u);
        }
    } else if (a == ACT_SAVE) {
        int rc = scene_save(ui.slot);
        ui_say(rc ? "SAVE FAILED " : "SAVED ", d);
    } else {
        patch_init(&pt);
        ui_say("INIT SOUND", 0);
    }
}

static void it_turn(const item_t *it, uint32_t e, int32_t s)
{
    const pdesc_t *d = it_desc(it);
    int32_t v;
    ui.dirty = 1;
    switch (it->kind) {
    case IK_VBIT: {                                         /* right: GEN sounding / MOD on */
        int16_t *p = &pt.v[ui.voice][it->id], m = (int16_t)(1 << it->bit);
        if ((s > 0) == (it->id == CL_MOD))
            *p |= m;
        else
            *p &= (int16_t)~m;
        return;
    }
    case IK_SET:
        if (it->id == SET_BRIGHT)
            settings.bright = (uint8_t)clamp(settings.bright + s, 1, 8);
        else if (it->id == SET_CONSOLE)
            settings.console = s > 0;
        else
            settings.boot_scene = (uint8_t)clamp(settings.boot_scene + s, 0, NSCENE);
        return;
    case IK_ACT:
        if (it->id == ACT_SLOT) {
            ui.slot = (uint8_t)clamp(ui.slot + s, 0, NSCENE - 1);
            ui.arm = 0;
        } else
            scene_act(it->id, s);
        return;
    case IK_V:
        v = pt.v[ui.voice][it->id];
        if (d->fmt == F_TUNE && pt.g[G_SNAP]) {             /* to the next note of the tuning set */
            int32_t lo = GEN_LO[it->id - CL_TUNE1] * 100, c = lo + v, k;
            for (k = 0; k < (s > 0 ? s : -s); k++)
                c = tun_next((uint32_t)pt.g[G_TSET], (uint32_t)pt.g[G_ROOT], c, s > 0 ? 1 : -1);
            pt.v[ui.voice][it->id] = vp_clamp(ui.voice, it->id, c - lo);
            return;
        }
        pt.v[ui.voice][it->id] = vp_clamp(ui.voice, it->id, v + accel(e, s, d->max - d->min));
        return;
    case IK_G:
        v = pt.g[it->id];
        pt.g[it->id] = (int16_t)clamp(v + accel(e, s, d->max - d->min), d->min, d->max);
        return;
    }
}

/* ------------------------------------------------------------- gates --- */
static void gate_press(uint32_t v)
{
    if (ui.latched[v]) {                   /* a tap on a held drone ends it */
        ui.latched[v] = 0;
        ui.pad_gate[v] = 0;
        ui.ignore_rel[v] = 1;
        return;
    }
    ui.pad_gate[v] = 1;
    ui.t_press[v] = fm1_ms;
}
static void gate_release(uint32_t v)
{
    if (ui.ignore_rel[v]) {
        ui.ignore_rel[v] = 0;
        return;
    }
    if (fm1_ms - ui.t_press[v] < 350u)
        ui.latched[v] = 1;                 /* a tap: HOLD */
    else
        ui.pad_gate[v] = 0;                /* held: momentary */
}

static void pads_poll(void)
{
    int32_t e;
    while ((e = smk_pad_take()) >= 0) {
        uint32_t pad = (uint32_t)e >> 8, vel = (uint32_t)e & 255u;
        if (pad < 8u) {                    /* top row: the voices' gates */
            if (vel)
                gate_press(pad);
            else
                gate_release(pad);
        }
    }
}

/* ------------------------------------------------------------- faders --- */
static void faders_poll(void)
{
    static const uint8_t BANK[4][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {G_FFL, G_FRL, G_FFR, G_FRR},
                                       {G_BLEND, G_LARATE, G_LBRATE, G_MASTER}};
    uint32_t i;
    if (fm1_in.frames < 200u)              /* (the first readings settle) */
        return;
    for (i = 0; i < 4u; i++) {
        uint32_t v = fm1_in.fader[i];
        if (ui.fad_last[i] == 0xFF || v == ui.fad_last[i]) {   /* takes over once it moves */
            ui.fad_last[i] = (uint8_t)v;
            continue;
        }
        ui.fad_last[i] = (uint8_t)v;
        if (ui.fbank < 2u)
            pt.v[BANK[ui.fbank][i]][VP_VOL] = (int16_t)v;
        else
            pt.g[BANK[ui.fbank][i]] = (int16_t)v;
        ui.dirty = 1;
    }
}

/* ------------------------------------------------------------- buttons --- */
static void page_go(uint32_t pg)
{
    if (ui.settings) {
        ui.settings = 0;
        settings_save();
    }
    if (ui.page == pg)
        ui.sub = (uint8_t)((ui.sub + 1u) % page_subs());
    else {
        ui.page = (uint8_t)pg;
        ui.sub = 0;
    }
    ui.arm = 0;
    ui.dirty = 1;
}
static void voice_sel(uint32_t v)
{
    ui.voice = (uint8_t)(v % NV);
    ui.sub = 0;
    ui.dirty = 1;
}

static void buttons_poll(void)
{
    uint32_t rel, pr = fm1_input_edges(&rel), b;
    for (b = 0; b < FM1_NBTN; b++) {
        if (!((pr >> b) & 1u))
            continue;
        switch (b) {
        case B_FADERBANK:
            ui.fbank = (uint8_t)((ui.fbank + 1u) & 3u);
            for (rel = 0; rel < 4u; rel++)
                ui.fad_last[rel] = 0xFF;
            ui_say("FADERS ", ui.fbank == 0 ? "A: VOL 1-4" : ui.fbank == 1 ? "B: VOL 5-8"
                              : ui.fbank == 2 ? "C: FILTER" : "D: FX LFO MST");
            break;
        case B_LEFT:
            voice_sel(ui.voice + NV - 1u);
            break;
        case B_RIGHT:
            voice_sel(ui.voice + 1u);
            break;
        case B_OCTDN:
            if (ui.oct > -3) ui.oct--;
            ui.dirty = 1;
            break;
        case B_OCTUP:
            if (ui.oct < 3) ui.oct++;
            ui.dirty = 1;
            break;
        case B_BT:                         /* NOISE: voices 3 and 8 */
            voice_sel(ui.voice == 2u ? 7u : 2u);
            ui.page = PG_VOICE;
            break;
        case B_PARA:                       /* VCO: voices 4 and 5 */
            voice_sel(ui.voice == 3u ? 4u : 3u);
            ui.page = PG_VOICE;
            break;
        case B_PATCH:
            ui.patch_down = fm1_ms | 1u;
            break;
        case B_PLAY:
            pt.g[G_RUN] = (int16_t)!pt.g[G_RUN];
            ui.dirty = 1;
            break;
        default:
            if (BTN_PAGE[b] >= 0)
                page_go((uint32_t)BTN_PAGE[b]);
        }
    }
    if ((rel >> B_PATCH) & 1u && ui.patch_down) {           /* PATCH: tap = SCENES, held = settings */
        if (ui.settings) {
            ui.settings = 0;
            settings_save();
            ui.dirty = 1;
        } else if (fm1_ms - ui.patch_down < 500u)
            page_go(PG_SCENES);
        ui.patch_down = 0;
    }
    if (ui.patch_down && !ui.settings && fm1_ms - ui.patch_down >= 500u) {
        ui.settings = 1;
        ui.sub = 0;
        ui.dirty = 1;
    }
}

/* ------------------------------------------------------------- knobs --- */
static void knobs_poll(void)
{
    int32_t s;
    uint32_t k;
    if ((s = fm1_enc_take(0)) != 0) {                       /* K1 master */
        pt.g[G_MASTER] = (int16_t)clamp(pt.g[G_MASTER] + accel(0, s, 127) * 2, 0, 127);
        ui.dirty = 1;
    }
    if ((s = fm1_enc_take(1)) != 0) {                       /* K2 pages (and their fours) */
        int32_t pos = ui.page * 8 + ui.sub;
        if (ui.settings)
            ui.settings = 0, settings_save();
        while (s) {
            int32_t d = s > 0 ? 1 : -1;
            page_build();
            if (d > 0)
                pos = ui.sub + 1u < page_subs() ? pos + 1 : ((ui.page + 1) % PG_N) * 8;
            else if (ui.sub)
                pos--;
            else {
                ui.page = (uint8_t)((ui.page + PG_N - 1u) % PG_N);
                ui.sub = 0;
                page_build();
                pos = ui.page * 8 + (int32_t)page_subs() - 1;
            }
            ui.page = (uint8_t)(pos / 8);
            ui.sub = (uint8_t)(pos % 8);
            s -= d;
        }
        ui.arm = 0;
        ui.dirty = 1;
    }
    if ((s = fm1_enc_take(2)) != 0)                         /* K3 voice */
        voice_sel(ui.voice + (uint32_t)(s > 0 ? s : NV * 8 + s));
    if ((s = fm1_enc_take(3)) != 0) {                       /* K4 tuning set */
        pt.g[G_TSET] = (int16_t)clamp(pt.g[G_TSET] + s, 0, TS_N - 1);
        ui_say("TUNING ", TSETS[pt.g[G_TSET]].name);
    }
    page_build();
    for (k = 0; k < 4u; k++) {
        uint32_t i = ui.sub * 4u + k;
        if ((s = fm1_enc_take(4u + k)) == 0)
            continue;
        if (i < ui.nit && ui.it[i].kind != IK_NONE)
            it_turn(&ui.it[i], 4u + k, s);
    }
}

/* ------------------------------------------------------------- LEDs --- */
static const uint8_t VCOL[NV][3] = {{255, 46, 136}, {176, 76, 255}, {46, 230, 255}, {255, 176, 0},
                                    {255, 106, 0}, {61, 123, 255}, {255, 79, 216}, {46, 255, 154}};
static void ui_leds(void)
{
    uint32_t v, b, sc = 6u + settings.bright * 6u;          /* pad brightness of 255 */
    for (v = 0; v < NV; v++) {
        int32_t l = vs[v].level, k = 8 + (l >> 7);         /* idle: a dim glow; sounding: up to full */
        k = k > 256 ? 256 : k;
        padled_set(v, (uint8_t)(VCOL[v][0] * k * sc >> 16), (uint8_t)(VCOL[v][1] * k * sc >> 16),
                   (uint8_t)(VCOL[v][2] * k * sc >> 16));
    }
    for (b = 0; b < FM1_NBTN; b++)
        fm1_led_key(b, BTN_PAGE[b] == (int8_t)ui.page && !ui.settings &&
                           (b != B_BT || VTYPE[ui.voice] == VT_NOISE) && (b != B_PARA || VTYPE[ui.voice] == VT_VCO) &&
                           (b != B_ARP || ui.page != PG_VOICE || VTYPE[ui.voice] == VT_CLASSIC));
    fm1_led_key(B_FADERBANK, ui.fbank != 0);
    fm1_led_key(B_PATCH, ui.settings || ui.page == PG_SCENES);
    fm1_led_key(B_PLAY, pt.g[G_RUN] != 0);
    fm1_led_key(B_OCTDN, ui.oct < 0);
    fm1_led_key(B_OCTUP, ui.oct > 0);
}

static void ui_init(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        ui.fad_last[i] = 0xFF;
    ui.dirty = 1;
    page_build();
}

static void ui_input(void)
{
    uint32_t v;
    pads_poll();
    keys_poll();
    buttons_poll();
    knobs_poll();
    faders_poll();
    for (v = 0; v < NV; v++)
        gate_in[v] = ui.pad_gate[v];
    if (ui.arm && (int32_t)(fm1_ms - ui.arm_until) > 0) {
        ui.arm = 0;
        ui.dirty = 1;
    }
}

/* ------------------------------------------------------------- keys --- */
/* the keybed and USB MIDI notes play voices 4 + 5: the last two keys held (4 the lower, 5 the upper),
 * one key both (KEYS DUO); KEYS UNISON: the last key both. Tuned by the tuning set. */
static struct { uint8_t id; int8_t k; } keyq[8];
static uint32_t nkeyq;

static int32_t key_to_cents(int32_t k)              /* k: semitone keys above C2 (degrees for non-12 sets) */
{
    uint32_t set = (uint32_t)pt.g[G_TSET], root = (uint32_t)pt.g[G_ROOT], n = TSETS[set % TS_N].n;
    return n == 12u ? tun_key(set, root, 36 + k) : tun_degree(set, root, 3 * (int32_t)n + k);
}

static void key_event(uint32_t id, int32_t k, int on)
{
    uint32_t i, j;
    for (i = j = 0; i < nkeyq; i++)                    /* (a key held again moves to the end) */
        if (keyq[i].id != id)
            keyq[j++] = keyq[i];
    nkeyq = j;
    if (on) {
        if (nkeyq == 8u) {
            for (i = 1; i < 8u; i++)
                keyq[i - 1u] = keyq[i];
            nkeyq--;
        }
        keyq[nkeyq].id = (uint8_t)id;
        keyq[nkeyq].k = (int8_t)k;
        nkeyq++;
        key_trig++;
    }
}

static void keys_update(void)                       /* every poll: follows the tuning set too */
{
    if (nkeyq) {
        int32_t a = key_to_cents(keyq[nkeyq - 1u].k), b = nkeyq > 1u && !pt.g[G_KEYS] ? key_to_cents(keyq[nkeyq - 2u].k) : a;
        key_cents[0] = a < b ? a : b;
        key_cents[1] = a < b ? b : a;
    }
    key_gate = nkeyq != 0u;
}

static void keys_poll(void)
{
    uint64_t now = fm1_in.keys, ch = now ^ ui.keys;
    uint32_t k;
    ui.keys = now;
    for (k = 0; ch && k < 37u; k++)
        if ((ch >> k) & 1u)
            key_event(k, (int32_t)k + 12 * ui.oct, (int)((now >> k) & 1u));
    keys_update();
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
        if (cin == 9u || cin == 8u)
            key_event(128u + n, (int32_t)n - 36, cin == 9u && vel);
    }
    keys_update();
}
