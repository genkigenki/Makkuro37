/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* The screen. Top: eight fuzzy soot creatures, one per voice: a hammer bonks one when its gate opens, it
 * squishes, glows in its colour and sings (bobs with its pitch and level), and sleeps when silent.
 * Bottom: the page as a little animated picture, its four elements in the four knob colours; a turned knob
 * shows its value big for a moment. Few words, big letters. */
#define C_BG RGB(4, 0, 10)
static const uint8_t KCOL[4][3] = {{40, 124, 255}, {30, 204, 112}, {255, 198, 24}, {255, 98, 26}};   /* K5..K8 */
static const uint8_t DIMC[3] = {70, 64, 96};

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

/* ------------------------------------------------------------- creatures --- */
#define CW 60
#define CH 62
static struct {
    uint8_t hits;                          /* the voice's hits seen */
    uint32_t hit_t;                        /* the last bonk (ms) */
    uint32_t ph;                           /* singing phase */
    uint32_t blink_t;                      /* next blink (ms) */
    uint32_t drawn;                        /* hash of what was drawn last (skip when unchanged) */
} cre[NV];
static uint32_t fuzz_seed;

static inline uint32_t hash3(int32_t x, int32_t y, uint32_t s)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* one creature into the canvas: centre (cx, cy), body radius r; lv 0..32767 level; hit 0..256 (bonk, fading);
 * sleep: eyes shut; sx / sy: squish (x256); col: its colour */
static void creature(int32_t cx, int32_t cy, int32_t r, int32_t lv, int32_t hit, int sleep, int blink,
                     const uint8_t *col, uint32_t seed)
{
    int32_t sx = 256 + hit / 5, sy = 256 - hit / 4;                   /* squashed by the hammer */
    int32_t rx = r * sx >> 8, ry = r * sy >> 8, x, y, glow = lv >> 7;  /* glow 0..255 */
    int32_t by = cy + (r - ry);                                       /* (squashed onto its feet) */
    int32_t f2 = (rx + 6) * (rx + 6) * 256 / (rx * rx), rxi = rx * rx, ryi = ry * ry;   /* the fur's edge, Q8 */
    uint16_t hair = glow > 30 ? rgbk(col, 70 + glow / 2) : RGB(92, 82, 116), hair2 = RGB(40, 34, 54);
    for (y = -ry - 10; y <= ry + 10; y++)
        for (x = -rx - 10; x <= rx + 10; x++) {
            int32_t d2 = x * x * 256 / rxi + y * y * 256 / ryi;      /* (radius / body radius)^2, Q8 */
            if (d2 <= 256) {                                          /* the body, a soft sheen on top */
                cv_pset(cx + x, by + y, RGB(18, 14, 26));
            } else if (d2 <= f2) {                                    /* fur: hairs, thinner outwards */
                uint32_t h = hash3(x, y, seed), t = (uint32_t)((d2 - 256) * 256 / (f2 - 256));
                if ((h & 255u) < 250u - t * 230u / 256u)
                    cv_pset(cx + x, by + y, (h & 0x300u) ? hair2 : hair);
            } else if (glow > 8) {                                    /* the glow around it */
                int32_t d = (d2 - f2) / 8;
                if (d < 30 && ((hash3(x, y, seed >> 3) & 3u) || d < 8))
                    cv_pset(cx + x, by + y, rgbk(col, glow * (30 - d) / 34));
            }
        }
    cv_ellipse(cx - r / 3, by - ry / 2 - 1, r / 5, r / 8, RGB(40, 34, 54));   /* a sheen */
    /* feet */
    cv_ellipse(cx - r / 2, cy + r + 4, 4, 2, RGB(120, 108, 150));
    cv_ellipse(cx + r / 2, cy + r + 4, 4, 2, RGB(120, 108, 150));
    {   /* eyes: big and white; wide when bonked; shut (a line) when asleep */
        int32_t ex = r * 2 / 5 * sx >> 8, ey = by - ry / 4, er = r / 3 + hit / 80;
        if (sleep || blink) {
            cv_rect(cx - ex - er, ey, 2 * er, 2, RGB(170, 170, 190));
            cv_rect(cx + ex - er, ey, 2 * er, 2, RGB(170, 170, 190));
        } else {
            int32_t look = hit ? 0 : isin(seed * 40503u, 2);
            cv_ellipse(cx - ex, ey, er, er + 1, RGB(250, 250, 255));
            cv_ellipse(cx + ex, ey, er, er + 1, RGB(250, 250, 255));
            cv_ellipse(cx - ex + look, ey + 1, er / 2, er / 2, RGB(0, 0, 0));
            cv_ellipse(cx + ex + look, ey + 1, er / 2, er / 2, RGB(0, 0, 0));
        }
        if (!sleep && lv > 1500) {                                    /* singing: a mouth in its colour */
            int32_t m = 1 + (lv >> 13);
            cv_ellipse(cx, ey + er + 4 + m, 3, m, rgbk(col, 200));
        }
    }
}

/* the hammer: t 0..1 (x256) through its swing; contact near t = 0.35 */
static void hammer(int32_t cx, int32_t top, int32_t t)
{
    int32_t lift = t < 90 ? 90 - t : (t - 90) / 2, hx = cx + 4, hy = top - 6 - lift / 6;
    uint16_t wood = RGB(170, 110, 60), head = RGB(210, 210, 225);
    if (t >= 256)
        return;
    cv_thick(hx + 4, hy, hx + 22, hy - 14 - lift / 10, wood);         /* handle */
    cv_rect(hx - 8, hy - 5, 16, 9, head);                             /* head */
    cv_rect(hx - 8, hy - 5, 16, 2, RGB(255, 255, 255));
    if (t > 80 && t < 150) {                                          /* bonk! */
        cv_line(hx - 14, hy + 2, hx - 20, hy - 2, RGB(255, 240, 120));
        cv_line(hx + 12, hy + 2, hx + 18, hy - 2, RGB(255, 240, 120));
    }
}

static int creature_cell(uint32_t v, int force)
{
    int32_t lv = vs[v].level, hit = 0, t, bob;
    uint32_t now = fm1_ms, h;
    int sleep = !vs[v].stage && lv < 200, blink;
    if (vs[v].hits != cre[v].hits) {
        cre[v].hits = vs[v].hits;
        cre[v].hit_t = now;
    }
    t = (int32_t)(now - cre[v].hit_t) * 256 / 300;                    /* the swing: 300 ms */
    if (t < 256 && t > 80)
        hit = t < 140 ? 256 : 256 - (t - 140) * 2;
    if ((int32_t)(now - cre[v].blink_t) > 0)
        cre[v].blink_t = now + 2000u + hash3((int32_t)v, (int32_t)now, 7) % 3000u;
    blink = cre[v].blink_t - now < 130u && !sleep;
    cre[v].ph += (vs[v].inc[0] >> 9) + 3000000u;                      /* sings with its pitch */
    bob = sleep ? isin(now * 1200000u, 1) : isin(cre[v].ph, 1 + (lv >> 14));
    h = (uint32_t)(lv >> 9) * 31u + (uint32_t)hit * 7u + (uint32_t)bob * 131u + (uint32_t)blink * 3u +
        (sleep ? 0u : (now >> 7)) + (v == ui.voice) * 997u + ui.latched[v] * 4441u;
    if (!force && h == cre[v].drawn)
        return 0;
    cre[v].drawn = h;
    cv_begin(CW, CH, C_BG);
    creature(CW / 2, 31 + bob, 17, lv, hit, sleep, blink, VCOL[v], sleep ? fuzz_seed >> 4 : fuzz_seed + v);
    if (t < 256)
        hammer(CW / 2, 31 - 17, t);
    if (sleep && ((now >> 10) + v) % 4u == 0u)
        cv_text(CW / 2 + 14, 10 - (int32_t)((now >> 6) & 7u), &FONT_S, "z", RGB(110, 100, 150));
    if (ui.latched[v])                                                /* HOLD: a dot of its colour */
        cv_ellipse(6, CH - 6, 3, 3, rgbk(VCOL[v], 230));
    if (v == ui.voice) {                                              /* selected: a white marker */
        cv_rect(CW / 2 - 9, CH - 3, 18, 3, RGB(240, 240, 250));
    }
    cv_blit(CW * (v & 3u), (uint32_t)CH * (v >> 2u));
    return 1;
}

static uint32_t sun_ph_store[NV][5];
static uint32_t sun_ph(uint32_t v, uint32_t g) { return sun_ph_store[v][g]; }
static void sun_tick(uint32_t dt)
{
    uint32_t v, g;
    for (v = 0; v < NV; v++)
        for (g = 0; g < 5u; g++)
            sun_ph_store[v][g] += (vs[v].inc[g] >> 3) * dt;
}

/* ------------------------------------------------------------- the pages --- */
/* the knob colour an item has now (in the four on K5..K8), else dim */
static uint16_t icol(uint8_t kind, uint8_t id, uint8_t bit, int32_t k)
{
    uint32_t i;
    for (i = ui.sub * 4u; i < ui.sub * 4u + 4u && i < ui.nit; i++)
        if (ui.it[i].kind == kind && ui.it[i].id == id && ui.it[i].bit == bit)
            return rgbk(KCOL[i & 3u], k);
    return rgbk(DIMC, k);
}
#define VC(id) icol(IK_V, (uint8_t)(id), 0, 256)
#define GC(id) icol(IK_G, (uint8_t)(id), 0, 256)

static void bar_v(int32_t x, int32_t y, int32_t w, int32_t h, int32_t v, int32_t max, uint16_t c)
{
    int32_t f = clamp(v, 0, max) * h / (max ? max : 1);
    cv_rect(x, y, w, h, RGB(18, 14, 28));
    cv_rect(x, y + h - f, w, f, c);
}

static void scene_classic(uint32_t v, uint32_t now)
{
    const int16_t *p = pt.v[v];
    uint32_t g;
    for (g = 0; g < 5u; g++) {                       /* five rails: each generator's range, its pitch a dot */
        int32_t x = 24 + 48 * (int32_t)g, span = (GEN_HI[g] - GEN_LO[g]) * 100, y = 66 - p[CL_TUNE1 + g] * 56 / span;
        uint16_t c = ui.page == PG_MUTEMOD ? icol(IK_VBIT, CL_MUTE, (uint8_t)g, 256) : VC(CL_TUNE1 + g);
        int muted = (p[CL_MUTE] >> g) & 1, mod = (p[CL_MOD] >> g) & 1;
        cv_rect(x, 8, 2, 60, RGB(36, 30, 50));
        if (mod) {                                   /* MOD: the dot wobbles */
            uint16_t mc = ui.page == PG_MUTEMOD ? icol(IK_VBIT, CL_MOD, (uint8_t)g, 256) : rgbk(DIMC, 256);
            y += isin(now * 5000000u + g * 0x30000000u, 4);
            cv_circle(x + 1, y, 11, mc);
        }
        if (muted)
            cv_circle(x + 1, y, 7, c);
        else {
            int32_t pulse = isin(sun_ph(v, g), 1);
            cv_ellipse(x + 1, y, 7 + pulse, 7 + pulse, c);
        }
    }
    if (ui.page == PG_VOICE) {                       /* VOLT: the floor sinks; past half, sparks (FM) */
        int32_t vt = p[CL_VOLT];
        cv_rect(4, 74, vt * 232 / 127, 4, VC(CL_VOLT));
        if (vt > 64 && ((now >> 6) & 1u))
            cv_rect(4 + vt * 232 / 127, 70, 4, 4, RGB(255, 255, 200));
    }
}

static void scene_noise(uint32_t v, uint32_t now)
{
    const int16_t *p = pt.v[v];
    int32_t x, per = 44 - p[NZ_PITCH] * 36 / 127, dv = clamp(p[NZ_DIV], 1, 8), ly = 0, lt = 0;
    uint32_t sc = now * (uint32_t)(p[NZ_RATE] + 8) * (p[NZ_MULT] ? 40u : 4u) * 4000u;
    int fm = p[NZ_MODE] == NM_FM || p[NZ_MODE] == NM_FMAM, am = p[NZ_MODE] >= NM_AM;
    uint32_t tph = now << 22;
    for (x = 0; x < 240; x++) {
        uint32_t ph = (uint32_t)x * 0x1800000u + sc;
        int32_t st = (int32_t)((ph >> 28) % (uint32_t)(2 * dv)), y = 76 - st * 18 / (2 * dv - 1 > 0 ? 2 * dv - 1 : 1);
        int32_t pp = fm ? per - st * per * p[NZ_FMAMT] / (127 * 2 * dv) : per, t;
        if (x)                                       /* the modulator: a staircase */
            cv_thick(x - 1, ly, x, y, VC(NZ_RATE));
        ly = y;
        tph += 0xFFFFFFFFu / (uint32_t)(pp < 3 ? 3 : pp);
        t = tph < 0x80000000u ? 8 : 40;              /* the tone: a square, bent by FM, gated by AM */
        if (p[NZ_PITCH] && (!am || ((ph >> 28) & 1u) == 0u)) {
            if (x && t != lt)
                cv_rect(x, 8, 2, 34, VC(NZ_PITCH));
            cv_rect(x, t, 1, 3, VC(NZ_PITCH));
        }
        lt = t;
        if (p[NZ_NOISE] && (int32_t)(hash3(x, (int32_t)(now >> 5), 1) & 127u) < p[NZ_NOISE])
            cv_rect(x, 6 + (int32_t)(hash3(x, (int32_t)now, 2) % 40u), 2, 2, VC(NZ_NOISE));
    }
}

static int32_t wave_y(uint32_t w, uint32_t ph, int32_t shape)       /* a VCO wave for the picture, Q15 */
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

static void scene_vco(uint32_t v, uint32_t now)
{
    const int16_t *p = pt.v[v];
    int32_t x, ly = 0, oc = p[VC_OCT];
    uint32_t w = (uint32_t)p[VC_WAVE];
    for (x = 0; x < 180; x++) {                      /* two cycles of the wave, drifting */
        int32_t y = 42 - (wave_y(w, (uint32_t)x * 0x2D82D82u + now * 3000000u, p[VC_SHAPE]) * 30 >> 15);
        if (x)
            cv_thick(x - 1, ly, x, y, VC(VC_WAVE));
        ly = y;
    }
    cv_rect(0, 76, 180 * p[VC_SHAPE] / 127, 3, VC(VC_SHAPE));
    for (x = 0; x < 8; x++)                          /* the octave switch: LO .. 1' */
        cv_rect(196, 70 - x * 9, 16 + x * 2, 6, x == oc ? VC(VC_OCT) : RGB(30, 26, 44));
    cv_rect(230, 40 - p[VC_TUNE] * 34 / 600, 8, 3, VC(VC_TUNE));   /* TUNE: a needle */
    cv_rect(233, 6, 2, 70, RGB(30, 26, 44));
    if (ui.sub) {                                    /* FM / SYNC */
        if (p[VC_FM])
            cv_circle(90, 42, 10 + p[VC_FM] / 6 + isin(now * 9000000u, 3), VC(VC_FM));
        if (v == 4u && p[VC_SYNC])
            cv_rect(20, 6, 4, 72, VC(VC_SYNC));
    }
}

static void scene_env(uint32_t v)
{
    const int16_t *p = pt.v[v];
    int vco = VTYPE[v] == VT_VCO;
    int32_t a = 6 + p[VP_ATT] * 60 / 127, d = vco ? 6 + p[VC_DEC] * 50 / 127 : 0, r = 6 + p[VP_RLS] * 60 / 127;
    int32_t sus = vco ? (p[VC_EG] == EG_HOLD ? 127 : p[VC_SUS]) : 127, sy = 72 - sus * 62 / 127, x = 10;
    int32_t e = vs[v].env >> 18, hold = 60;                           /* e 0..64 */
    cv_thick(x, 72, x + a, 10, VC(VP_ATT));
    x += a;
    if (vco) {
        cv_thick(x, 10, x + d, sy, VC(VC_DEC));
        x += d;
    }
    cv_thick(x, sy, x + hold, sy, vco ? VC(VC_SUS) : RGB(60, 52, 80));
    x += hold;
    cv_thick(x, sy, x + r, 72, VC(VP_RLS));
    cv_ellipse(200, 72 - e * 62 / 64, 6, 6, rgbk(VCOL[v], 256));     /* where the envelope is now */
    cv_rect(197, 8, 6, 64, RGB(22, 18, 32));
    if (vco && p[VC_EG] == EG_LOOP)
        cv_circle(225, 30, 9, VC(VC_EG));
}

static void scene_mix(uint32_t v)
{
    const int16_t *p = pt.v[v];
    uint32_t i;
    bar_v(14, 8, 26, 66, p[VP_VOL], 127, VC(VP_VOL));
    cv_rect(44, 74 - (vs[v].level >> 9), 6, vs[v].level >> 9, rgbk(VCOL[v], 256));
    cv_rect(66, 40, 76, 2, RGB(50, 44, 70));
    cv_ellipse(66 + p[VP_PAN] * 76 / 127, 41, 8, 8, VC(VP_PAN));
    bar_v(160, 8, 26, 66, pt.g[G_MASTER], 127, GC(G_MASTER));
    for (i = 0; i < 4u; i++)                         /* BLEND: wet drops */
        if ((int32_t)i * 32 < pt.g[G_BLEND])
            cv_ellipse(212, 66 - 16 * (int32_t)i, 6, 6, GC(G_BLEND));
}

static void scene_tuning(uint32_t now)
{
    const tset_t *t = &TSETS[pt.g[G_TSET] % TS_N];
    uint32_t k;
    char b[4];
    cv_rect(8, 30, 224, 2, RGB(50, 44, 70));
    for (k = 0; k < t->n; k++) {                     /* the scale's ladder over one octave */
        int32_t x = 8 + t->c[k] * 224 / 1200;
        cv_rect(x, k ? 14 : 6, 3, k ? 30 : 38, k ? GC(G_TSET) : GC(G_ROOT));
    }
    str_cpy(b, NOTE_NAMES[pt.g[G_ROOT] % 12], sizeof b);
    cv_text(10, 46, &FONT_L, b, GC(G_ROOT));
    if (pt.g[G_SNAP]) {                              /* SNAP: a magnet */
        cv_rect(96, 50, 6, 24, GC(G_SNAP));
        cv_rect(118, 50, 6, 24, GC(G_SNAP));
        cv_rect(96, 68, 28, 6, GC(G_SNAP));
    } else
        cv_circle(110, 62, 10, GC(G_SNAP));
    cv_rect(150, 72, 80, 2, RGB(50, 44, 70));
    cv_thick(190, 72, 190 + pt.g[G_FINE] * 36 / 100, 48, GC(G_FINE));
    if (ui.sub) {
        cv_thick(10, 76, 10 + 30 + pt.g[G_GLIDE] / 2, 50, GC(G_GLIDE));
        cv_ellipse(200, 60, 6, 6, GC(G_KEYS));
        if (!pt.g[G_KEYS])
            cv_ellipse(220, 60, 6, 6, GC(G_KEYS));
    }
    (void)now;
}

static void lfo_draw(int32_t y0, uint32_t ph, int32_t wave, uint16_t c1, uint16_t c2, uint32_t speed)
{
    int32_t x, ly = 0;
    for (x = 0; x < 200; x++) {
        int32_t y = y0 + 30 - (lfo_val(ph + (uint32_t)x * speed, wave) * 28 >> 15);
        if (x)
            cv_line(x - 1, ly, x, y, x < 100 ? c1 : c2);
        ly = y;
    }
    cv_ellipse(214, y0 + 30 - (lfo_val(ph, wave) * 28 >> 15), 6, 6, c1);
}

static void scene_lfo(void)
{
    if (!ui.sub) {
        lfo_draw(4, md.pa, pt.g[G_LAWAVE], GC(G_LARATE), GC(G_LAWAVE), 0x1000000u);
        lfo_draw(42, md.pb, pt.g[G_LBWAVE], GC(G_LBRATE), GC(G_LBWAVE), (1u << 25));
    } else {                                         /* the joystick: where it is now */
        int32_t x = 120 + (modv[MS_JOYX] * 50 >> 15), y = 74 - (modv[MS_JOYY] * 66 >> 15);
        cv_rect(70, 41, 100, 2, RGB(50, 44, 70));
        cv_rect(119, 6, 2, 70, RGB(50, 44, 70));
        cv_rect(120 + pt.g[G_JXOFS] * 50 / 64, 38, 3, 8, GC(G_JXOFS));
        cv_rect(116, 74 - (pt.g[G_JYOFS] + 64) * 66 / 128, 8, 3, GC(G_JYOFS));
        cv_ellipse(x, y, 8, 8, RGB(240, 240, 250));
    }
}

static void scene_seq(uint32_t now)
{
    uint32_t s, st = 3u + (uint32_t)pt.g[G_STAGES];
    for (s = 0; s < 5u; s++) {
        int32_t x = 12 + 46 * (int32_t)s, on = (pt.g[G_SGATES] >> s) & 1;
        uint16_t c = s < st ? GC(G_SEQ1 + s) : RGB(26, 22, 36);
        bar_v(x, 6, 30, 58, pt.g[G_SEQ1 + s], 127, c);
        cv_rect(x, 68, 30, 8, on && s < st ? RGB(150, 60, 255) : RGB(30, 26, 44));
        if (pt.g[G_RUN] && md.step == s)
            cv_rect(x - 3, 4, 3, 72, RGB(255, 255, 255));
    }
    if (ui.sub) {
        if (pt.g[G_RUN] && md.pp < 0x40000000u)
            cv_ellipse(232, 10, 5, 5, GC(G_PULSER));
        else
            cv_circle(232, 10, 5, GC(G_PULSER));
    }
    (void)now;
}

static void scene_filter(uint32_t now)
{
    uint32_t s;
    for (s = 0; s < 2u; s++) {                       /* each side's response: flat, a bump, then down */
        int32_t f = s ? pt.g[G_FFR] : pt.g[G_FFL], r = s ? pt.g[G_FRR] : pt.g[G_FRL], x, ly = 0;
        int32_t fx = 10 + f * 220 / 127, y0 = 6 + 36 * (int32_t)s;
        uint16_t cf = GC(s ? G_FFR : G_FFL), cr = GC(s ? G_FRR : G_FRL);
        if (pt.g[G_FLINK] && s)
            f = pt.g[G_FFL], r = pt.g[G_FRL], fx = 10 + f * 220 / 127;
        for (x = 0; x < 236; x++) {
            int32_t d = x - fx, y;
            if (d < -20)
                y = 10;
            else if (d < 0)
                y = 10 - (r * (20 + d) / 20) * 9 / 127;
            else
                y = 10 - r * 9 / 127 + d * 2;
            y = clamp(y, 0, 30) + y0;
            if (x)
                cv_line(x - 1, ly, x, y, d < 0 ? cf : cr);
            ly = y;
        }
    }
    if (ui.sub && pt.g[G_FDRIVE]) {                  /* DRIVE: a zigzag */
        int32_t x;
        for (x = 0; x < 60; x += 6)
            cv_line(170 + x, 40 + (x % 12 ? 6 : -6) * pt.g[G_FDRIVE] / 127, 176 + x,
                    40 - (x % 12 ? 6 : -6) * pt.g[G_FDRIVE] / 127, GC(G_FDRIVE));
    }
    (void)now;
}

static void scene_fx(uint32_t now)
{
    uint32_t s, k;
    for (s = 0; s < 2u; s++) {
        int32_t x = 6 + 118 * (int32_t)s, on = pt.g[G_FXL + s] && !((ui.fx_off >> s) & 1u);
        uint16_t c = GC(G_FXL + s);
        cv_rect(x, 4, 110, 2, on ? c : RGB(40, 34, 56));
        cv_rect(x, 40, 110, 2, on ? c : RGB(40, 34, 56));
        cv_text(x + 4, 8, &FONT_L, N_FX[pt.g[G_FXL + s] % FX_N], on ? c : RGB(70, 64, 96));
        for (k = 0; k < 3u; k++)
            bar_v(x + 10 + 34 * (int32_t)k, 46, 22, 30, pt.g[(s ? G_FXRX : G_FXLX) + k], 127,
                  GC((s ? G_FXRX : G_FXLX) + k));
    }
    if (pt.g[G_BLEND] && ((now >> 8) & 1u))
        cv_ellipse(118, 60, 3, 3, GC(G_BLEND));
}

static void scene_scenes(void)
{
    uint32_t n;
    for (n = 0; n < NSCENE; n++) {
        int32_t x = 6 + 30 * (int32_t)n, used = (scene_used >> n) & 1u, sel = n == ui.slot;
        uint16_t c = sel ? icol(IK_ACT, ACT_SLOT, 0, 256) : RGB(70, 64, 96);
        if (used)
            cv_rect(x, 8, 24, 24, sel ? c : RGB(60, 50, 90));
        else
            cv_circle(x + 12, 20, 10, c);
        if (sel)
            cv_rect(x, 36, 24, 3, c);
    }
    cv_text(10, 46, &FONT_L, "LOAD", icol(IK_ACT, ACT_LOAD, 0, 256));
    cv_text(104, 46, &FONT_L, "SAVE", icol(IK_ACT, ACT_SAVE, 0, 256));
    bar_v(206, 44, 28, 32, ui.morph, 127, icol(IK_ACT, ACT_MORPH, 0, 256));
}

static void scene_settings(void)
{
    char b[4];
    bar_v(10, 8, 30, 66, settings.bright, 8, icol(IK_SET, SET_BRIGHT, 0, 256));
    cv_text(70, 26, &FONT_L, settings.console ? "USB ON" : "USB OFF", icol(IK_SET, SET_CONSOLE, 0, 256));
    b[0] = settings.boot_scene ? (char)('0' + settings.boot_scene) : '-';
    b[1] = 0;
    cv_text(200, 26, &FONT_L, b, icol(IK_SET, SET_BOOT, 0, 256));
}

/* big value: label (L) and value (XL) of what was turned last */
static struct { uint32_t until; uint8_t k; char label[16], val[16]; } big;
static void big_show(uint32_t k, const char *label, const char *val)
{
    big.until = fm1_ms + 1200u;
    big.k = (uint8_t)k;
    str_cpy(big.label, label, sizeof big.label);
    str_cpy(big.val, val, sizeof big.val);
    ui.dirty = 1;
}

static void draw_scene(void)
{
    uint32_t now = fm1_ms, v = ui.voice;
    cv_begin(240, 82, C_BG);
    if ((int32_t)(big.until - now) > 0) {
        const uint8_t *c = big.k < 4u ? KCOL[big.k] : (const uint8_t *)"\xff\x50\xc8";
        const felucca_font_t *f = text_w(&FONT_XL, big.val) <= 236 ? &FONT_XL : &FONT_L;
        cv_text(4, 0, &FONT_L, big.label, rgbk(c, 170));
        text_c(120, 82 - f->h, f, big.val, rgbk(c, 256));
    } else if ((int32_t)(ui.msg_until - now) > 0) {
        text_c(120, 24, &FONT_L, ui.msg, RGB(255, 80, 200));
    } else if (ui.settings)
        scene_settings();
    else
        switch (ui.page) {
        case PG_VOICE:
        case PG_MUTEMOD:
            if (VTYPE[v] == VT_CLASSIC)
                scene_classic(v, now);
            else if (VTYPE[v] == VT_NOISE)
                scene_noise(v, now);
            else
                scene_vco(v, now);
            if (ui.page == PG_MUTEMOD && VTYPE[v] != VT_CLASSIC && pt.v[v][VP_MSRC])
                cv_text(4, 0, &FONT_L, N_MSRC[pt.v[v][VP_MSRC] % MS_N], VC(VP_MSRC));
            break;
        case PG_ENV: scene_env(v); break;
        case PG_MIX: scene_mix(v); break;
        case PG_TUNING: scene_tuning(now); break;
        case PG_LFO: scene_lfo(); break;
        case PG_SEQ: scene_seq(now); break;
        case PG_FILTER: scene_filter(now); break;
        case PG_FX: scene_fx(now); break;
        default: scene_scenes(); break;
        }
    cv_blit(0, 158);
}

/* the page's header: its name (L), its fours as dots, the voice as a coloured badge */
static void draw_header(void)
{
    uint32_t n = page_subs(), k;
    int32_t x;
    cv_begin(240, 34, C_BG);
    cv_rect(0, 0, 240, 1, RGB(60, 20, 70));
    x = cv_text(4, 1, &FONT_L, ui.settings ? "SETUP" : PAGE_NAME[ui.page], RGB(235, 235, 245));
    for (k = 0; n > 1u && k < n; k++)
        cv_ellipse(x + 8 + 12 * (int32_t)k, 17, 4, 4, k == ui.sub ? RGB(235, 235, 245) : RGB(60, 54, 84));
    if (!ui.settings && (ui.page <= PG_MIX)) {
        char d[2] = {(char)('1' + ui.voice), 0};
        cv_ellipse(220, 17, 15, 15, rgbk(VCOL[ui.voice], 256));
        text_c(221, 1, &FONT_L, d, RGB(0, 0, 0));
    }
    cv_blit(0, 124);
}

static void draw_frame(void)
{
    static uint32_t t_cre, t_scene, last;
    static uint8_t hdr_page = 0xFF, hdr_sub, hdr_voice, hdr_set;
    uint32_t now = fm1_ms, v;
    if (now - t_cre >= 40u) {                        /* creatures: only the cells that changed */
        sun_tick(now - last);
        last = now;
        t_cre = now;
        fuzz_seed = now >> 7;
        for (v = 0; v < NV; v++)
            creature_cell(v, 0);
    }
    page_build();
    if (hdr_page != ui.page || hdr_sub != ui.sub || hdr_voice != ui.voice || hdr_set != ui.settings) {
        hdr_page = ui.page, hdr_sub = ui.sub, hdr_voice = ui.voice, hdr_set = ui.settings;
        draw_header();
        ui.dirty = 1;
    }
    if (ui.dirty || now - t_scene >= 66u) {          /* the page's picture moves at ~15 frames / s */
        ui.dirty = 0;
        t_scene = now;
        draw_scene();
    }
}

/* ------------------------------------------------------------- boot --- */
/* the creatures pop up in a row, the hammers bonk them one by one (a rising chord), then the name */
static void boot_anim(void)
{
    static const int16_t CHORD[NV] = {4800, 5200, 5500, 5900, 6000, 6400, 6700, 7100};   /* C E G B C E G B */
    uint32_t t0 = fm1_ms, v, rung = 0;
    lcd_fill(0, 0, 240, 240, C_BG);
    for (;;) {
        uint32_t t = fm1_ms - t0;
        fm1_wdt_feed();
        if (t > 2100u || (fm1_in.keys && t > 300u))
            break;
        cv_begin(240, 80, C_BG);
        for (v = 0; v < NV; v++) {
            int32_t ts = (int32_t)t - (int32_t)v * 70, pop = clamp(ts * 12 / 200, 0, 12), hit = 0;
            int32_t bt = (int32_t)t - 700 - (int32_t)v * 90, x = 15 + 30 * (int32_t)v;
            if (pop <= 0)
                continue;
            if (bt >= 0 && bt < 300) {
                int32_t h = bt * 256 / 300;
                if (h > 80)
                    hit = h < 140 ? 256 : 256 - (h - 140) * 2;
                if (rung <= v && h > 80) {
                    chime_hit(CHORD[v]);
                    rung = v + 1u;
                }
            }
            creature(x, 54 - pop + 12, pop, hit ? 20000 : 0, hit, bt < 0, 0, VCOL[v], fuzz_seed + v + (t >> 7));
            if (bt >= 0 && bt < 300)
                hammer(x, 54 - 2 * pop + 12, bt * 256 / 300);
        }
        cv_blit(0, 30);
        if (t > 1400u) {
            int32_t k = (int32_t)clamp((int32_t)(t - 1400u) * 256 / 300, 0, 256);
            cv_begin(240, 76, C_BG);
            text_c(120, 0, &FONT_XL, "MAKKURO", RGB(255 * k >> 8, 46 * k >> 8, 136 * k >> 8));
            text_c(120, 50, &FONT_L, "37", RGB(120 * k >> 8, 90 * k >> 8, 180 * k >> 8));
            cv_blit(0, 120);
            cv_begin(240, 18, C_BG);
            text_c(120, 0, &FONT_S, "SMK-37 PORT BY GENKIGENKI", RGB(70 * k >> 8, 60 * k >> 8, 100 * k >> 8));
            cv_blit(0, 214);
        }
        fm1_delay_ms(15);
    }
    lcd_fill(0, 0, 240, 240, C_BG);
}

static void splash(void)                             /* before the audio runs: the name only */
{
    lcd_fill(0, 0, 240, 240, C_BG);
    cv_begin(240, 50, C_BG);
    text_c(120, 0, &FONT_XL, "MAKKURO", RGB(255, 46, 136));
    cv_blit(0, 96);
    lcd_sync();
}
