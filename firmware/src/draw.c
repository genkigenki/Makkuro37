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
#define CRH 84
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
    if ((uint8_t)(vs[v].hits + ui.czhit[v]) != cre[v].hits) {   /* woken, or rolled by CHAOS: a squish */
        cre[v].hits = (uint8_t)(vs[v].hits + ui.czhit[v]);
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
    creature(COLW / 2, 36 + bob, 12, 17, lv, hit, sleep, blink, VCOL[v], sleep ? fuzz_seed >> 4 : fuzz_seed + v);
    if (sleep && ((now >> 10) + v) % 4u == 0u)
        cv_text(COLW - 9, 3 - (int32_t)((now >> 7) & 3u), &FONT_S, "z", RGB(170, 160, 220));
    if (gs != GS_PAD) {                                               /* who plays it besides the pad */
        static const char *const GW[GS_N] = {"", "SEQ", "LFO", "LFO", "CAS"};
        text_c(COLW / 2, CRH - 17, &FONT_S, GW[gs % GS_N], rgbk(VCOL[v], 200));
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
static int big_draw(int32_t y0, int32_t h)               /* into a 240-wide canvas, rows y0 .. y0 + h: 1 if shown */
{
    static const uint8_t WHITE[3] = {255, 255, 255};
    const uint8_t *c = big.col < NV ? VCOL[big.col] : WHITE;
    const felucca_font_t *f;
    if ((int32_t)(big.until - fm1_ms) <= 0)
        return 0;
    f = text_w(&FONT_XL, big.val) <= 236 ? &FONT_XL : &FONT_L;
    if (big.col < NV)                                    /* a mark under its column */
        cv_rect(COLW * big.col + 2, y0, COLW - 4, 4, rgbk(c, 256));
    text_c(120, y0 + 4, &FONT_L, big.label, rgbk(c, 200));
    text_c(120, y0 + h - f->h, f, big.val, rgbk(c, 256));
    return 1;
}

static void home_header(void)
{
    char b[16];
    cv_begin(240, 32, C_BG);
    preset_name(ui.preset, b, sizeof b);
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

/* below the creatures: each voice's note (one row), then the turned value / a message / CHAOS / KB, then what
 * the bottom pads do */
static void home_bottom(void)
{
    static const char *const BOT[8] = {"KB", "", "", "", "", "PULSE", "HOLD", "CHAOS"};
    uint32_t v, bl = (fm1_ms >> 8) & 1u, cz = (int32_t)(ui.cz.until - fm1_ms) > 0;
    cv_begin(240, 124, C_BG);
    for (v = 0; v < NV; v++) {                           /* the notes: selected (KB) boxed, rolling (CHAOS) white */
        char b[8];
        int32_t x = COLW * (int32_t)v;
        int roll = cz && ui.cz.n < 3u && ((ui.cz.mask >> v) & 1u);
        if ((ui.nsel >> v) & 1u)
            cv_rect(x + 1, 0, COLW - 2, 17, rgbk(VCOL[v], 256));
        cents_text(b, sizeof b, voice_note_now(v));
        text_c(x + COLW / 2, 1, &FONT_S, b, (ui.nsel >> v) & 1u ? RGB(0, 0, 0) : roll ? RGB(255, 255, 255)
                                                                   : rgbk(VCOL[v], 256));
    }
    if (big_draw(20, 70))
        ;                                                /* (a turned knob: its value) */
    else if (cz) {                                       /* CHAOS: each voice's three notes, the kept one boxed */
        for (v = 0; v < NV; v++) {
            int32_t x = COLW * (int32_t)v + COLW / 2, r;
            if (!((ui.cz.mask >> v) & 1u))
                continue;
            for (r = 0; r < (int32_t)ui.cz.n; r++) {
                char b[8];
                int last = r == 2;
                cents_text(b, sizeof b, ui.cz.hist[v][r]);
                if (last)
                    cv_rect(x - 14, 22 + 22 * r, 28, 19, rgbk(VCOL[v], 256));
                text_c(x, 24 + 22 * r, &FONT_S, b, last ? RGB(0, 0, 0) : rgbk(VCOL[v], r ? 150 : 90));
                if (!last)
                    cv_rect(x - 11, 31 + 22 * r, 22, 1, rgbk(VCOL[v], 120));   /* (struck out) */
            }
        }
    } else if (ui.kbm)                                   /* KB: what to do, blinking until a voice is selected */
        text_c(120, 40, &FONT_L, ui.nsel ? "PLAY KEYBOARD" : "SELECT VOICES",
               ui.nsel || bl ? RGB(60, 220, 255) : RGB(20, 70, 90));
    else if ((int32_t)(ui.msg_until - fm1_ms) > 0)
        text_c(120, 40, &FONT_L, ui.msg, RGB(255, 255, 255));
    for (v = 0; v < 8u; v++) {                           /* the bottom pads' words, staggered on two rows */
        int32_t w = text_w(&FONT_S, BOT[v]), x = clamp(COLW * (int32_t)v + COLW / 2, w / 2, 240 - w / 2);
        uint16_t c = v == 0u ? RGB(60, 220, 255) : v == 5u ? RGB(255, 210, 40) : v == 6u ? RGB(200, 200, 220)
                     : RGB(255, 60, 200);
        text_c(x, (v & 1u) ? 106 : 90, &FONT_S, BOT[v], v == 0u && ui.kbm && bl ? RGB(255, 255, 255) : c);
    }
    cv_blit(0, 116);
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
static void menu_header(const char *name, uint32_t p, uint32_t n)   /* the menu's name, its page "p/n" */
{
    char b[8];
    cv_begin(240, 32, C_BG);
    cv_text(2, 0, &FONT_L, name, RGB(235, 235, 245));
    fmt_int(b, (int32_t)p + 1);
    str_cat(b, "/", sizeof b);
    fmt_int(b + str_len(b), (int32_t)n);
    cv_text(236 - text_w(&FONT_L, b), 0, &FONT_L, b, RGB(150, 140, 190));
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
        if (it->alt != NO && ((ui.altmask >> k) & 1u))   /* a pair showing its second value: lighter */
            c = RGB((VCOL[k][0] + 255) >> 1, (VCOL[k][1] + 255) >> 1, (VCOL[k][2] + 255) >> 1);
        cv_rect(x + 1, 0, COLW - 2, 3, c);              /* the column's colour, under its knob */
        v = it_get(it);
        if (it->kind == IK_V && (it->id == VP_WAVE || (it->id == VC_WAVE && VTYPE[it->voice] == VT_VCO))) {
            int32_t yy, ly = 0;                          /* WAVE: the wave itself, down the column */
            for (yy = 0; yy < 96; yy++) {
                uint32_t ph = (uint32_t)yy * 0x5555555u;   /* (two periods) */
                int vco = !(it->id == VP_WAVE && VTYPE[it->voice] != VT_VCO);
                int32_t s2 = (vco ? v == W_NOISE : v == CW_NOISE) ? (int32_t)(hash3(yy / 4, k, 7) & 0xFFFFu) - 32768
                             : vco ? wave_y((uint32_t)v, ph, pt.v[it->voice][VC_SHAPE]) : cwave((uint32_t)v, ph, 0);
                int32_t xx = x + 15 + (s2 * 11 >> 15);
                if (yy)
                    cv_thick(x + 15 + ly, 10 + yy - 1, xx, 10 + yy, c);
                ly = xx - x - 15;
            }
            continue;
        }
        if (it->pid != NO || it->bid != NO) {            /* its pads: a bit each (left top, right bottom) */
            int32_t x2 = it->kind == IK_G ? pt.g[it->pid] : pt.v[it->voice][it->pid];
            if (it->pid != NO)
                cv_rect(x + 4, 113, 10, 10, (((uint32_t)x2 >> it->pbit) & 1u) ^ it->pinv ? c : dim);
            if (it->bid != NO)
                cv_rect(x + 16, 113, 10, 10, ((uint32_t)pt.v[it->voice][it->bid] >> it->pbit) & 1u ? c : dim);
        }
        if (it_onpads(it)) {                             /* two pads: the lit one is the choice */
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

static void help_text(int32_t y, const char *s)           /* lines (\n) of FONT_S, centred */
{
    while (s && *s) {
        char b[32];
        uint32_t n = 0;
        while (s[n] && s[n] != '\n' && n < sizeof b - 1u)
            b[n] = s[n], n++;
        b[n] = 0;
        text_c(120, y, &FONT_S, b, RGB(150, 140, 190));
        s += n + (s[n] == '\n');
        y += 16;
    }
}
static void menu_columns_text(void)   /* a row of the voices: the page's name; else each column's; the values */
{
    uint32_t k;
    const page_t *pg = page_cur();
    int row = pg->kind == PG_ROW;
    help_text(50, pg->kind == PG_VOICE ? VHELP[VTYPE[pg->id[0]]] : pg->help);
    if (row)
        text_c(120, -2, &FONT_L, pg->name, RGB(255, 220, 90));
    for (k = 0; k < 8u; k++) {
        char b[16];
        if (ui.it[k].kind == IK_NONE)
            continue;
        if (!row) {                                      /* labels on two rows: room for longer words */
            int32_t w, cx = COLW * (int32_t)k + COLW / 2;
            it_label(&ui.it[k], b, sizeof b);
            b[6] = 0;
            w = text_w(&FONT_S, b);
            cx = clamp(cx, w / 2 + 1, 239 - w / 2);      /* (inside the screen) */
            text_c(cx, (k & 1u) ? 15 : 0, &FONT_S, b, rgbk(VCOL[k], 256));
        }
        it_text(&ui.it[k], b, sizeof b);
        b[3] = 0;
        text_c(COLW * (int32_t)k + COLW / 2, 31, &FONT_S, b, rgbk(VCOL[k], 200));
    }
}
static void menu_bottom(void)                 /* a turned knob's value, big; a message; or the page's words */
{
    cv_begin(240, 84, C_BG);
    if (!big_draw(0, 84)) {
        if ((int32_t)(ui.msg_until - fm1_ms) > 0)
            text_c(120, 26, &FONT_L, ui.msg, RGB(255, 255, 255));
        else
            menu_columns_text();
    }
    cv_blit(0, 156);
}

/* ------------------------------------------------------------- PRESETS: the picker --- */
static void presets_screen(void)
{
    uint32_t k, th = ui.pre_sel / PRESET_PAGE, base = th * PRESET_PAGE;
    char nm[32];
    const felucca_font_t *f;
    uint32_t n = ui.preset / PRESET_PAGE == th ? ui.preset : base;
    menu_header(th < NTHEME ? THEME_NAME[th] : "MINE", th, NTHEME + 1u);
    nm[0] = 0;
    if (th < NTHEME) {                                   /* "5 RITUAL PAD"; MINE: "MINE 3", "EMPTY 4" */
        fmt_int(nm, (int32_t)(n % PRESET_PAGE + 1u));
        str_cat(nm, " ", sizeof nm);
    }
    preset_name(n, nm + str_len(nm), sizeof nm - str_len(nm));
    f = text_w(&FONT_XL, nm) <= 236 ? &FONT_XL : &FONT_L;
    cv_begin(240, 124, C_BG);
    for (k = 0; k < 8u; k++) {                           /* K1-K8: the filters, a bar each (the type in use lit) */
        const pdesc_t *d = &GDESC[LENS[k].id];
        int32_t x = COLW * (int32_t)k, h = (pt.g[LENS[k].id] - d->min) * 16 / (d->max - d->min);
        int on = LENS[k].mode == NO || pt.g[G_FMODE] == LENS[k].mode;
        const char *b = LENS[k].name;
        cv_rect(x + 2, 0, COLW - 4, 3, rgbk(VCOL[k], on ? 256 : 70));
        cv_rect(x + 8, 22 - h, 14, h, rgbk(VCOL[k], on ? 256 : 70));
        text_c(clamp(x + COLW / 2, text_w(&FONT_S, b) / 2, 240 - text_w(&FONT_S, b) / 2), (k & 1u) ? 40 : 24,
               &FONT_S, b, rgbk(VCOL[k], on ? 230 : 90));
    }
    text_c(120, 60, f, nm, RGB(255, 255, 255));
    text_c(120, 108, &FONT_S, th < NTHEME ? "< > THEME   KNOBS: FILTERS" : "HOLD A PAD: SAVE", RGB(150, 140, 190));
    cv_blit(0, 32);
    if (ui.dlg)
        return;
    cv_begin(240, 84, C_BG);
    if (big_draw(0, 84)) {
        cv_blit(0, 156);
        return;
    }
    for (k = 0; k < 16u; k++) {                          /* the pads as they are lit: one preset each */
        uint32_t m = base + k, has = m < NPRESET || ((user_used >> (m - NPRESET)) & 1u);
        int32_t x = 4 + 29 * (int32_t)(k & 7u), y = k < 8u ? 2 : 30;
        cv_rect(x, y, 25, 25, m == ui.preset ? RGB(255, 255, 255) : rgbk(VCOL[k & 7u], has ? 160 : 30));
    }
    text_c(120, 56, &FONT_L, "PATCH: HOME", RGB(150, 140, 190));
    cv_blit(0, 156);
}

/* ------------------------------------------------------------- FADERS: the bank in use --- */
/* four faders as on the panel: the value filled, a white line where the fader stands; below, the banks as the pads */
static const uint8_t FCOL[4] = {0, 2, 4, 6};            /* (the colours: VCOL) */
static void faders_screen(void)
{
    uint32_t i;
    menu_header(FBANK[ui.fbank].name, ui.fbank, NFBANK);
    cv_begin(240, 124, C_BG);
    for (i = 0; i < 4u; i++) {
        const fdest_t *f = &FBANK[ui.fbank].f[i];
        const pdesc_t *d = fd_desc(f);
        const uint8_t *c = VCOL[FCOL[i]];
        int32_t x = 60 * (int32_t)i + 22, y0 = 118, y1 = 6, span = d->max - d->min > 0 ? d->max - d->min : 1;
        int32_t h = (fd_get(f) - d->min) * (y0 - y1) / span, y = y0 - (int32_t)fm1_in.fader[i] * (y0 - y1) / 127;
        cv_rect(x, y1, 16, y0 - y1, rgbk(c, 40));
        cv_rect(x, y0 - h, 16, h, rgbk(c, 256));
        cv_rect(x - 10, clamp(y - 2, 0, 120), 36, 4, RGB(255, 255, 255));   /* (the fader) */
    }
    cv_blit(0, 32);
    cv_begin(240, 84, C_BG);
    for (i = 0; i < 4u; i++) {
        const fdest_t *f = &FBANK[ui.fbank].f[i];
        char b[8];
        fmt_int(b, fd_get(f));
        text_c(60 * (int32_t)i + 30, 0, &FONT_S, f->name, rgbk(VCOL[FCOL[i]], 230));
        text_c(60 * (int32_t)i + 30, 16, &FONT_L, b, rgbk(VCOL[FCOL[i]], 256));
    }
    for (i = 0; i < NFBANK; i++) {                       /* the top pads: one bank each */
        char a[2] = {FBANK[i].name[0], 0};
        int32_t x = COLW * (int32_t)i + 2;
        cv_rect(x, 56, COLW - 4, 26, i == ui.fbank ? RGB(255, 255, 255) : rgbk(VCOL[i], 150));
        text_c(x + (COLW - 4) / 2, 61, &FONT_S, a, RGB(0, 0, 0));
    }
    text_c(210, 61, &FONT_S, "< >", RGB(150, 140, 190));
    cv_blit(0, 156);
}

/* ------------------------------------------------------------- a dialog --- */
static void dialog_screen(void)
{
    char q[24] = "REPLACE ";
    fmt_int(q + str_len(q), ui.dlg_arg + 1);
    str_cat(q, "?", sizeof q);
    cv_begin(240, 84, RGB(0, 0, 0));
    text_c(120, 0, &FONT_L, q, RGB(255, 255, 255));
    cv_rect(COLW * 6 + 2, 40, COLW - 4, 40, RGB(255, 40, 40));   /* red: pad 15, green: pad 16 (under them) */
    cv_rect(COLW * 7 + 2, 40, COLW - 4, 40, RGB(40, 230, 70));
    text_c(COLW * 6 + COLW / 2, 44, &FONT_L, "X", RGB(0, 0, 0));
    cv_thick(COLW * 7 + 7, 60, COLW * 7 + 12, 66, RGB(0, 0, 0));    /* a tick */
    cv_thick(COLW * 7 + 12, 66, COLW * 7 + 21, 52, RGB(0, 0, 0));
    text_c(COLW * 6 + COLW / 2, 22, &FONT_S, "NO", RGB(255, 90, 90));
    text_c(COLW * 7 + COLW / 2 - 4, 22, &FONT_S, "YES", RGB(90, 240, 120));
    text_c(84, 48, &FONT_S, "PADS 15 / 16", RGB(150, 140, 190));
    cv_blit(0, 156);
}

/* ------------------------------------------------------------- HELP (KNOB BANK held) --- */
/* six panels of six lines; the menus' lines come from the table (ui.c) */
#define HELP_N 6u
static const char *help_line(uint32_t p, uint32_t i, char *b, uint32_t n)
{
    static const char *const T[HELP_N] = {"PADS", "KNOBS", "MENUS", "MENUS", "PLAY", "FADERS"};
    static const char *const L[HELP_N][6] = {
        {"", "", "", "KB KEYBOARD", "P PULSE H HOLD", "C CHAOS"},
        {"HOME:", "K1-K8 PITCH", "MENU:", "KNOB=COLUMN", "2 CHOICES:", "THE PADS"},
        {0, 0, 0, 0, 0, 0},
        {0, 0, "KNOBB  PRESETS", "PATCH  HOME", "PATCH+ SETUP", "< >    PAGE"},
        {"PLAY  FREE", "REC   HOLD", "OCT   TRANSP", "FADERB BANKS", "KNOBB+ HELP", ""},
        {0, 0, 0, 0, "PITCH BEND", "MOD  CLOSE"}};
    if (i == 0xFFu)
        return T[p];
    if (L[p][i])
        return L[p][i];
    if (p == 5u) {                                       /* the faders */
        str_cpy(b, "F1 ", n);
        b[1] = (char)('1' + i);
        str_cat(b, FBANK[ui.fbank].f[i].name, n);
        return b;
    }
    {   /* the menus: button, name */
        uint32_t m = MN_SOUND + (p - 2u) * 6u + i;
        str_cpy(b, MENUS[m].button, n);
        while (str_len(b) < 7u && str_len(b) + str_len(MENUS[m].name) < 14u)
            str_cat(b, " ", n);
        if (str_len(b) == str_len(MENUS[m].button))
            str_cat(b, " ", n);
        str_cat(b, MENUS[m].name, n);
        return b;
    }
}
static void draw_help(void)
{
    uint32_t p, k;
    char b[24];
    if (fm1_ms - ui.help_t >= 2500u) {
        ui.help_t = fm1_ms;
        ui.help_panel++;
    }
    p = ui.help_panel % HELP_N;
    cv_begin(240, 120, RGB(0, 0, 0));
    text_c(120, 0, &FONT_L, help_line(p, 0xFFu, b, sizeof b), RGB(255, 255, 255));
    if (p == 0u) {                                       /* the pads in their colours */
        static const char BOT[8] = {'K', ' ', ' ', ' ', ' ', 'P', 'H', 'C'};
        for (k = 0; k < 16u; k++) {
            int32_t x = 1 + 30 * (int32_t)(k & 7u), y = k < 8u ? 34 : 76;
            char d[2] = {k < 8u ? (char)('1' + k) : BOT[k - 8u], 0};
            uint16_t c = k < 8u ? rgbk(VCOL[k], 256) : k == 8u ? RGB(60, 220, 255) : k == 13u ? RGB(255, 210, 40)
                         : k == 14u ? RGB(200, 200, 220)
                         : k == 15u ? RGB(255, 60, 200) : RGB(30, 28, 40);
            cv_rect(x, y, 28, 38, c);
            text_c(x + 14, y + 3, &FONT_L, d, RGB(0, 0, 0));
        }
    } else
        for (k = 0; k < 3u; k++)
            cv_text(4, 30 + 29 * (int32_t)k, &FONT_L, help_line(p, k, b, sizeof b), RGB(220, 220, 240));
    cv_blit(0, 0);
    cv_begin(240, 120, RGB(0, 0, 0));
    for (k = 3; k < 6u; k++)
        cv_text(4, 2 + 29 * (int32_t)(k - 3u), &FONT_L, help_line(p, k, b, sizeof b), RGB(220, 220, 240));
    for (k = 0; k < HELP_N; k++)
        cv_rect(84 + 12 * (int32_t)k, 112, 8, 6, k == p ? RGB(255, 255, 255) : RGB(70, 66, 90));
    cv_blit(0, 120);
}

/* ------------------------------------------------------------- the frame --- */
static void draw_frame(void)
{
    static uint32_t t_cre = 0xFFFF0000u, t_rest, t_help;
    static uint8_t last_menu = 0xFF, last_page, in_help, last_dlg;
    uint32_t now = fm1_ms, v;
    if (ui.help) {
        if (!in_help || now - t_help >= 200u) {
            in_help = 1;
            t_help = now;
            draw_help();
        }
        return;
    }
    menu_build();
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
        if (ui.menu == MN_FADERS)
            faders_screen();
        else if (ui.menu == MN_PRESETS) {
            presets_screen();
            if (ui.dlg)
                dialog_screen();
        } else {
            menu_header(MENUS[ui.menu].name, ui.page, MENUS[ui.menu].n);
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
