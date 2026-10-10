/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* The screen: eight voice suns (top, 240 x 124), the page's four values (bottom, 240 x 116). Neon on black. */
#define C_BG RGB(4, 0, 10)
static const uint8_t KCOL[4][3] = {{40, 124, 255}, {30, 204, 112}, {255, 198, 24}, {255, 98, 26}};   /* K5..K8 */

static uint16_t rgbk(const uint8_t *c, int32_t k)          /* colour c at k / 256 */
{
    k = clamp(k, 0, 256);
    return RGB(c[0] * k >> 8, c[1] * k >> 8, c[2] * k >> 8);
}

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
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t y, x;
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++)
            if (x * x + y * y <= r * r)
                cv_pset(cx + x, cy + y, c);
}

/* ------------------------------------------------------------- suns --- */
static uint32_t sun_ph[NV][5];             /* ring pulse phases */

static void draw_suns(void)
{
    static uint32_t last;
    uint32_t v, g, dt = fm1_ms - last;
    last = fm1_ms;
    cv_begin(240, 124, C_BG);
    cv_text(4, 2, &FONT_S, BRAND_NAME, RGB(150, 40, 110));
    {
        const char *t = TSETS[pt.g[G_TSET] % TS_N].name;
        cv_text(236 - text_w(&FONT_S, t), 2, &FONT_S, t, RGB(90, 70, 140));
    }
    for (v = 0; v < NV; v++) {
        int32_t cx = 30 + 60 * (int32_t)(v & 3u), cy = 46 + 52 * (int32_t)(v >> 2), lv = vs[v].level;
        int32_t glow = 40 + (lv >> 7);                      /* 40..296 / 256 */
        uint32_t t = VTYPE[v], nring = t == VT_CLASSIC ? 5u : 2u;
        if (v == ui.voice)
            cv_circle(cx, cy, 25, RGB(200, 200, 220));
        for (g = 0; g < nring; g++) {
            uint32_t inc = vs[v].inc[g];
            int32_t r = t == VT_CLASSIC ? 4 + 4 * (int32_t)g : 9 + 10 * (int32_t)g, k;
            if (t == VT_CLASSIC && ((pt.v[v][CL_MUTE] >> g) & 1)) {
                cv_circle(cx, cy, r, rgbk(VCOL[v], 24));
                continue;
            }
            /* pulse at the generator's pitch, slowed down (each octave doubles it) */
            sun_ph[v][g] += (inc >> 3) * dt;
            k = SINE[sun_ph[v][g] >> 22] + 32768;          /* 0..65535 */
            k = glow * (160 + (k >> 9)) >> 8;
            cv_circle(cx, cy, r, rgbk(VCOL[v], k));
            if (lv > 2000)
                cv_circle(cx, cy, r + 1, rgbk(VCOL[v], k / 3));
        }
        if (t != VT_CLASSIC)
            cv_disc(cx, cy, 4 + (lv >> 12), rgbk(VCOL[v], glow));
        else
            cv_disc(cx, cy, 1 + (lv >> 13), rgbk(VCOL[v], glow));
        if (ui.latched[v])
            cv_pset(cx + 21, cy - 21, RGB(255, 255, 255));
    }
    cv_blit(0, 0);
}

/* ------------------------------------------------------------- the page --- */
static const char *const VT_NAME[3] = {"CLASSIC", "NOISE", "VCO"};

static void draw_page(void)
{
    char b[32];
    uint32_t k;
    page_build();
    cv_begin(240, 116, C_BG);
    cv_rect(0, 0, 240, 1, RGB(60, 20, 70));
    if (ui.settings)
        str_cpy(b, "SETTINGS", sizeof b);
    else {
        str_cpy(b, PAGE_NAME[ui.page], sizeof b);
        if (page_subs() > 1u) {
            uint32_t n = str_len(b);
            b[n] = ' ', b[n + 1] = (char)('1' + ui.sub), b[n + 2] = '/', b[n + 3] = (char)('0' + page_subs()), b[n + 4] = 0;
        }
    }
    cv_text(4, 4, &FONT_S, b, RGB(230, 230, 240));
    if (!ui.settings && ui.page != PG_SCENES && ui.page != PG_TUNING && ui.page != PG_LFO && ui.page != PG_SEQ &&
        ui.page != PG_FILTER && ui.page != PG_FX) {
        b[0] = 'V', b[1] = (char)('1' + ui.voice), b[2] = ' ';
        str_cpy(b + 3, VT_NAME[VTYPE[ui.voice]], sizeof b - 3);
        cv_text(236 - text_w(&FONT_S, b), 4, &FONT_S, b, rgbk(VCOL[ui.voice], 256));
    }
    for (k = 0; k < 4u; k++) {
        uint32_t i = ui.sub * 4u + k;
        int32_t x = 60 * (int32_t)k, w;
        const item_t *it;
        const pdesc_t *d;
        if (i >= ui.nit || ui.it[i].kind == IK_NONE)
            continue;
        it = &ui.it[i];
        d = it_desc(it);
        it_label(it, b, sizeof b);
        cv_text(x + 30 - text_w(&FONT_S, b) / 2, 28, &FONT_S, b, rgbk(KCOL[k], 150));
        it_text(it, b, sizeof b);
        w = text_w(&FONT_L, b);
        if (w <= 58 && it->kind != IK_ACT)
            cv_text(x + 30 - w / 2, 48, &FONT_L, b, rgbk(KCOL[k], 256));
        else
            cv_text(x + 30 - text_w(&FONT_S, b) / 2, 56, &FONT_S, b, rgbk(KCOL[k], 256));
        if (d && d->max > d->min) {                         /* where the value sits in its range */
            int32_t f = (it_get(it) - d->min) * 52 / (d->max - d->min);
            cv_rect(x + 4, 88, 52, 2, rgbk(KCOL[k], 50));
            cv_rect(x + 4, 88, f, 2, rgbk(KCOL[k], 200));
        }
    }
    if ((int32_t)(ui.msg_until - fm1_ms) > 0)
        cv_text(120 - text_w(&FONT_S, ui.msg) / 2, 98, &FONT_S, ui.msg, RGB(255, 80, 200));
    else {
        char o[16] = "OCT ";
        if (ui.oct) {
            o[4] = ui.oct > 0 ? '+' : '-';
            o[5] = (char)('0' + (ui.oct > 0 ? ui.oct : -ui.oct));
            o[6] = 0;
            cv_text(4, 98, &FONT_S, o, RGB(90, 70, 140));
        }
        str_cpy(o, "MST ", sizeof o);
        fmt_int(o + 4, pt.g[G_MASTER]);
        cv_text(236 - text_w(&FONT_S, o), 98, &FONT_S, o, RGB(90, 70, 140));
    }
    cv_blit(0, 124);
}

static void draw_frame(void)
{
    static uint32_t t_suns, t_page;
    if (fm1_ms - t_suns >= 50u) {          /* ~20 frames / s */
        t_suns = fm1_ms;
        draw_suns();
    }
    if (ui.dirty || fm1_ms - t_page >= 250u) {
        ui.dirty = 0;
        t_page = fm1_ms;
        draw_page();
    }
}

static void splash(void)
{
    lcd_fill(0, 0, 240, 240, C_BG);
    cv_begin(240, 80, C_BG);
    cv_text(120 - text_w(&FONT_L, BRAND_NAME) / 2, 10, &FONT_L, BRAND_NAME, RGB(255, 46, 136));
    cv_text(120 - text_w(&FONT_S, "DRONE SYNTH " MK_VERSION) / 2, 52, &FONT_S, "DRONE SYNTH " MK_VERSION,
            RGB(120, 90, 180));
    cv_blit(0, 80);
    lcd_sync();
}
