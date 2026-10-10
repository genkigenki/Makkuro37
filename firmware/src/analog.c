/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Melodee), Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca)
 * Makkuro37: Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* The analog parts, after Melodee37's PROPHET engine and master (eng_prophet, fx.c): a tanh knee, the
 * four-pole TPT ladder (Curtis: saturating input; SSI: every stage saturates), the DC blocker with error
 * feedback, the peak limiter in front of the soft clipper. */

/* 32767 tanh(x / 32768), |x| < 65536 (flat beyond), interpolated */
static inline int32_t sat(int32_t x)
{
    int32_t a = x < 0 ? -x : x, i = a >> 5, y;
    y = i >= 2048 ? SAT_Q15[2048] : SAT_Q15[i] + (((SAT_Q15[i + 1] - SAT_Q15[i]) * (a & 31)) >> 5);
    return x < 0 ? -y : y;
}
static inline int32_t m12(int32_t a, int32_t b) { return (a * b) >> 12; }

typedef struct {
    int32_t z[4];
    int32_t g, g2, g3, rcp, k, gain, mk;   /* Q12 poles, 1 / (1 + k G^4) Q13, resonance Q12, in / out gain Q15 / Q12 */
} lad_t;

/* cutoff 0..1023 (the filter's curve), res 0..127 */
static void lad_coef(lad_t *l, int32_t cutoff, int32_t res)
{
    int32_t g = LAD_G[clamp(cutoff, 0, 1023)], g4;
    l->g = g;
    l->g2 = m12(g, g);
    l->g3 = m12(l->g2, g);
    g4 = m12(l->g3, g);
    l->k = clamp(res, 0, 127) * 18800 / 127;               /* ~4.6: self-oscillates near the top */
    l->rcp = (int32_t)((8192u * 4096u) / (uint32_t)(4096 + m12(l->k, g4)));
    l->gain = 32767 - clamp(res, 0, 127) * 60;             /* (the resonance's bass loss, a little) */
    l->mk = 4096 + (l->k * 3 >> 2);                        /* make-up: the pass band 1 / (1 + k) */
}

static inline int32_t lad_run(lad_t *l, int32_t x, int ssi)
{
    int32_t g = l->g, u, d, y;
    int32_t sigma = m12(4096 - g, m12(l->g3, l->z[0]) + m12(l->g2, l->z[1]) + m12(g, l->z[2]) + l->z[3]);
    x = (x * l->gain) >> 15;
    u = clamp(x - m12(l->k, sigma), -262143, 262143);
    u = sat((u * l->rcp) >> 13);
#define LSTAGE(j, S) d = m12(g, u - l->z[j]); y = l->z[j] + d; l->z[j] = y + d; u = S(y)   /* (bounded: u is) */
#define LIN(v) (v)
    if (ssi) {
        LSTAGE(0, sat); LSTAGE(1, sat); LSTAGE(2, sat); LSTAGE(3, sat);
    } else {
        LSTAGE(0, LIN); LSTAGE(1, LIN); LSTAGE(2, LIN); LSTAGE(3, LIN);
    }
#undef LIN
#undef LSTAGE
    return sat((u * l->mk) >> 12);
}

/* the master: DC blocker (~2 Hz; the step's remainder fed back, so no offset is left after silence), peak
 * limiter (fast attack, ~150 ms release) in front of the soft clipper: chords get quieter, not crushed */
#define LIM_T 18000
static struct { int32_t dc[2], dce[2], env; } mst = {{0, 0}, {0, 0}, LIM_T};
static inline int32_t dc_block(int32_t x, int32_t *dc, int32_t *err)
{
    int32_t e = (x << 6) - *dc + *err, d = e >> 12;
    *err = e - (d << 12);
    *dc += d;
    return x - ((*dc + 32) >> 6);
}
static inline void master_out(int32_t *l, int32_t *r)
{
    int32_t al, ar, a;
    *l = dc_block(*l, &mst.dc[0], &mst.dce[0]);
    *r = dc_block(*r, &mst.dc[1], &mst.dce[1]);
    al = *l < 0 ? -*l : *l;
    ar = *r < 0 ? -*r : *r;
    a = al > ar ? al : ar;
    if (a > mst.env)
        mst.env += (a - mst.env) >> 2;
    else if (mst.env > LIM_T)
        mst.env -= ((mst.env - LIM_T) >> 12) + 1;
    if (mst.env > LIM_T) {
        int32_t g = (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)mst.env);
        *l = ((*l >> 4) * g) >> 11;
        *r = ((*r >> 4) * g) >> 11;
        syn.overs++;
    }
    *l = sat(*l);
    *r = sat(*r);
}
