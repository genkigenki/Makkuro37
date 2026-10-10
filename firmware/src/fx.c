/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Dual filter (L / R, 12 dB, Polivoks-like: driven, the band state clipped) and dual effector (one
 * "cartridge" per side: reverb, delay, shimmer, chorus, phaser, crush, ring, freeze; X Y Z each, BLEND). */

/* ------------------------------------------------------------- filter --- */
typedef struct {
    int32_t ic1, ic2;                      /* TPT state-variable filter state */
    int32_t a1, a2, a3;                    /* Q16 */
    int32_t drive;                         /* input gain, Q12 */
    int32_t out;                           /* output gain, Q15: resonance makes it louder */
    uint8_t on;
} flt_t;
static flt_t flt[2];

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

static void flt_tick(flt_t *f, int32_t freq, int32_t res, int32_t cv)
{
    int32_t idx = clamp(freq * 1023 / 127 + cv, 0, 1023), g = (int32_t)SVF_G[idx];
    int32_t k = 131072 - clamp(res, 0, 127) * 1010, den;
    f->on = freq < 127 || res > 0 || pt.g[G_FDRIVE] > 0 || cv < 0;
    den = 65536 + (int32_t)(((int64_t)g * (g + k)) >> 16);
    f->a1 = (int32_t)(0xFFFFFFFFu / (uint32_t)den);
    f->a2 = (int32_t)(((int64_t)g * f->a1) >> 16);
    f->a3 = (int32_t)(((int64_t)g * f->a2) >> 16);
    f->drive = ((4096 + pt.g[G_FDRIVE] * 96) * (32768 - clamp(res, 0, 127) * 100)) >> 15;   /* x1 .. x4, less with RES */
    f->out = ((k + 65536) >> 2) * 32767 / (3 * 16384);                                      /* 1 .. 0.35 */
}

static void flt_run(flt_t *f, int32_t *x, uint32_t n)
{
    uint32_t i;
    int32_t ic1 = f->ic1, ic2 = f->ic2, a1 = f->a1, a2 = f->a2, a3 = f->a3, dr = f->drive;
    if (!f->on) {
        f->ic1 = f->ic2 = 0;
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t in = clip_soft((x[i] * dr) >> 12, 20000), v3 = in - ic2, v1, v2;
        v1 = (int32_t)(((int64_t)a1 * ic1 + (int64_t)a2 * v3) >> 16);
        v2 = ic2 + (int32_t)(((int64_t)a2 * ic1 + (int64_t)a3 * v3) >> 16);
        ic1 = clip_soft(2 * v1 - ic1, 16000);              /* the band state clipped: growl, no blow-up */
        ic2 = 2 * v2 - ic2;
        x[i] = (v2 * f->out) >> 15;
    }
    f->ic1 = ic1, f->ic2 = ic2;
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
    uint16_t ri[6];                        /* reverb line positions */
} fx_t;
static fx_t fx[2];
static volatile uint8_t fx_mute;           /* bit per side: off (pads 15 / 16) */

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

static int32_t fx_sample(fx_t *f, int16_t *m, int32_t x, uint32_t side)
{
    const int16_t *g = &pt.g[side ? G_FXRX : G_FXLX];
    int32_t X = g[0], Y = g[1], Z = g[2], y = 0;
    uint32_t w = ++f->w;
    switch (f->type) {
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
        m[w & FXM] = s16(x + ((f->lp[0] * Y * 250) >> 15));
        break;
    }
    case FX_CHORUS: {                      /* X rate, Y depth, Z feedback */
        int32_t d;
        f->ph += (uint32_t)(X * X + 40) * 13u;
        d = (661 << 8) + ((SINE[f->ph >> 22] * (Y * 14 + 20)) >> 9);   /* 15 ms +- up to 10 ms */
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
    uint32_t s, i;
    int32_t blend = pt.g[G_BLEND] * 258;
    for (s = 0; s < 2u; s++) {
        fx_t *f = &fx[s];
        int16_t *m = fxmem[s];
        int32_t *x = s ? r : l, tgt, dw;
        uint32_t t = (uint32_t)pt.g[G_FXL + s];
        if (t != f->type) {                /* a new cartridge: clear its memory first (wet muted) */
            f->type = (uint8_t)t;
            f->clr = 1;
            f->clrpos = 0;
            f->frz = 0;
            f->dly = 0;
            memset(f->lp, 0, sizeof f->lp);
            memset(f->st, 0, sizeof f->st);
            memset(f->ri, 0, sizeof f->ri);
            f->hold = 0;
        }
        if (f->clr) {
            for (i = 0; i < 2048u && f->clrpos < FXN; i++)
                m[f->clrpos++] = 0;
            if (f->clrpos >= FXN)
                f->clr = 0;
        }
        tgt = f->type && !f->clr && !((fx_mute >> s) & 1u) ? blend : 0;
        if (((fx_mute >> s) & 1u) && f->type == FX_FREEZE)
            f->frz = f->hold = 0;          /* the next freeze takes a new loop */
        if (!f->type || (f->clr && !f->wet))
            continue;
        dw = (tgt - f->wet) / (int32_t)n;
        for (i = 0; i < n; i++) {
            int32_t y = fx_sample(f, m, clamp(x[i], -32767, 32767), s);
            f->wet += dw;
            x[i] = ((x[i] * (32767 - f->wet)) >> 15) + ((y * f->wet) >> 15);
        }
        f->wet = tgt;
    }
}
