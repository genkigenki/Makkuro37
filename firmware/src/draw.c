/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* The screen, laid out as the panel: eight 30-pixel columns, column i under knob Ki and above pad column i, in
 * voice i's colour. Home: the eight soot creatures (they squish when woken, glow and sing while they sound,
 * sleep when silent). Menus: the eight values in the columns. Big letters; a turned knob shows its value big. */
#define C_BG RGB(4, 0, 10)
#define COLW 30

static uint16_t rgbk(const uint8_t *c, int32_t k)          /* colour c at k / 256 */
{
    k = clamp(k, 0, 256);
    return RGB(c[0] * k >> 8, c[1] * k >> 8, c[2] * k >> 8);
}
static inline int32_t isin(uint32_t ph, int32_t amp) { return (SINE[ph >> 22] * amp) >> 15; }

static void cv_circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t x = r, y = 0, e = 1 - r;
    while (x >= y) {
        cv_pset(cx + x, cy + y, c), cv_pset(cx - x, cy + y, c), cv_pset(cx + x, cy - y, c), cv_pset(cx - x, cy - y, c);
        cv_pset(cx + y, cy + x, c), cv_pset(cx - y, cy + x, c), cv_pset(cx + y, cy - x, c), cv_pset(cx - y, cy - x, c);
        y++;
        if (e < 0)
            e += 2 * y + 1;
        else {
            x--;
            e += 2 * (y - x) + 1;
        }
    }
}
static void cv_ellipse(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t c)   /* filled */
{
    int32_t x, y, rr = rx * rx * ry * ry;
    for (y = -ry; y <= ry; y++)
        for (x = -rx; x <= rx; x++)
            if (x * x * ry * ry + y * y * rx * rx <= rr)
                cv_pset(cx + x, cy + y, c);
}
static void cv_thick(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)     /* a 2-pixel line */
{
    cv_line(x0, y0, x1, y1, c);
    cv_line(x0 + 1, y0, x1 + 1, y1, c);
    cv_line(x0, y0 + 1, x1, y1 + 1, c);
}
static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}
static inline uint32_t hash3(int32_t x, int32_t y, uint32_t s)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* ------------------------------------------------------------- a creature --- */
/* crisp: a black body, a light spiky fur line (its colour while it sings), a solid glow ring by its level, big
 * white eyes. (cx, cy) the body's centre, rx / ry its radii, lv 0..32767, hit 0..256 (a squish) */
static const int8_t SPIKE[16][2] = {{0, -16}, {6, -15}, {11, -11}, {15, -6}, {16, 0}, {15, 6}, {11, 11}, {6, 15},
                                    {0, 16}, {-6, 15}, {-11, 11}, {-15, 6}, {-16, 0}, {-15, -6}, {-11, -11}, {-6, -15}};
static void creature(int32_t cx, int32_t cy, int32_t r0x, int32_t r0y, int32_t lv, int32_t hit, int sleep, int blink,
                     const uint8_t *col, uint32_t seed)
{
    int32_t rx = r0x * (256 + hit / 5) >> 8, ry = r0y * (256 - hit / 4) >> 8, x, y, glow = lv >> 7, k;
    int32_t by = cy + (r0y - ry);                                     /* (squashed onto its feet) */
    int32_t rxi = rx * rx, ryi = ry * ry;
    uint16_t fur = glow > 24 ? rgbk(col, 150 + glow / 2) : RGB(150, 140, 180);
    if (glow > 24) {                                                  /* the glow: a solid ring */
        uint16_t gc = rgbk(col, 70 + glow * 3 / 4);
        int32_t gx = rx + 4, gy = ry + 5 + (glow >> 6);
        for (k = 0; k < 2; k++, gx++, gy++)
            for (y = -gy; y <= gy; y++) {                             /* (an ellipse outline, row by row) */
                int32_t xx = gx * gx - gx * gx * y * y / (gy * gy), xs = 0;
                while ((xs + 1) * (xs + 1) <= xx)
                    xs++;
                cv_pset(cx - xs, by + y, gc), cv_pset(cx + xs, by + y, gc);
            }
    }
    for (k = 0; k < 16; k++) {                                        /* fur: spikes, a little restless */
        int32_t j = (int32_t)((hash3(k, 0, seed) & 3u) == 0u), ex = SPIKE[k][0], ey = SPIKE[k][1];
        if (!ey)
            continue;                                                 /* (none sideways: neighbours touch) */
        cv_thick(cx + ex * rx / 20, by + ey * ry / 20, cx + ex * (rx + 3 + j) / 16, by + ey * (ry + 3 + j) / 16, fur);
    }
    for (y = -ry - 1; y <= ry + 1; y++)
        for (x = -rx - 1; x <= rx + 1; x++) {
            int32_t d2 = x * x * 256 / rxi + y * y * 256 / ryi;
            if (d2 <= 256)
                cv_pset(cx + x, by + y, d2 > 200 ? fur : RGB(10, 8, 16));   /* body, a crisp rim */
        }
    cv_rect(cx - rx / 2 - 3, cy + r0y + 1, 5, 3, RGB(200, 190, 230));   /* feet */
    cv_rect(cx + rx / 2 - 2, cy + r0y + 1, 5, 3, RGB(200, 190, 230));
    {   /* eyes: big and white; wide when woken; shut (a line) when asleep */
        int32_t ex = rx / 2, ey = by - ry / 4, er = rx / 3 + hit / 100;
        if (sleep || blink) {                                         /* shut: two small smiles */
            int32_t s2;
            for (s2 = -1; s2 <= 1; s2 += 2) {
                int32_t x0 = cx + s2 * ex;
                cv_rect(x0 - er, ey, 2, 2, RGB(220, 220, 235));
                cv_rect(x0 - er + 2, ey + 1, 2 * er - 3, 2, RGB(220, 220, 235));
                cv_rect(x0 + er - 1, ey, 2, 2, RGB(220, 220, 235));
            }
        } else {
            int32_t look = hit ? 0 : isin(seed * 40503u, 1);
            cv_ellipse(cx - ex, ey, er, er + 1, RGB(255, 255, 255));
            cv_ellipse(cx + ex, ey, er, er + 1, RGB(255, 255, 255));
            cv_ellipse(cx - ex + look, ey + 1, er / 2, er / 2, RGB(0, 0, 0));
            cv_ellipse(cx + ex + look, ey + 1, er / 2, er / 2, RGB(0, 0, 0));
        }
        if (!sleep && lv > 1500) {                                    /* singing: a mouth in its colour */
            int32_t m = 1 + (lv >> 13);
            cv_ellipse(cx, ey + er + 4 + m, 2, m, rgbk(col, 256));
        }
    }
}

/* ------------------------------------------------------------- home: the creatures in a row --- */
#define CRH 124
static struct {
    uint8_t hits;
    uint32_t hit_t, ph, blink_t, drawn;
} cre[NV];
static uint32_t fuzz_seed;

static int creature_cell(uint32_t v, int force)
{
    int32_t lv = vs[v].level, hit = 0, t, bob;
    uint32_t now = fm1_ms, h, gs = (uint32_t)pt.v[v][VP_GSRC];
    int sleep = !vs[v].stage && lv < 200, blink;
    uint16_t c = rgbk(VCOL[v], 256);
    if (vs[v].hits != cre[v].hits) {
        cre[v].hits = vs[v].hits;
        cre[v].hit_t = now;
    }
    t = (int32_t)(now - cre[v].hit_t) * 256 / 260;                    /* the wake-up squish: 260 ms */
    if (t < 256)
        hit = t < 60 ? t * 4 : 256 - (t - 60) * 256 / 196;
    if ((int32_t)(now - cre[v].blink_t) > 0)
        cre[v].blink_t = now + 2000u + hash3((int32_t)v, (int32_t)now, 7) % 3000u;
    blink = cre[v].blink_t - now < 130u && !sleep;
    cre[v].ph += (vs[v].inc[0] >> 9) + 3000000u;                      /* sings with its pitch */
    bob = sleep ? isin(now * 1200000u, 2) : isin(cre[v].ph, 2 + (lv >> 12));
    h = (uint32_t)(lv >> 9) * 31u + (uint32_t)hit * 7u + (uint32_t)bob * 131u + (uint32_t)blink * 3u +
        (sleep ? 0u : (now >> 7)) + ui.latched[v] * 4441u + ui.pad_gate[v] * 7919u + gs * 104729u +
        (VTYPE[v] == VT_VCO ? kb.padhold[v - 3u] * 31337u : 0u) + 1u;   /* (never 0: 0 = not drawn yet) */
    if (!force && h == cre[v].drawn)
        return 0;
    cre[v].drawn = h;
    cv_begin(COLW, CRH, C_BG);
    {
        char d[2] = {(char)('1' + v), 0};
        text_c(COLW / 2, -3, &FONT_L, d, c);                         /* its number, its colour */
    }
    creature(COLW / 2, 72 + bob, 12, 17, lv, hit, sleep, blink, VCOL[v], sleep ? fuzz_seed >> 4 : fuzz_seed + v);
    if (sleep && ((now >> 10) + v) % 4u == 0u)
        cv_text(COLW - 9, 30 - (int32_t)((now >> 7) & 3u), &FONT_S, "z", RGB(170, 160, 220));
    /* how its gate runs: HOLD (a solid square), held now (a ring), SEQ (steps), LFO (a wave), VCO: keys */
    if (VTYPE[v] == VT_VCO)
        cv_rect(7, CRH - 8, 16, 4, kb.padhold[v - 3u] ? c : RGB(110, 100, 140));   /* (a key) */
    else if (ui.latched[v])
        cv_rect(10, CRH - 11, 10, 9, c);
    else if (ui.pad_gate[v])
        cv_circle(15, CRH - 7, 4, c);
    if (gs == GS_SEQ)
        cv_rect(2, CRH - 5, 2, 3, c), cv_rect(5, CRH - 7, 2, 5, c), cv_rect(8, CRH - 9, 2, 7, c);
    else if (gs == GS_LFOA || gs == GS_LFOB) {
        int32_t i;
        for (i = 0; i < 8; i++)
            cv_rect(1 + i, CRH - 6 + isin((uint32_t)i * 0x20000000u, 3), 1, 2, c);
    }
    cv_blit(COLW * v, 32);
    return 1;
}

/* the big value: label (L) and value (XL) of what was turned last, in its column's colour */
static struct { uint32_t until; uint8_t col; char label[16], val[16]; } big;
static void big_show(uint32_t col, const char *label, const char *val)
{
    big.until = fm1_ms + 1200u;
    big.col = (uint8_t)col;
    str_cpy(big.label, label, sizeof big.label);
    str_cpy(big.val, val, sizeof big.val);
    ui.dirty = 1;
}
static int big_draw(int32_t h)                           /* into a 240 x h canvas: 1 if shown */
{
    static const uint8_t WHITE[3] = {255, 255, 255};
    const uint8_t *c = big.col < NV ? VCOL[big.col] : WHITE;
    const felucca_font_t *f;
    if ((int32_t)(big.until - fm1_ms) <= 0)
        return 0;
    f = text_w(&FONT_XL, big.val) <= 236 ? &FONT_XL : &FONT_L;
    if (big.col < NV)                                    /* a mark under its column */
        cv_rect(COLW * big.col + 2, 0, COLW - 4, 4, rgbk(c, 256));
    text_c(120, 4, &FONT_L, big.label, rgbk(c, 200));
    text_c(120, h - f->h, f, big.val, rgbk(c, 256));
    return 1;
}

static void home_header(void)
{
    char b[16];
    cv_begin(240, 32, C_BG);
    str_cpy(b, PRESET_NAME[ui.preset % NPRESET], sizeof b);
    cv_text(2, 0, &FONT_L, b, RGB(235, 235, 245));
    if (pt.g[G_RUN])                                     /* the pulser runs: a green triangle */
        for (int32_t i = 0; i < 9; i++)
            cv_rect(196 + i, 8 + i / 2, 1, 18 - i, RGB(60, 240, 90));
    if (ui.mo.state == MO_REC)                           /* the motion loop: red recording, green looping */
        cv_ellipse(226, 16, 7, 7, (fm1_ms >> 8) & 1u ? RGB(255, 40, 40) : RGB(90, 10, 10));
    else if (ui.mo.state == MO_LOOP)
        cv_circle(226, 16, 7, RGB(60, 240, 90)), cv_circle(226, 16, 6, RGB(60, 240, 90));
    cv_blit(0, 0);
}

static void home_bottom(void)
{
    uint32_t v;
    cv_begin(240, 84, C_BG);
    if (!big_draw(84)) {
        if ((int32_t)(ui.msg_until - fm1_ms) > 0)
            text_c(120, 26, &FONT_L, ui.msg, RGB(255, 255, 255));
        else
            for (v = 0; v < NV; v++) {                   /* each voice's note, big, staggered: 1 3 5 7 up, 2 4 6 8 down */
                char b[16];
                int32_t x = COLW * (int32_t)v + COLW / 2, y = (v & 1u) ? 44 : 6, c;
                if (VTYPE[v] == VT_VCO && !kb.played)
                    str_cpy(b, "-", sizeof b);
                else {
                    c = voice_base(v) + pt.v[v][VP_PITCH];
                    c = (c + 50) / 100 * 100;            /* (the nearest note) */
                    cents_text(b, sizeof b, c);
                }
                cv_rect(x - 1, (v & 1u) ? 2 : 38, 2, 4, rgbk(VCOL[v], 160));   /* (a tick up to its creature) */
                text_c(clamp(x, text_w(&FONT_L, b) / 2, 240 - text_w(&FONT_L, b) / 2), y, &FONT_L, b,
                       rgbk(VCOL[v], 256));
            }
    }
    cv_blit(0, 156);
}

/* a VCO wave for the picture, Q15 */
static int32_t wave_y(uint32_t w, uint32_t ph, int32_t shape)
{
    vstate_t t;
    t.kv[VK_W] = clamp(shape * 258, 128, 32640);
    t.kv[VK_KU] = (65535 << 6) / t.kv[VK_W];
    t.kv[VK_KD] = (65535 << 6) / (32768 - t.kv[VK_W]);
    t.kv[VK_SHAPE] = shape * 258;
    t.kv[VK_PW] = (int32_t)(0x80000000u + (uint32_t)(shape - 64) * 0x1F00000u);
    if (w == W_SAW)
        return (int32_t)(ph >> 16) - 32768;
    if (w == W_PULSE)
        return ph < (uint32_t)t.kv[VK_PW] ? 26000 : -26000;
    return vco_wave(&t, w, ph, 0);
}

/* ------------------------------------------------------------- menus: the eight columns --- */
static void menu_header(uint32_t npages)
{
    uint32_t k;
    cv_begin(240, 32, C_BG);
    cv_text(2, 0, &FONT_L, MENU_NAME[ui.menu], RGB(235, 235, 245));
    for (k = 0; npages > 1u && k < npages && k < 8u; k++)
        cv_rect(238 - 8 * ((int32_t)npages - (int32_t)k), 26, 6, 4, k == ui.page ? RGB(255, 255, 255) : RGB(80, 74, 104));
    if (!str_eq(ui.page_name, MENU_NAME[ui.menu]))      /* the page (VOICE: the voice's number, its colour) */
        cv_text(236 - text_w(&FONT_L, ui.page_name), 0, &FONT_L, ui.page_name,
                ui.menu == MN_VOICE ? rgbk(VCOL[ui.page % NV], 256) : RGB(255, 220, 90));
    cv_blit(0, 0);
}

static void menu_columns(void)
{
    uint32_t k;
    cv_begin(240, 124, C_BG);
    for (k = 0; k < 8u; k++) {
        const item_t *it = &ui.it[k];
        const pdesc_t *d = it_desc(it);
        int32_t x = COLW * (int32_t)k, v;
        uint16_t c = rgbk(VCOL[k], 256), dim = rgbk(VCOL[k], 70);
        if (it->kind == IK_NONE || !d)
            continue;
        if (it->alt != 0xFF && ((ui.altmask >> k) & 1u))   /* a pair showing its second value: lighter */
            c = RGB((VCOL[k][0] + 255) >> 1, (VCOL[k][1] + 255) >> 1, (VCOL[k][2] + 255) >> 1);
        cv_rect(x + 1, 0, COLW - 2, 3, c);              /* the column's colour, under its knob */
        v = it_get(it);
        if (it->kind == IK_V && (it->id == VP_WAVE || (it->id == VC_WAVE && VTYPE[it->voice] == VT_VCO))) {
            int32_t yy, ly = 0;                          /* WAVE: the wave itself, down the column */
            for (yy = 0; yy < 96; yy++) {
                uint32_t ph = (uint32_t)yy * 0x5555555u;   /* (two periods) */
                int32_t s2 = it->id == VP_WAVE && VTYPE[it->voice] != VT_VCO ? cwave((uint32_t)v, ph, 0)
                                                                              : wave_y((uint32_t)v, ph, pt.v[it->voice][VC_SHAPE]);
                int32_t xx = x + 15 + (s2 * 11 >> 15);
                if (yy)
                    cv_thick(x + 15 + ly, 10 + yy - 1, xx, 10 + yy, c);
                ly = xx - x - 15;
            }
            continue;
        }
        if (it_binary(it)) {                             /* two pads: the lit one is the choice */
            int on = d->fmt == F_ONOFF ? v != 0 : v == d->min;
            cv_rect(x + 6, 20, 18, 18, on ? c : dim);
            cv_rect(x + 6, 44, 18, 18, d->fmt == F_ENUM && !on ? c : RGB(30, 28, 40));
            if (d->fmt == F_ENUM) {                      /* the two choices' first letters */
                char a[2] = {d->names[0][0], 0}, b[2] = {d->names[1][0], 0};
                text_c(x + 15, 18, &FONT_S, a, on ? RGB(0, 0, 0) : c);
                text_c(x + 15, 42, &FONT_S, b, !on ? RGB(0, 0, 0) : c);
            }
            cv_rect(x + 9, 70, 12, 2, RGB(120, 112, 150));   /* (a pad symbol: "on the pads") */
            cv_rect(x + 9, 70, 2, 8, RGB(120, 112, 150));
            cv_rect(x + 19, 70, 2, 8, RGB(120, 112, 150));
            cv_rect(x + 9, 76, 12, 2, RGB(120, 112, 150));
        } else if (d->fmt == F_ENUM) {                   /* a ladder of its choices, the current one lit */
            int32_t n = d->max - d->min + 1, i, hh = 96 / (n > 1 ? n : 1);
            for (i = 0; i < n; i++)
                cv_rect(x + 7, 110 - (i + 1) * hh + 1, 16, hh - 2 > 1 ? hh - 2 : 1, i == v - d->min ? c : dim);
        } else if (d->fmt == F_BITS) {                   /* five generators: on / off */
            int32_t i;
            for (i = 0; i < 5; i++) {
                int on = (v >> i) & 1;
                if (it->id == CL_MUTE)
                    on = !on;                            /* (MUTE: lit = sounding) */
                cv_rect(x + 8, 100 - i * 18, 14, 14, on ? c : dim);
            }
        } else {                                         /* a bar, from the bottom (signed: from the middle) */
            int32_t lo = d->min, hi = d->max, y0 = 110, y1 = 10;
            cv_rect(x + 8, y1, 14, y0 - y1, RGB(36, 32, 50));
            if (lo < 0) {
                int32_t mid = (y0 + y1) / 2, y = mid - v * (y0 - y1) / (2 * (hi > -lo ? hi : -lo));
                cv_rect(x + 8, y < mid ? y : mid, 14, y < mid ? mid - y : y - mid + 1, c);
            } else
                cv_rect(x + 8, y0 - (v - lo) * (y0 - y1) / (hi - lo > 0 ? hi - lo : 1), 14,
                        (v - lo) * (y0 - y1) / (hi - lo > 0 ? hi - lo : 1), c);
        }
    }
    cv_blit(0, 32);
}

static void menu_bottom(void)
{
    uint32_t k;
    cv_begin(240, 84, C_BG);
    if (!big_draw(84)) {
        int perv = 1;
        for (k = 0; k < 8u; k++)                         /* a row of the 8 voices? then their numbers */
            perv &= ui.it[k].kind == IK_NONE || (ui.it[k].kind == IK_V && ui.it[k].voice == k);
        if ((int32_t)(ui.msg_until - fm1_ms) > 0)
            text_c(120, 26, &FONT_L, ui.msg, RGB(255, 255, 255));
        else
            for (k = 0; k < 8u; k++) {
                char b[16];
                if (ui.it[k].kind == IK_NONE)
                    continue;
                if (perv && ui.menu != MN_VOICE) {
                    b[0] = (char)('1' + k), b[1] = 0;
                    text_c(COLW * (int32_t)k + COLW / 2, 6, &FONT_L, b, rgbk(VCOL[k], 256));
                } else {                                 /* labels on two rows: room for longer words */
                    int32_t w, cx = COLW * (int32_t)k + COLW / 2;
                    it_label(&ui.it[k], b, sizeof b);
                    b[6] = 0;
                    w = text_w(&FONT_S, b);
                    cx = clamp(cx, w / 2 + 1, 239 - w / 2);      /* (inside the screen) */
                    text_c(cx, (k & 1u) ? 28 : 6, &FONT_S, b, rgbk(VCOL[k], 256));
                }
                it_text(&ui.it[k], b, sizeof b);
                b[3] = 0;
                text_c(COLW * (int32_t)k + COLW / 2, 56, &FONT_S, b, rgbk(VCOL[k], 200));
            }
    }
    cv_blit(0, 156);
}

/* ------------------------------------------------------------- PRESETS: the picker --- */
static void presets_screen(void)
{
    uint32_t k;
    uint32_t th = ui.pre_sel / PRESET_PAGE;
    char nm[32], b[8];
    const felucca_font_t *f;
    fmt_int(nm, ui.pre_sel % PRESET_PAGE + 1);
    str_cat(nm, " ", sizeof nm);
    str_cat(nm, PRESET_NAME[ui.pre_sel % NPRESET], sizeof nm);
    f = text_w(&FONT_XL, nm) <= 236 ? &FONT_XL : &FONT_L;
    cv_begin(240, 124, C_BG);
    cv_text(4, 0, &FONT_L, THEME_NAME[th], RGB(255, 220, 90));          /* the theme, and which of how many */
    fmt_int(b, th + 1);
    str_cat(b, "/", sizeof b);
    fmt_int(b + str_len(b), NTHEME);
    cv_text(236 - text_w(&FONT_L, b), 0, &FONT_L, b, RGB(150, 140, 190));
    text_c(120, 44, f, nm, RGB(255, 255, 255));
    text_c(120, 94, &FONT_L, "< >  THEME", RGB(150, 140, 190));
    cv_blit(0, 32);
    cv_begin(240, 84, C_BG);
    for (k = 0; k < 16u; k++) {                          /* the pads as they are lit: one preset each */
        uint32_t base = (ui.pre_sel / PRESET_PAGE) * PRESET_PAGE;
        int32_t x = 4 + 29 * (int32_t)(k & 7u), y = k < 8u ? 2 : 30;
        uint16_t c = base + k < NPRESET ? base + k == ui.preset ? RGB(255, 255, 255) : rgbk(VCOL[k & 7u], 160) : RGB(20, 18, 28);
        cv_rect(x, y, 25, 25, c);
    }
    text_c(120, 56, &FONT_L, "PATCH: HOME", RGB(150, 140, 190));
    cv_blit(0, 156);
}

/* ------------------------------------------------------------- a dialog --- */
static void dialog_screen(void)
{
    char q[24];
    cv_begin(240, 84, RGB(0, 0, 0));
    if (ui.dlg == DLG_PRESET) {
        str_cpy(q, PRESET_NAME[ui.dlg_arg % NPRESET], sizeof q);
        text_c(120, 0, &FONT_L, q, RGB(255, 255, 255));
    } else {
        str_cpy(q, ui.dlg == DLG_LOAD ? "LOAD " : "SAVE ", sizeof q);
        q[5] = (char)('1' + ui.dlg_arg), q[6] = '?', q[7] = 0;
        text_c(120, 0, &FONT_L, q, RGB(255, 255, 255));
    }
    cv_rect(COLW * 6 + 2, 40, COLW - 4, 40, RGB(255, 40, 40));   /* red: pad 15, green: pad 16 (under them) */
    cv_rect(COLW * 7 + 2, 40, COLW - 4, 40, RGB(40, 230, 70));
    text_c(COLW * 6 + COLW / 2, 44, &FONT_L, "X", RGB(0, 0, 0));
    text_c(120, 44, &FONT_L, "OK?", RGB(160, 240, 170));
    cv_blit(0, 156);
}

/* ------------------------------------------------------------- HELP (KNOB BANK held) --- */
static void help_lines(int32_t y, const char *const *ln, uint32_t n, uint16_t c)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        cv_text(4, y + 29 * (int32_t)i, &FONT_L, ln[i], c);
}
static void draw_help(void)
{
    static const char *const P[6][7] = {
        {"PADS", "", "", "", "TOP: VOICES", "BOTTOM: STEPS", "P HOLD CHAOS"},
        {"KNOBS", "HOME:", "K1-K8 PITCH", "MENU:", "KNOB=COLUMN", "2 CHOICES:", "THE PADS"},
        {"BUTTONS", "PARA   VOICE", "CHORD  ENV", "REPEAT MOD", "GLOBE  MIX", "SCALE  TUNING", "BT     LFO"},
        {"BUTTONS", "SEQ    SEQ", "FX     FX", "ARP    KEYS", "PATCH PRESET", "KNOBB  HOME", "< >    PAGE"},
        {"PLAY", "PLAY  FREE", "REC   HOLD", "OCT   TRANSP", "FADERB BANK", "PATCH+ SETUP", "KNOBB+ HELP"},
        {"FADERS", "F1  CUTOFF", "F2  RESO", "F3  FMOD RATE", "F4  FMOD DEPTH", "PITCH BEND", "MOD  SWEEP"}};
    uint32_t p, k;
    if (fm1_ms - ui.help_t >= 2500u) {
        ui.help_t = fm1_ms;
        ui.help_panel = (uint8_t)((ui.help_panel + 1u) % 6u);
    }
    p = ui.help_panel;
    cv_begin(240, 120, RGB(0, 0, 0));
    text_c(120, 0, &FONT_L, P[p][0], RGB(255, 255, 255));
    if (p == 0u) {                                       /* the pads in their colours */
        static const char BOT[8] = {'1', '2', '3', '4', '5', 'P', 'H', 'C'};
        for (k = 0; k < 16u; k++) {
            int32_t x = 1 + 30 * (int32_t)(k & 7u), y = k < 8u ? 34 : 76;
            char d[2] = {k < 8u ? (char)('1' + k) : BOT[k - 8u], 0};
            uint16_t c = k < 8u ? rgbk(VCOL[k], 256) : k < 13u ? RGB(150, 70, 255) : k == 13u ? RGB(255, 210, 40)
                         : k == 14u ? RGB(200, 200, 220) : RGB(255, 60, 200);
            cv_rect(x, y, 28, 38, c);
            text_c(x + 14, y + 3, &FONT_L, d, RGB(0, 0, 0));
        }
    } else
        help_lines(30, &P[p][1], 3, RGB(220, 220, 240));
    cv_blit(0, 0);
    cv_begin(240, 120, RGB(0, 0, 0));
    help_lines(2, &P[p][4], 3, RGB(220, 220, 240));
    for (k = 0; k < 6u; k++)
        cv_rect(84 + 12 * (int32_t)k, 112, 8, 6, k == p ? RGB(255, 255, 255) : RGB(70, 66, 90));
    cv_blit(0, 120);
}

/* ------------------------------------------------------------- the frame --- */
static void draw_frame(void)
{
    static uint32_t t_cre = 0xFFFF0000u, t_rest, t_help;
    static uint8_t last_menu = 0xFF, last_page, in_help, last_dlg;
    uint32_t now = fm1_ms, v, np = 1;
    if (ui.help) {
        if (!in_help || now - t_help >= 200u) {
            in_help = 1;
            t_help = now;
            draw_help();
        }
        return;
    }
    if (ui.menu != MN_HOME)
        np = menu_build();
    if (in_help || last_menu != ui.menu || last_page != ui.page || last_dlg != ui.dlg) {   /* anew */
        in_help = 0;
        last_menu = ui.menu, last_page = ui.page, last_dlg = ui.dlg;
        for (v = 0; v < NV; v++)
            cre[v].drawn = 0;
        lcd_fill(0, 0, 240, 240, C_BG);
        ui.dirty = 1;
        t_rest = now - 1000u;
    }
    if (ui.menu == MN_HOME) {
        if (now - t_cre >= 40u) {                        /* creatures: only the cells that changed */
            t_cre = now;
            fuzz_seed = now >> 7;
            for (v = 0; v < NV; v++)
                creature_cell(v, 0);
        }
        if (ui.dirty || now - t_rest >= 100u) {
            ui.dirty = 0;
            t_rest = now;
            home_header();
            if (ui.dlg)
                dialog_screen();
            else
                home_bottom();
        }
        return;
    }
    if (ui.dirty || now - t_rest >= 120u) {
        ui.dirty = 0;
        t_rest = now;
        menu_header(np);
        if (ui.menu == MN_PRESETS && ui.page == 0u)
            presets_screen();
        else {
            menu_columns();
            if (ui.dlg)
                dialog_screen();
            else
                menu_bottom();
        }
    }
}

/* ------------------------------------------------------------- boot: a neon 3D scene --- */
/* the camera flies in over a neon grid towards the creatures standing in a row, swinging round to face them;
 * they wake one by one (a rising chord), the row lands where the home screen has it, then the name */
#define BOOT_F 200
#define BOOT_HOR 64
#define BOOT_H 250                              /* the camera's height above the ground, x 100 */
static void boot_frame(uint32_t t)
{
    static const int16_t CHORD[NV] = {4800, 5200, 5500, 5900, 6000, 6400, 6700, 7100};   /* C E G B C E G B */
    static uint8_t rung;
    int32_t e = t >= 1400u ? 256 : (int32_t)(t * 256u / 1400u), a, ca, sa, d, half;
    e = 256 - ((256 - e) * (256 - e) >> 8);              /* ease out */
    a = (256 - e) * 60 / 256;                            /* yaw: 60 degrees .. 0 */
    ca = SINE[256 + a * 256 / 90 % 1024] , sa = SINE[a * 256 / 90];
    d = 667 + (256 - e) * 1900 / 256;                    /* camera distance x 100: 25.7 .. 6.67 units */
    if (t == 0u)
        rung = 0;
    for (half = 0; half < 2; half++) {
        int32_t y0 = half * 120, k;
        cv_begin(240, 120, RGB(4, 0, 12));
        cv_oy = -y0;
        for (k = 0; k < 30; k++)                         /* the sky's glow above the horizon */
            cv_rect(0, BOOT_HOR - 30 + k, 240, 1, RGB(10 + k * 2, 0, 20 + k * 2));
        for (k = 0; k < 24; k++) {                       /* a striped neon sun */
            int32_t r = 38, y = BOOT_HOR - 2 - k * 2, w;
            int32_t dy = BOOT_HOR - 22 - y, ww = r * r - dy * dy;
            if (ww <= 0 || (k % 4) == 3)
                continue;
            for (w = 0; (w + 1) * (w + 1) <= ww; w++)
                ;
            cv_rect(120 - w, y, 2 * w, 2, RGB(255, 120 - k * 3, 60 + k * 6));
        }
        for (k = -12; k <= 12; k++) {                    /* the grid: lines along z */
            int32_t x0 = k * 100, z0 = -2000, z1 = 4000, X0, Z0, X1, Z1, sx0, sx1, sy0, sy1;
            X0 = (x0 * ca - z0 * sa) >> 15, Z0 = ((x0 * sa + z0 * ca) >> 15) + d;
            X1 = (x0 * ca - z1 * sa) >> 15, Z1 = ((x0 * sa + z1 * ca) >> 15) + d;
            if (Z0 < 60) {                               /* (cut where it passes the camera) */
                X0 = X1 + (X0 - X1) * (Z1 - 60) / (Z1 - Z0 > 0 ? Z1 - Z0 : 1);
                Z0 = 60;
            }
            if (Z1 < 60) Z1 = 60;
            sx0 = 120 + X0 * BOOT_F / Z0, sy0 = BOOT_HOR + BOOT_H * BOOT_F / Z0;
            sx1 = 120 + X1 * BOOT_F / Z1, sy1 = BOOT_HOR + BOOT_H * BOOT_F / Z1;
            cv_line(sx0, sy0, sx1, sy1, RGB(200, 30, 170));
        }
        for (k = 0; k < 24; k++) {                       /* across, scrolling towards us */
            int32_t z = k * 160 - 1200 - (int32_t)((t / 4u) % 160u), Zc = z + d, sy;
            if (Zc < 60)
                continue;
            sy = BOOT_HOR + BOOT_H * BOOT_F / Zc;
            cv_rect(0, sy, 240, 1, RGB(200 * 300 / (Zc > 300 ? Zc : 300), 30, 170 * 300 / (Zc > 300 ? Zc : 300)));
        }
        {   /* the creatures: far ones first */
            int32_t order[NV], v, i, j;
            for (i = 0; i < NV; i++)
                order[i] = i;
            for (i = 0; i < NV; i++)
                for (j = i + 1; j < NV; j++) {
                    int32_t zi = (((order[i] * 100 - 350) * sa) >> 15), zj = (((order[j] * 100 - 350) * sa) >> 15);
                    if (zj > zi) {
                        int32_t tmp = order[i];
                        order[i] = order[j], order[j] = tmp;
                    }
                }
            for (i = 0; i < NV; i++) {
                int32_t x = (order[i] - 3) * 100 - 50, X = (x * ca) >> 15, Z = ((x * sa) >> 15) + d, r, sx, sy, wake;
                v = order[i];
                if (Z < 100)
                    continue;
                r = 45 * BOOT_F / Z;                     /* body radius in pixels */
                sx = 120 + X * BOOT_F / Z;
                sy = BOOT_HOR + BOOT_H * BOOT_F / Z - r - 4;
                wake = (int32_t)t - 600 - v * 90;
                if (wake > 0 && rung <= (uint32_t)v && half == 0) {
                    chime_hit(CHORD[v]);
                    rung = (uint8_t)(v + 1);
                }
                creature(sx, sy, r * 11 / 14, r, wake > 0 ? 22000 : 0, wake > 0 && wake < 260 ? 256 - wake : 0,
                         wake <= 0, 0, VCOL[v], v + (t >> 7));
            }
        }
        if (t > 1300u) {                                 /* the name, then the credit */
            int32_t k2 = clamp((int32_t)(t - 1300u) * 256 / 400, 0, 256);
            cv_rect(0, 158, 240, 82, RGB(4, 0, 12));     /* (a calm band under the name) */
            text_c(120, 162, &FONT_XL, "MAKKURO37", RGB(255 * k2 >> 8, 64 * k2 >> 8, 200 * k2 >> 8));
            text_c(120, 208, &FONT_L, "by genkigenki", RGB(200 * k2 >> 8, 190 * k2 >> 8, 240 * k2 >> 8));
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
    }
}

static void boot_anim(void)
{
    uint32_t t0 = fm1_ms;
    for (;;) {
        uint32_t t = fm1_ms - t0;
        fm1_wdt_feed();
        if (t > 2100u || (fm1_in.keys && t > 300u))
            break;
        boot_frame(t);
        fm1_delay_ms(10);
    }
    lcd_fill(0, 0, 240, 240, C_BG);
}

static void splash(void)                                 /* before the audio runs: the name only */
{
    lcd_fill(0, 0, 240, 240, C_BG);
    cv_begin(240, 50, C_BG);
    text_c(120, 0, &FONT_XL, "MAKKURO37", RGB(255, 64, 200));
    cv_blit(0, 96);
    lcd_sync();
}
