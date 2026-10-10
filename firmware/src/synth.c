/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* The sound engine, fixed point. synth_block() renders CTL stereo samples (Q15 in int32, L R L R ...):
 * one control tick (envelopes, pitches, gains), then the voices into the bus, then the master. */
static patch_t pt;                         /* the sound (scene) */
static vstate_t vs[NV];
static volatile uint8_t gate_in[NV];       /* gates from the pads / keys (UI) */
static int32_t modv[MS_N];                 /* modulation sources, Q15 */
static struct {
    int32_t peak;                          /* |out| max since the last read (console), Q15 */
    uint32_t overs;                        /* samples the limiter had to bend */
    uint32_t ticks;
} syn;

static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* cents above MIDI 0 (8.18 Hz), -3600 .. 13199 -> phase increment (2^32 = FS) */
static uint32_t cents_inc(int32_t c)
{
    uint32_t o, r;
    c = clamp(c, -3600, 13199) + 3600;
    o = (uint32_t)c / 1200u;
    r = (uint32_t)c - o * 1200u;
    return INC_TOP[r] >> (13u - o);
}

/* polyBLEP residual (Q15) of a saw step at a phase wrap */
static inline int32_t blep(uint32_t ph, uint32_t inc)
{
    uint32_t d = inc >> 15;
    int32_t x;
    if (!d)
        return 0;
    if (ph < inc) {
        x = (int32_t)(ph / d);
        return x + x - ((x * x) >> 15) - 32768;
    }
    if (ph > 0xFFFFFFFFu - inc) {
        x = -(int32_t)((0xFFFFFFFFu - ph) / d);
        return ((x * x) >> 15) + x + x + 32768;
    }
    return 0;
}
static inline int32_t saw(uint32_t ph, uint32_t inc) { return (int32_t)(ph >> 16) - 32768 - blep(ph, inc); }

/* soft knee: linear to -6 dBFS, then tanh up to full scale */
static inline int32_t soft(int32_t x)
{
    int32_t a = x < 0 ? -x : x, i, f, y;
    if (a <= 16384)
        return x;
    syn.overs += a > 32767;
    a -= 16384;                            /* tanh(a / 16384) over the table's 0..4 */
    i = a >> 8;
    if (i >= 256)
        y = 32767;
    else {
        f = a & 255;
        y = TANH[256 + i] + (((TANH[257 + i] - TANH[256 + i]) * f) >> 8);
    }
    y = 16384 + (y >> 1);
    return x < 0 ? -y : y;
}

/* ------------------------------------------------------------- envelopes --- */
/* A / R (classic, noise); amplitude = env squared */
static void env_ar(vstate_t *s, uint32_t gate, uint32_t att, uint32_t rls)
{
    if (gate && (!s->gate || s->stage == 0u || s->stage == 3u))
        s->stage = 1;
    if (!gate && s->stage && s->stage != 3u)
        s->stage = 3;
    s->gate = (uint8_t)gate;
    if (s->stage == 1u) {
        s->env += (int32_t)ENV_STEP[att & 127u];
        if (s->env >= 1 << 24) {
            s->env = 1 << 24;
            s->stage = 2;
        }
    } else if (s->stage == 3u) {
        s->env = (int32_t)(((uint32_t)s->env >> 8) * ENV_FALL[rls & 127u] >> 8);
        if (s->env < 1 << 10) {
            s->env = 0;
            s->stage = 0;
        }
    }
}
static inline int32_t env_amp(int32_t env) { int32_t e = env >> 9; return (e * e) >> 15; }   /* Q15 */

/* ------------------------------------------------------------- classic --- */
static const uint8_t GEN_LO[5] = {12, 35, 50, 64, 79};      /* C0, B1, D3, E4, G5 */
static const uint8_t GEN_HI[5] = {43, 55, 71, 84, 98};      /* G2, G3, B4, C6, D7 */
static const uint16_t VNORM[6] = {0, 16384, 11585, 9459, 8192, 7327};   /* 0.5 / sqrt(n) */

static int32_t mod_cents(const int16_t *p)      /* the voice's MOD source: up to +-1 octave */
{
    int32_t a = p[VP_MAMT];
    return p[VP_MSRC] ? (modv[p[VP_MSRC] % MS_N] * (a * a * 1200 / 16129)) >> 15 : 0;
}

static void classic_tick(vstate_t *s, const int16_t *p, int32_t fine)
{
    int32_t volt = p[CL_VOLT], trans = -volt * 2400 / 127, mc = mod_cents(p);
    uint32_t g;
    for (g = 0; g < 5u; g++) {
        int32_t c = GEN_LO[g] * 100 + p[CL_TUNE1 + g] + trans + fine;
        if ((p[CL_MOD] >> g) & 1)
            c += mc;
        s->inc[g] = (p[CL_MUTE] >> g) & 1 ? 0u : cents_inc(c);
    }
}

static void classic_render(vstate_t *s, const int16_t *p, int32_t *o, uint32_t n)
{
    int32_t volt = p[CL_VOLT], fm = volt > 64 ? (volt - 64) * 520 : 0;   /* past half: cross-FM, Q15 */
    uint32_t g, i, act = 0;
    for (g = 0; g < 5u; g++)
        act += s->inc[g] != 0u;
    if (!act)
        return;
    if (!fm) {                                 /* one generator at a time */
        int32_t k = VNORM[act];
        for (g = 0; g < 5u; g++) {
            uint32_t ph = s->ph[g], inc = s->inc[g];
            if (!inc)
                continue;
            for (i = 0; i < n; i++) {
                ph += inc;
                o[i] += (saw(ph, inc) * k) >> 15;
            }
            s->ph[g] = ph;
            s->last[g] = 0;
        }
        return;
    }
    {   /* each generator's pitch follows the one before it (5 -> 1 -> 2 -> ...) */
        int32_t k = VNORM[act], l0 = s->last[0], l1 = s->last[1], l2 = s->last[2], l3 = s->last[3], l4 = s->last[4];
        for (i = 0; i < n; i++) {
            int32_t sum = 0;
#define GEN(G, PREV, L)                                                                         \
            if (s->inc[G]) {                                                                    \
                uint32_t inc = s->inc[G] + (uint32_t)((int32_t)(s->inc[G] >> 15) * ((PREV * fm) >> 15)); \
                s->ph[G] += inc;                                                                \
                L = saw(s->ph[G], inc);                                                         \
                sum += L;                                                               \
            }
            GEN(0, l4, l0)
            GEN(1, l0, l1)
            GEN(2, l1, l2)
            GEN(3, l2, l3)
            GEN(4, l3, l4)
#undef GEN
            o[i] += (sum * k) >> 15;
        }
        s->last[0] = l0, s->last[1] = l1, s->last[2] = l2, s->last[3] = l3, s->last[4] = l4;
    }
}

/* ------------------------------------------------------------- the mix --- */
static int32_t vol_q15(int32_t v) { v = clamp(v, 0, 127); return v * v * 2; }

static void synth_tick(void)
{
    uint32_t v;
    syn.ticks++;
    for (v = 0; v < NV; v++) {
        vstate_t *s = &vs[v];
        const int16_t *p = pt.v[v];
        uint32_t gate = gate_in[v];
        int32_t amp;
        if (VTYPE[v] != VT_CLASSIC)
            continue;
        env_ar(s, gate, (uint32_t)p[VP_ATT], (uint32_t)p[VP_RLS]);
        if (s->stage)
            classic_tick(s, p, pt.g[G_FINE]);
        amp = (env_amp(s->env) * vol_q15(p[VP_VOL])) >> 15;
        s->level = amp;
    }
}

static int32_t bus_l[CTL], bus_r[CTL], vbuf[CTL];

/* one control block: n = CTL frames into out (L, R interleaved, Q15) */
static void synth_block(int32_t *out, uint32_t n)
{
    uint32_t v, i;
    int32_t m;
    synth_tick();
    for (i = 0; i < n; i++)
        bus_l[i] = bus_r[i] = 0;
    for (v = 0; v < NV; v++) {
        vstate_t *s = &vs[v];
        const int16_t *p = pt.v[v];
        int32_t pan = clamp(p[VP_PAN], 0, 127) * 256 / 127;          /* quarter sine: constant power */
        int32_t tl = (s->level * SINE[256 + pan]) >> 15, tr = (s->level * SINE[pan]) >> 15;
        int32_t dl, dr, gl = s->gl, gr = s->gr;
        if (!s->stage && !gl && !gr)
            continue;
        for (i = 0; i < n; i++)
            vbuf[i] = 0;
        if (VTYPE[v] == VT_CLASSIC)
            classic_render(s, p, vbuf, n);
        dl = (tl - gl) >> 5;                   /* (n = CTL = 32) */
        dr = (tr - gr) >> 5;
        for (i = 0; i < n; i++) {
            gl += dl;
            gr += dr;
            bus_l[i] += (vbuf[i] * gl) >> 15;
            bus_r[i] += (vbuf[i] * gr) >> 15;
        }
        s->gl = tl;
        s->gr = tr;
    }
    m = vol_q15(pt.g[G_MASTER]);
    for (i = 0; i < n; i++) {
        int32_t l = soft((bus_l[i] * (m >> 4)) >> 11), r = soft((bus_r[i] * (m >> 4)) >> 11);
        int32_t a = l < 0 ? -l : l, b = r < 0 ? -r : r;
        if (a > syn.peak)
            syn.peak = a;
        if (b > syn.peak)
            syn.peak = b;
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}
