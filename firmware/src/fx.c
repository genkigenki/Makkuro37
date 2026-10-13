/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) */
/* The filter (stereo: LADDER, POLIVOKS, BAND, DIST; driven, the band state clipped) and dual effector (one
 * "cartridge" per side: reverb, delay, shimmer, chorus, phaser, crush, ring, freeze; X Y Z each, BLEND). */

/* ------------------------------------------------------------- filter --- */
typedef struct {
    int32_t ic1, ic2;                      /* TPT state-variable filter state */
    int32_t a1, a2, a3;                    /* Q16 */
    int32_t drive;                         /* input gain, Q12 */
    int32_t out;                           /* output gain, Q15: resonance makes it louder */
    uint8_t on;
    lad_t lad;                             /* FILTER LADDER: Melodee's four-pole (analog.c) */
    int32_t cs, rs;                        /* cutoff index, resonance: smoothed (x 256): no zipper from a fader */
    uint8_t warm, side;                    /* side: 1 = R */
    int32_t wet, wt;                       /* the filtered part (Q15), ramped to wt: on / off without a step */
} flt_t;
static flt_t flt[2], flto[2];              /* flto: the type before a change, fading out */

static inline int32_t clip_soft(int32_t x, int32_t knee)   /* linear to knee, then tanh to 2 x knee */
{
    int32_t a = x < 0 ? -x : x, i, y;
    if (a <= knee)
        return x;
    a = ((a - knee > 1 << 22 ? 1 << 22 : a - knee) << 8) / (knee >> 6);   /* in units of knee / 16384 */
    i = a >> 6;
    y = i >= 256 ? 32767 : TANH[256 + i] + (((TANH[257 + i] - TANH[256 + i]) * (a & 63)) >> 6);
    y = knee + (int32_t)(((int64_t)y * knee) >> 15);
    return x < 0 ? -y : y;
}

static void flt_tick(flt_t *f, uint32_t mode, int32_t freq, int32_t res, int32_t cv)
{
    int32_t idx = clamp(freq * 1023 / 127 + cv, 0, 1023), g, k, den;
    res = clamp(res, 0, 127);
    if (!f->warm) {                        /* the cutoff and resonance glide to their targets (~6 ms) */
        f->warm = 1;
        f->cs = idx << 8, f->rs = res << 8;
    }
    f->cs += ((idx << 8) - f->cs) >> 3;
    f->rs += ((res << 8) - f->rs) >> 3;
    idx = (f->cs + 128) >> 8, res = (f->rs + 128) >> 8;
    g = (int32_t)SVF_G[idx];
    k = 131072 - res * 1010;
    f->on = freq < 127 || res > 0 || pt.g[G_FDRIVE] > 0 || cv < 0;
    f->wt = f->on ? 32767 : 0;
    if (mode == FM_BAND)                   /* BAND: faded in over the cutoff's top 16 steps */
        f->wt = clamp((1023 - idx) * 128, 0, 32767);
    if (mode == FM_DIST) {                 /* DIST: drive, bias, a tone low-pass (cutoff) */
        f->on = 1;
        f->wt = 32767;
        f->drive = 4096 + pt.g[G_FDRIVE] * 220 + res * 60;
        f->a1 = clamp(idx * 32, 2000, 32767);
        return;
    }
    if (mode == FM_LADDER) {               /* LADDER: 24 dB, saturating input */
        lad_coef(&f->lad, idx, res);
        f->drive = 4096 + pt.g[G_FDRIVE] * 96;
        return;
    }
    den = 65536 + (int32_t)(((int64_t)g * (g + k)) >> 16);
    f->a1 = (int32_t)(0xFFFFFFFFu / (uint32_t)den);
    f->a2 = (int32_t)(((int64_t)g * f->a1) >> 16);
    f->a3 = (int32_t)(((int64_t)g * f->a2) >> 16);
    f->drive = ((4096 + pt.g[G_FDRIVE] * 96) * (32768 - clamp(res, 0, 127) * 100)) >> 15;   /* x1 .. x4, less with RES */
    f->out = ((k + 65536) >> 2) * 32767 / (3 * 16384);                                      /* 1 .. 0.35 */
}

static void flt_core(flt_t *f, uint32_t mode, int32_t *x, uint32_t n)
{
    uint32_t i;
    int32_t ic1 = f->ic1, ic2 = f->ic2, a1 = f->a1, a2 = f->a2, a3 = f->a3, dr = f->drive;
    if (mode == FM_LADDER) {                   /* (at full rate: at half rate the ringing aliased) */
        for (i = 0; i < n; i++)
            x[i] = lad_run(&f->lad, (x[i] * dr) >> 12, 0);
        f->ic1 = f->ic2 = 0;
        return;
    }
    if (mode == FM_DIST) {
        int32_t lp = f->ic1, b = f->side ? -2600 : 3000;   /* (the sides' bias apart: stereo) */
        for (i = 0; i < n; i++) {
            int32_t y = sat(((x[i] * dr) >> 12) + b) - sat(b);
            lp += ((y - lp) * a1) >> 15;
            x[i] = (lp * 3) >> 2;
        }
        f->ic1 = lp;
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t in = clip_soft((x[i] * dr) >> 12, 20000), v3 = in - ic2, v1, v2;
        v1 = (int32_t)(((int64_t)a1 * ic1 + (int64_t)a2 * v3) >> 16);
        v2 = ic2 + (int32_t)(((int64_t)a2 * ic1 + (int64_t)a3 * v3) >> 16);
        ic1 = clip_soft(2 * v1 - ic1, 16000);              /* the band state clipped: growl, no blow-up */
        ic2 = 2 * v2 - ic2;
        x[i] = ((mode == FM_BAND ? (v1 * 3) >> 1 : v2) * f->out) >> 15;   /* BAND: the band-pass */
    }
    f->ic1 = ic1, f->ic2 = ic2;
}
/* the filter on x: its wet part glides (~12 ms) to on, off or BAND's fade, so nothing steps */
static void flt_run(flt_t *f, uint32_t mode, int32_t *x, uint32_t n)
{
    int32_t dry[CTL], w0 = f->wet, w1, i;
    f->wet += clamp(f->wt - f->wet, -2048, 2048);
    w1 = f->wet;
    if (!w0 && !w1) {
        f->ic1 = f->ic2 = 0;
        return;
    }
    for (i = 0; i < (int32_t)n; i++)
        dry[i] = x[i];
    flt_core(f, mode, x, n);
    if (w0 < 32767 || w1 < 32767)
        for (i = 0; i < (int32_t)n; i++) {
            int32_t w = w0 + (w1 - w0) * i / (int32_t)n;
            x[i] = dry[i] + (int32_t)(((int64_t)(x[i] - dry[i]) * w) >> 15);
        }
}

/* ------------------------------------------------------------- effector --- */
#define FXN 65536u                         /* samples of delay memory per side (1.49 s) */
#define FXM (FXN - 1u)
static int16_t fxmem[2][FXN] __attribute__((section(".pool")));
static const uint16_t COMB[4] = {2491, 2591, 2389, 2273}, AP[2] = {889, 705};   /* reverb lines (x1.6 Freeverb) */
/* reverb memory: the lines in [0, 11500), a pre-delay ring [16384, 32768), the shimmer's ring [32768, 36864) */
enum { RV_PRE = 16384, RV_PREM = 16383, SH_RING = 32768, SH_M = 4095 };

typedef struct {
    uint8_t type, clr;                     /* clr: memory being cleared (after a change), wet muted */
    uint32_t w, clrpos;                    /* write position */
    int32_t lp[4], st[8];                  /* filters, misc. state */
    uint32_t ph, ph2;                      /* LFO / oscillator phases */
    int32_t dly;                           /* delay: time now (samples x 256), slewing */
    int32_t wet;                           /* wet gain now (Q15): pads 15 / 16, ramped */
    uint32_t hold, frz;                    /* crush counter / freeze: samples armed; freeze: the loop end */
    uint16_t ri[16];                       /* reverb line positions (ROOM 6, PLATE 14) */
    int32_t sp[4 * 11];                    /* SPRING: its allpass chain */
    int32_t pv[5];                         /* PLATE (half rate): last and this L / R, the input held */
} fx_t;
static fx_t fx[2];

static inline int32_t rdi(const int16_t *m, uint32_t pos8)   /* linear interpolation, position x 256 */
{
    uint32_t p = pos8 >> 8, f = pos8 & 255u;
    int32_t a = m[p & FXM], b = m[(p + 1u) & FXM];
    return a + (((b - a) * (int32_t)f) >> 8);
}
static inline int16_t s16(int32_t x) { return (int16_t)clamp(x, -32767, 32767); }

/* reverb: 4 combs + 2 allpasses over a pre-delay; returns the wet sample */
static int32_t reverb(fx_t *f, int16_t *m, int32_t in, int32_t fb, int32_t damp, uint32_t pre, uint32_t side)
{
    uint32_t c, off = 0, w = f->w;
    int32_t out = 0, x;
    uint16_t *ri = f->ri;
    m[RV_PRE + (w & RV_PREM)] = s16(in);
    x = m[RV_PRE + ((w - pre) & RV_PREM)] >> 3;
    for (c = 0; c < 4u; c++) {             /* combs live in [0, 9744 + 4 * 23) */
        uint32_t len = COMB[c] + side * 23u, pos = off + ri[c];
        ri[c] = (uint16_t)(ri[c] + 1u >= len ? 0u : ri[c] + 1u);
        int32_t y = m[pos];
        f->lp[c] = y + (((f->lp[c] - y) * damp) >> 15);
        m[pos] = s16(x + ((f->lp[c] * fb) >> 15));
        out += y;
        off += len;
    }
    for (c = 0; c < 2u; c++) {
        uint32_t len = AP[c] + side * 11u, pos = off + ri[4u + c];
        ri[4u + c] = (uint16_t)(ri[4u + c] + 1u >= len ? 0u : ri[4u + c] + 1u);
        int32_t b = m[pos];
        m[pos] = s16(out + (b >> 1));
        out = b - out;
        off += len;
    }
    return out;
}

/* two heads half a window apart, crossfading (triangles): read speed 1 + rate / 256 behind a moving end
 * (pitch shifter: 256 = an octave up), rate / 256 with a fixed end (freeze loop). Positions in base + (mask). */
static int32_t heads(const int16_t *m, uint32_t base, uint32_t mask, uint32_t end, uint32_t len, uint32_t *ph,
                     uint32_t rate)
{
    uint32_t L8 = len << 8, p = *ph = (*ph + rate) % L8, q = (p + (len << 7)) % L8, tri, ga, i;
    int32_t a, b;
    tri = p < (len << 7) ? p : L8 - p;
    ga = tri / (len >> 1);                         /* 0..256 */
    i = ((end - len) << 8) + p;
    a = m[base + ((i >> 8) & mask)];
    i = ((end - len) << 8) + q;
    b = m[base + ((i >> 8) & mask)];
    return (a * (int32_t)ga + b * (int32_t)(256u - ga)) >> 8;
}

/* PLATE: Dattorro's figure-eight tank (J. Dattorro, "Effect Design 1", 1997), lengths for 44.1 kHz: input
 * diffusers, two halves of a modulated allpass, delay, damping, allpass, delay; seven taps per side (the
 * slot L takes the left ones, R the right). X decay, Y tone, Z pre-delay. */
enum { PL_IN = 0, PL_PRE = 50000, PL_PREM = 8191 };
static const uint16_t PL_LEN[12] = {210, 159, 562, 410,           /* input diffusers */
                                    1012, 6598, 2667, 5512,       /* half A: mod allpass, delay, allpass, delay */
                                    1362, 6249, 3936, 4687};      /* half B */
static const int16_t PL_TAP[2][7] = {{394, 4407, 2835, 2958, 2949, 277, 1580},
                                     {523, 5374, 1820, 3961, 3128, 496, 179}};
static const uint16_t PL_OFF[12] = {0, 210, 369, 931, 1341, 2353, 8951, 11618, 17130, 18492, 24741, 28677};
_Static_assert(28677 + 4687 <= PL_PRE, "PLATE: the lines below the pre-delay");
static inline int32_t pl_at(const int16_t *b, uint32_t len, int32_t i, int32_t back)   /* back samples ago */
{
    i -= back;
    return b[i < 0 ? i + (int32_t)len : i];
}
static inline int32_t pl_ap(int16_t *b, uint16_t *ri, uint32_t len, int32_t x, int32_t c)   /* allpass, c Q15 */
{
    int32_t o = b[*ri], w = x - ((o * c) >> 15);
    b[*ri] = s16(w);
    *ri = (uint16_t)(*ri + 1u >= len ? 0u : *ri + 1u);
    return o + ((w * c) >> 15);
}
static inline void pl_put(int16_t *b, uint16_t *ri, uint32_t len, int32_t x)
{
    b[*ri] = s16(x);
    *ri = (uint16_t)(*ri + 1u >= len ? 0u : *ri + 1u);
}
static int32_t plate(fx_t *f, int16_t *m, int32_t x, int32_t X, int32_t Y, int32_t Z, uint32_t side)
{
    uint16_t *ri = f->ri;
    int32_t decay = 14000 + X * 115, damp = 5000 + Y * 210, a, b, o, k;
    int32_t ex;
    uint32_t pc = f->hold++;                                         /* (this tank's own clock: half rate) */
    m[PL_PRE + (pc & PL_PREM)] = s16(x);
    x = m[PL_PRE + ((pc - 1u - (uint32_t)Z * 30u) & PL_PREM)];      /* pre-delay 0 .. 170 ms */
    f->lp[2] += ((x - f->lp[2]) * (8000 + Y * 190)) >> 15;          /* input bandwidth */
    x = f->lp[2] >> 1;
    x = pl_ap(m + PL_OFF[0], &ri[0], PL_LEN[0], x, 24576);
    x = pl_ap(m + PL_OFF[1], &ri[1], PL_LEN[1], x, 24576);
    x = pl_ap(m + PL_OFF[2], &ri[2], PL_LEN[2], x, 20480);
    x = pl_ap(m + PL_OFF[3], &ri[3], PL_LEN[3], x, 20480);
    f->ph += 154000u;                                                /* ~0.8 Hz, the tank's wobble */
    ex = SINE[f->ph >> 22] >> 11;                                    /* +-16 samples */
    for (k = 0; k < 2; k++) {                                        /* the two halves, each fed by the other */
        uint32_t h = (uint32_t)k * 4u + 4u;
        int16_t *mb = m + PL_OFF[h];
        int32_t in = x + ((f->st[k ^ 1] * decay) >> 15), e = k ? -ex : ex, rd, oo;
        rd = pl_at(mb, PL_LEN[h], ri[h + 0u], PL_LEN[h] - 20 + e);   /* the modulated allpass (coef -0.7) */
        oo = in + ((rd * 22938) >> 15);
        pl_put(mb, &ri[h], PL_LEN[h], oo);
        a = rd - ((oo * 22938) >> 15);
        pl_put(m + PL_OFF[h + 1u], &ri[h + 1u], PL_LEN[h + 1u], a);
        b = pl_at(m + PL_OFF[h + 1u], PL_LEN[h + 1u], ri[h + 1u], (int32_t)PL_LEN[h + 1u] - 1);
        f->lp[k] += ((b - f->lp[k]) * damp) >> 15;                  /* damping */
        b = pl_ap(m + PL_OFF[h + 2u], &ri[h + 2u], PL_LEN[h + 2u], (f->lp[k] * decay) >> 15, 16384);
        pl_put(m + PL_OFF[h + 3u], &ri[h + 3u], PL_LEN[h + 3u], b);
        f->st[k] = pl_at(m + PL_OFF[h + 3u], PL_LEN[h + 3u], ri[h + 3u], (int32_t)PL_LEN[h + 3u] - 1);
    }
    for (side = 0; side < 2u; side++) {   /* seven taps a side: + + - + (its half) - - - (the other) */
        const int16_t *t = PL_TAP[side];
        uint32_t A = side ? 4u : 8u, B = side ? 8u : 4u;   /* (left: mostly the second half, as Dattorro's) */
        o = pl_at(m + PL_OFF[A + 1u], PL_LEN[A + 1u], ri[A + 1u], t[0]) +
            pl_at(m + PL_OFF[A + 1u], PL_LEN[A + 1u], ri[A + 1u], t[1]) -
            pl_at(m + PL_OFF[A + 2u], PL_LEN[A + 2u], ri[A + 2u], t[2]) +
            pl_at(m + PL_OFF[A + 3u], PL_LEN[A + 3u], ri[A + 3u], t[3]) -
            pl_at(m + PL_OFF[B + 1u], PL_LEN[B + 1u], ri[B + 1u], t[4]) -
            pl_at(m + PL_OFF[B + 2u], PL_LEN[B + 2u], ri[B + 2u], t[5]) -
            pl_at(m + PL_OFF[B + 3u], PL_LEN[B + 3u], ri[B + 3u], t[6]);
        f->pv[2u + side] = (o * 19661) >> 15;
    }
    return 0;
}

/* SPRING (after Melodee37's spring, Kerem Kilic): the loop through ten stretched allpasses (the chirp, the
 * drips), damping and decay; the far end and a second pickup. X size / decay, Y damping, Z wobble. */
#define SP_N 10u
static int32_t spring(fx_t *f, int16_t *ln, int32_t xin, int32_t X, int32_t Y, int32_t Z)
{
    uint32_t wp = f->w, j = (wp & 3u) * (SP_N + 1u), k;
    int32_t g = 19661 + X * 85, kl = 26000 - Y * 160, len = 1323 + ((X * 1323) >> 7), L2, L3, fr, w, p, o, v;
    int32_t x = (xin * 2580) >> 15, r = ln[(wp - (uint32_t)len) & 4095u], t0, t1;
    int32_t *chain = f->sp + j;
    f->ph2 += 3000u + (uint32_t)Z * 200u;
    w = (len << 7) + ((SINE[f->ph2 >> 22] * (3 + Z / 16)) >> 8);   /* the far end, Q8 */
    L2 = w >> 8;
    fr = w & 255;
    L3 = (len * 3) >> 2;
    t0 = ln[(wp - (uint32_t)L2) & 4095u];
    t1 = ln[(wp - (uint32_t)L2 - 1u) & 4095u];
    f->lp[0] += ((r - f->lp[0]) * kl) >> 15;
    o = f->lp[0] * g;
    x += (o + ((o >> 31) & 32767)) >> 15;
    o = x - f->lp[1] + f->lp[2];                   /* low cut, the remainder kept */
    f->lp[2] = o & 63;
    f->lp[1] += o >> 6;
    x -= f->lp[1];
    p = chain[0];
    chain[0] = x;
    for (k = 1; k <= SP_N; k++) {
        v = (x - chain[k]) * 2867;
        o = chain[k];
        x = ((v + ((v >> 31) & 4095)) >> 12) + p;
        p = o;
        chain[k] = x;
    }
    ln[wp & 4095u] = s16(x);
    return (t0 + (((t1 - t0) * fr) >> 8)) * 4 + ln[(wp - (uint32_t)L3) & 4095u] * 2;
}

/* RESO: four tuned combs (root, fifth, octave, two octaves of the tuning's root + Z semitones), damped:
 * the drones ring in them. X decay, Y brightness, Z tuning. */
static int32_t reso(fx_t *f, int16_t *m, int32_t x, int32_t X, int32_t Y, int32_t Z, uint32_t side)
{
    static const int16_t IV[4] = {0, 700, 1200, 2400};
    int32_t fb = 24000 + X * 66, damp = 6000 + Y * 210, out = 0, c;
    uint32_t k;
    if ((f->w & 255u) == 0u || !f->pv[0])           /* the lengths (now and then: the root may change) */
        for (k = 0; k < 4u; k++) {
            int32_t cents = 3600 + pt.g[G_ROOT] * 100 + IV[k] + (Z - 64) * 100 + (int32_t)side * 7;
            uint32_t inc = cents_inc(cents);
            f->pv[k] = (int32_t)(0xFFFFFFFFu / (inc ? inc : 1u));   /* samples a period */
            if (f->pv[k] > 12000)
                f->pv[k] = 12000;
            if (f->pv[k] < 20)
                f->pv[k] = 20;
        }
    for (k = 0; k < 4u; k++) {
        uint32_t base = k * 12288u, len = (uint32_t)f->pv[k], pos;
        if (++f->ri[k] >= len)
            f->ri[k] = 0;
        pos = base + f->ri[k];
        c = m[pos];
        f->lp[k] += ((c - f->lp[k]) * damp) >> 15;
        m[pos] = s16((x >> 2) + ((f->lp[k] * fb) >> 15));
        out += c;
    }
    return out >> 1;
}

static int32_t fx_sample(fx_t *f, int16_t *m, int32_t x, uint32_t side)
{
    const int16_t *g = &pt.g[side ? G_FXRX : G_FXLX];
    int32_t X = g[0], Y = g[1], Z = g[2], y = 0;
    uint32_t w = ++f->w;
    switch (f->type) {
    case FX_PLATE:                         /* at half rate (the tail needs no more): both sides' taps */
        if (w & 1u) {
            f->pv[0] = f->pv[2], f->pv[1] = f->pv[3];
            plate(f, m, (x + f->pv[4]) >> 1, X, Y, Z, side);
            y = f->pv[2u + side];
        } else {
            f->pv[4] = x;
            y = (f->pv[side] + f->pv[2u + side]) >> 1;
        }
        break;
    case FX_SPRING:
        y = spring(f, m, x, X, Y, Z);
        break;
    case FX_RESO:
        y = reso(f, m, x, X, Y, Z, side);
        break;
    case FX_REVERB:                        /* X decay, Y tone, Z pre-delay */
        y = reverb(f, m, x, 22000 + X * 83, (127 - Y) * 200, 1u + (uint32_t)Z * 80u, side);
        break;
    case FX_SHIMMER: {                     /* X decay, Y shimmer, Z tone: an octave up in the tail */
        int32_t up = heads(m, SH_RING, SH_M, w, 3000, &f->ph, 256);
        f->st[1] += (up - f->st[1]) >> 7;          /* (no DC builds up in the loop) */
        up -= f->st[1];
        m[SH_RING + (w & SH_M)] = s16(f->st[0]);
        y = reverb(f, m, x + ((up * Y) >> 8), 22000 + X * 70, (127 - Z) * 200, 600, side);
        f->st[0] = y;
        break;
    }
    case FX_DELAY: {                       /* X time, Y feedback, Z tone */
        int32_t tgt = (int32_t)(1300u + (uint32_t)(X * X) * 4u) << 8;   /* 30 ms .. 1.5 s */
        f->dly = f->dly ? f->dly + ((tgt - f->dly) >> 11) : tgt;   /* (slews: tape-like) */
        y = rdi(m, ((w & FXM) << 8) - (uint32_t)f->dly);
        f->lp[0] = y + (((f->lp[0] - y) * (127 - Z) * 220) >> 15);
        m[w & FXM] = s16(sat(x + ((f->lp[0] * Y * 250) >> 15)));   /* (the feedback saturates: it sings) */
        break;
    }
    case FX_CHORUS: {                      /* X rate, Y depth, Z feedback */
        int32_t d;
        f->ph += (uint32_t)(X * X + 40) * 13u;
        d = (661 << 8) + ((SINE[(f->ph + side * 0x80000000u) >> 22] * (Y * 14 + 20)) >> 9);   /* 15 ms +- 10 ms */
        y = rdi(m, ((w & FXM) << 8) - (uint32_t)d);
        m[w & FXM] = s16(x + ((y * Z * 180) >> 15));
        break;
    }
    case FX_PHASER: {                      /* X rate, Y depth, Z feedback: 4 allpass stages */
        int32_t a, s = x + ((f->st[5] * Z * 200) >> 15);
        uint32_t k;
        f->ph += (uint32_t)(X * X + 30) * 9u;
        a = 9000 + (((SINE[f->ph >> 22] + 32768) * (Y * 150)) >> 16);   /* coefficient, Q15 */
        for (k = 0; k < 4u; k++) {
            int32_t o = f->st[k] - ((a * s) >> 15);
            f->st[k] = s + ((a * o) >> 15);
            s = o;
        }
        f->st[5] = s;
        y = s;
        break;
    }
    case FX_CRUSH: {                       /* X rate down, Y bits, Z drive */
        if (++f->hold >= 1u + (uint32_t)X / 4u) {
            int32_t sh = 1 + Y / 9, d = clip_soft((x * (4096 + Z * 64)) >> 12, 24000);
            f->hold = 0;
            f->st[0] = ((d + (1 << (sh - 1))) >> sh) << sh;
        }
        y = f->st[0];
        break;
    }
    case FX_RING: {                        /* X frequency, Y wobble (LFO B), Z ring .. AM */
        int32_t o, inc = (int32_t)cents_inc(2500 + X * 60 + ((modv[MS_LFOB] * Y) >> 13));
        f->ph += (uint32_t)inc;
        o = SINE[f->ph >> 22];
        o += ((32767 - o) * Z * 129) >> 15;        /* Z: towards a unipolar (AM) carrier */
        y = (x * o) >> 15;
        break;
    }
    case FX_FREEZE: {                      /* X loop length, Y speed, Z input fed in */
        uint32_t len = 2205u + (uint32_t)X * 400u;
        if (!f->frz) {                     /* armed: record a loop's length, then freeze it */
            m[w & FXM] = s16(x);
            if (++f->hold >= len)
                f->frz = w + 1u;
            y = x;
            break;
        }
        if (Z) {                           /* the input fed into the loop */
            uint32_t at = (f->frz - len + (f->ph >> 8)) & FXM;
            m[at] = s16(m[at] + (((x - m[at]) * Z) >> 9));
        }
        y = heads(m, 0, FXM, f->frz, len, &f->ph, 128u + (uint32_t)Y * 3u);
        break;
    }
    default:
        break;
    }
    return y;
}

/* both sides: bus in, bus out (dry + wet by BLEND) */
static void fx_run(int32_t *l, int32_t *r, uint32_t n)
{
    static int32_t in[2][CTL];
    uint32_t s, i;
    int32_t blend;
    for (i = 0; i < n; i++) {              /* each side's input hears a third of the other: a wider space */
        in[0][i] = clamp(l[i] + r[i] / 3, -32767, 32767);
        in[1][i] = clamp(r[i] + l[i] / 3, -32767, 32767);
    }
    /* two PLATEs: one tank, fed by both sides, its left taps to L, its right taps to R (Dattorro's stereo) */
    int shared = fx[0].type == FX_PLATE && fx[1].type == FX_PLATE && !fx[0].clr && !fx[1].clr &&
                 pt.g[G_FXL] == FX_PLATE && pt.g[G_FXR] == FX_PLATE;
    static int32_t pr[CTL];
    if (shared)
        for (i = 0; i < n; i++)
            in[0][i] = (in[0][i] + in[1][i]) >> 1;
    for (s = 0; s < 2u; s++) {
        fx_t *f = &fx[s];
        int16_t *m = fxmem[s];
        int32_t *x = s ? r : l, tgt, dw;
        uint32_t t = (uint32_t)pt.g[G_FXL + s];
        blend = pt.g[s ? G_BLENDR : G_BLEND] * 258;
        if (t != f->type) {                /* a new cartridge: clear its memory first (wet muted) */
            f->type = (uint8_t)t;
            f->clr = 1;
            f->clrpos = 0;
            f->frz = 0;
            f->dly = 0;
            memset(f->lp, 0, sizeof f->lp);
            memset(f->st, 0, sizeof f->st);
            memset(f->ri, 0, sizeof f->ri);
            memset(f->sp, 0, sizeof f->sp);
            memset(f->pv, 0, sizeof f->pv);
            f->hold = 0;
        }
        if (f->clr) {
            for (i = 0; i < 2048u && f->clrpos < FXN; i++)
                m[f->clrpos++] = 0;
            if (f->clrpos >= FXN)
                f->clr = 0;
        }
        tgt = f->type && !f->clr ? blend : 0;
        if (!f->type || (f->clr && !f->wet))
            continue;
        dw = (tgt - f->wet) / (int32_t)n;
        for (i = 0; i < n; i++) {
            int32_t y;
            if (shared && s) {
                y = pr[i];
            } else {
                y = fx_sample(f, m, in[s][i], s);
                pr[i] = (f->w & 1u) ? f->pv[3] : (f->pv[1] + f->pv[3]) >> 1;
            }
            f->wet += dw;
            x[i] = ((x[i] * (32767 - f->wet)) >> 15) + ((y * f->wet) >> 15);
        }
        f->wet = tgt;
    }
}
