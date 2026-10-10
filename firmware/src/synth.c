/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* The sound engine, fixed point. synth_block() renders CTL stereo samples (Q15 in int32, L R L R ...):
 * one control tick (envelopes, pitches, gains), then the voices into the bus, then the master. */
static patch_t pt;                         /* the sound (scene) */
static vstate_t vs[NV];
static volatile uint8_t gate_in[NV];       /* gates from the pads / keys (UI) */
static volatile uint8_t gate_vel[NV] = {100, 100, 100, 100, 100, 100, 100, 100};   /* the pads' velocity */
static int32_t modv[MS_N];                 /* modulation sources, Q15 */
static volatile int32_t joy_x, joy_y;      /* wheels, Q15: X -32767..32767 (springs back), Y 0..32767 */
static struct {
    int32_t peak;                          /* |out| max since the last read (console), Q15 */
    uint32_t overs;                        /* samples the limiter had to bend */
    uint32_t ticks;
    int32_t xl, xr, dl, dr;                /* DC blocker state (x256) */
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
static const uint16_t VNORM[6] = {0, 22938, 16219, 13243, 11469, 10258};   /* 0.7 / sqrt(n) */

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

/* ------------------------------------------------------------- noise ("Papa Srapa") --- */
/* tone: a square (PITCH, RANGE); modulator: a square (RATE, x1 / x10) whose DIVIDER makes a staircase of
 * 2 x DIVIDER steps; FM bends the tone up to 5x along it (with a glide: the sirens), AM gates the tone.
 * NOISE adds white noise; the S&H samples it on its clock and moves the tone (S&H AMT). */
enum { NK_FMD, NK_LAG, NK_STEP, NK_TONE, NK_NOISE, NK_MODE };
static uint8_t clk_edges;                  /* this tick: bit 0 LFO A wrapped, 1 LFO B, 2 pulser step */

static inline uint32_t xorshift(uint32_t *x)
{
    uint32_t v = *x ? *x : 0x9E3779B9u;
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    return *x = v;
}

static void noise_tick(vstate_t *s, const int16_t *p, int32_t fine)
{
    int32_t lo = p[NZ_RANGE] ? 4800 : 1200, hi = p[NZ_RANGE] ? 10000 : 6000, a = p[NZ_SHAMT], c;
    uint32_t clk = (uint32_t)p[NZ_SHCLK];
    if (clk && ((clk_edges >> (clk - 1u)) & 1u))
        s->sh = (int32_t)xorshift(&s->nz) >> 16;   /* S&H on LFO A / B / the pulser */
    c = lo + p[NZ_PITCH] * (hi - lo) / 127 + fine + mod_cents(p) + ((s->sh * (a * a * 2400 / 16129)) >> 15);
    s->inc[0] = cents_inc(c);
    if (s->inc[0] > 1u << 28)
        s->inc[0] = 1u << 28;
    s->minc = (MODR[p[NZ_RATE] & 127] * (p[NZ_MULT] ? 10u : 1u)) >> 5;   /* per sample */
    s->kv[NK_FMD] = p[NZ_FMAMT] * p[NZ_FMAMT] * 2;
    s->kv[NK_LAG] = clamp((int32_t)((s->minc >> 8) * 25u >> 9), 8, 32767);   /* glide ~ 1/8 modulator period */
    s->kv[NK_STEP] = 32767 / (2 * clamp(p[NZ_DIV], 1, 8) - 1);
    s->kv[NK_TONE] = p[NZ_PITCH] ? 24000 : 0;      /* PITCH all the way down: noise only */
    s->kv[NK_NOISE] = p[NZ_NOISE] * p[NZ_NOISE] * 3;
    s->kv[NK_MODE] = p[NZ_MODE];
}

static void noise_render(vstate_t *s, const int16_t *p, int32_t *o, uint32_t n)
{
    uint32_t i, ph = s->ph[0], base = s->inc[0], mph = s->mph, minc = s->minc, div = (uint32_t)clamp(p[NZ_DIV], 1, 8);
    uint32_t sh_mod = p[NZ_SHCLK] == 0, mode = (uint32_t)s->kv[NK_MODE], cnt = s->cnt;
    int32_t lag = s->lag, am = s->am, fmd = s->kv[NK_FMD], ka = s->kv[NK_LAG], step = s->kv[NK_STEP];
    int32_t tone = s->kv[NK_TONE], nl = s->kv[NK_NOISE];
    int fm = mode == NM_FM || mode == NM_FMAM, amm = mode == NM_AM || mode == NM_FMAM;
    for (i = 0; i < n; i++) {
        uint32_t inc = base, half;
        int32_t y, m;
        mph += minc;
        if (mph < minc) {                              /* the modulator's cycle */
            cnt = cnt + 1u >= div ? 0u : cnt + 1u;
            if (sh_mod)
                s->sh = (int32_t)xorshift(&s->nz) >> 16;
        }
        half = mph >> 31;
        if (fm) {
            m = (int32_t)(cnt * 2u + half) * step;     /* 0..32767 staircase (a square for DIVIDER 1) */
            lag += ((m - lag) * ka) >> 15;
            inc += (base >> 13) * (uint32_t)((lag * fmd) >> 15);
        }
        ph += inc;
        y = (ph < 0x80000000u ? 32767 : -32767) + blep(ph, inc) - blep(ph + 0x80000000u, inc);
        y = (y * tone) >> 15;
        if (amm) {
            am += (((int32_t)(half ^ 1u) * 32767 - am) >> 5);
            y = (y * am) >> 15;
        }
        if (nl)
            y += (((int32_t)xorshift(&s->nz) >> 16) * nl) >> 15;
        o[i] += y;
    }
    s->ph[0] = ph, s->mph = mph, s->cnt = (uint8_t)cnt, s->lag = lag, s->am = am;
}

/* ------------------------------------------------------------- VCO --- */
/* voices 4 + 5: six waves (two of them morphing), octave switch, TUNE, FM (4: feedback, 5: from voice 4),
 * SYNC (5 to 4), ADSR with HOLD / LOOP; pitch from the keys (key_cents), gliding. */
static int32_t key_cents[2] = {4800, 4800};        /* the keys' pitches for voices 4 / 5 (cents) */
static volatile uint8_t key_gate, key_trig;        /* a key held; presses so far */
static const int16_t OCT_CENTS[8] = {-6000, -3600, -2400, -1200, 0, 1200, 2400, 3600};
enum { VK_W, VK_KU, VK_KD, VK_SHAPE, VK_PW, VK_FM };

static void adsr(vstate_t *s, const int16_t *p, uint32_t gate)
{
    int32_t top = 1 << 24, sus = p[VC_SUS] * 132104;          /* SUSTAIN 127 = full */
    uint32_t eg = (uint32_t)p[VC_EG];
    if (gate && (!s->gate || s->stage == 0u || s->stage == 3u))
        s->stage = 1;
    if (!gate && s->stage && s->stage != 3u)
        s->stage = 3;
    s->gate = (uint8_t)gate;
    if (s->stage == 1u) {
        s->env += (int32_t)ENV_STEP[p[VP_ATT] & 127];
        if (s->env >= top) {
            s->env = top;
            s->stage = 2;
        }
    } else if (s->stage == 2u) {
        int32_t floor = eg == EG_LOOP ? 0 : eg == EG_HOLD ? top : sus;   /* HOLD: stays up (A / R) */
        s->env = floor + (int32_t)((((uint32_t)(s->env - floor)) >> 8) * ENV_FALL[p[VC_DEC] & 127] >> 8);
        if (eg == EG_LOOP && s->env < 1 << 18)
            s->stage = 1;                          /* LOOP: attack again (while the gate is on) */
    } else if (s->stage == 3u) {
        s->env = (int32_t)(((uint32_t)s->env >> 8) * ENV_FALL[p[VP_RLS] & 127] >> 8);
        if (s->env < 1 << 10) {
            s->env = 0;
            s->stage = 0;
        }
    }
}

static void vco_tick(uint32_t v, vstate_t *s, const int16_t *p, int32_t fine, int32_t glide)
{
    int32_t tgt = (key_cents[v - 3u] + OCT_CENTS[p[VC_OCT] & 7] + p[VC_TUNE] + fine + mod_cents(p) +
                   ((joy_x * 200) >> 15)) * 16, w;                 /* the joystick bends +-2 semitones */
    if (!s->pitch || !glide)
        s->pitch = tgt;
    else
        s->pitch = tgt + (int32_t)(((int64_t)(s->pitch - tgt) * ENV_FALL[glide]) >> 16);
    s->inc[0] = cents_inc(s->pitch >> 4);
    if (s->inc[0] > 1u << 29)
        s->inc[0] = 1u << 29;
    w = clamp(p[VC_SHAPE] * 258, 128, 32640);      /* the morph's corner, 15-bit phase */
    s->kv[VK_W] = w;
    s->kv[VK_KU] = (65535 << 6) / w;
    s->kv[VK_KD] = (65535 << 6) / (32768 - w);
    s->kv[VK_SHAPE] = p[VC_SHAPE] * 258;
    s->kv[VK_PW] = (int32_t)(0x80000000u + (uint32_t)(p[VC_SHAPE] - 64) * 0x1F00000u);   /* pulse width 5..95 % */
    s->kv[VK_FM] = p[VC_FM] * p[VC_FM] * 2;
}

static inline int32_t vco_wave(const vstate_t *s, uint32_t w, uint32_t ph, uint32_t inc)
{
    switch (w) {
    case W_SAW:
        return saw(ph, inc);
    case W_PULSE:
        return (saw(ph, inc) - saw(ph + (uint32_t)s->kv[VK_PW], inc)) >> 1;
    case W_TRI: {
        int32_t t = (int32_t)((ph + 0x40000000u) >> 15);
        return t < 65536 ? t - 32768 : 98303 - t;
    }
    case W_SINE:
        return SINE[ph >> 22];
    case W_SAWMORPH: {                             /* falling saw .. triangle .. rising saw */
        int32_t p15 = (int32_t)(ph >> 17), wq = s->kv[VK_W];
        return p15 < wq ? -32768 + ((p15 * s->kv[VK_KU]) >> 6) : 32767 - (((p15 - wq) * s->kv[VK_KD]) >> 6);
    }
    default: {                                     /* sine .. triangle */
        int32_t t = (int32_t)((ph + 0x40000000u) >> 15), sn = SINE[ph >> 22];   /* (in phase with the sine) */
        t = t < 65536 ? t - 32768 : 98303 - t;
        return sn + (((t - sn) * s->kv[VK_SHAPE]) >> 15);
    }
    }
}

/* both VCOs in one pass: 5 follows 4 (FM, SYNC) */
static void vco_render(int32_t *o4, int32_t *o5, uint32_t n)
{
    vstate_t *a = &vs[3], *b = &vs[4];
    const int16_t *pa = pt.v[3], *pb = pt.v[4];
    uint32_t i, wa = (uint32_t)pa[VC_WAVE], wb = (uint32_t)pb[VC_WAVE], pha = a->ph[0], phb = b->ph[0];
    uint32_t ia = a->inc[0], ib = b->inc[0], sync = (uint32_t)pb[VC_SYNC], on_a = o4 != 0, on_b = o5 != 0;
    int32_t fa = a->kv[VK_FM], fb = b->kv[VK_FM], ya = a->last[0], yb;
    for (i = 0; i < n; i++) {
        uint32_t inc = ia + (uint32_t)((int32_t)(ia >> 14) * ((ya * fa) >> 15));   /* 4: feedback FM */
        pha += inc;
        ya = vco_wave(a, wa, pha, inc);
        if (on_a)
            o4[i] += (ya * 26000) >> 15;
        if (on_b) {
            inc = ib + (uint32_t)((int32_t)(ib >> 13) * ((ya * fb) >> 15));         /* 5: FM from 4 */
            phb += inc;
            if (sync && pha < ia)
                phb = pha;                         /* hard sync: 5 restarts with 4 */
            yb = vco_wave(b, wb, phb, inc);
            o5[i] += (yb * 26000) >> 15;
        }
    }
    a->ph[0] = pha, b->ph[0] = phb, a->last[0] = ya;
}

/* ------------------------------------------------------------- modulation --- */
/* LFO A (slow) / B (fast): square .. triangle, positive only; the joystick (wheels, from the
 * UI); the 5-step sequencer on the PULSER clock; DRIFT, a slow random walk. Once per control tick. */
static struct {
    uint32_t pa, pb, pp;                   /* LFO A, LFO B, pulser phases */
    uint8_t step, gate;                    /* sequencer step 0..4, its gate now */
    volatile uint8_t restart;              /* (UI: tap tempo) start at step 1 on the next tick */
    int32_t drift, dtarget;
    uint32_t dt, rnd;
} md;
static volatile uint32_t pulser_inc;       /* tap tempo: phase step per tick, 0 = the PULSER knob */

static int32_t lfo_val(uint32_t ph, int32_t wave)    /* 0..32767; wave 0 square .. 127 triangle */
{
    int32_t sq = ph < 0x80000000u ? 32767 : 0, tri = (int32_t)((ph < 0x80000000u ? ph : ~ph) >> 16);
    return sq + (((tri - sq) * wave) >> 7);
}

static void mod_tick(void)
{
    uint32_t inc, old;
    clk_edges = 0;
    old = md.pa;
    md.pa += LFO_A[pt.g[G_LARATE] & 127];
    clk_edges |= md.pa < old;
    old = md.pb;
    md.pb += LFO_B[pt.g[G_LBRATE] & 127];
    clk_edges |= (uint8_t)((md.pb < old) << 1);
    modv[MS_LFOA] = lfo_val(md.pa, pt.g[G_LAWAVE]);
    modv[MS_LFOB] = lfo_val(md.pb, pt.g[G_LBWAVE]);
    modv[MS_JOYX] = clamp(joy_x + pt.g[G_JXOFS] * 512, -32767, 32767);
    modv[MS_JOYY] = clamp(joy_y + pt.g[G_JYOFS] * 512, -32767, 32767);
    if (md.restart) {
        md.restart = 0;
        md.step = 0;
        md.pp = 0;
        clk_edges |= 4u;
    }
    if (pt.g[G_RUN]) {
        inc = pulser_inc ? pulser_inc : PULSE[pt.g[G_PULSER] & 127];
        old = md.pp;
        md.pp += inc;
        if (md.pp < old) {                         /* the next step */
            md.step = (uint8_t)(md.step + 1u >= 3u + (uint32_t)pt.g[G_STAGES] ? 0u : md.step + 1u);
            clk_edges |= 4u;
        }
        md.gate = (uint8_t)(md.pp < 0x80000000u && ((pt.g[G_SGATES] >> md.step) & 1));
    } else
        md.gate = 0;
    if (md.step >= 3u + (uint32_t)pt.g[G_STAGES])
        md.step = 0;
    modv[MS_SEQ] = pt.g[G_SEQ1 + md.step] * 258;
    if (++md.dt >= 2756u) {                        /* DRIFT: a new aim every ~2 s */
        md.dt = 0;
        md.dtarget = (int32_t)xorshift(&md.rnd) >> 16;
    }
    md.drift += (md.dtarget - md.drift) >> 9;
    modv[MS_DRIFT] = md.drift;
}

static uint32_t gate_src(uint32_t s)
{
    switch (s) {
    case GS_SEQ: return md.gate;
    case GS_LFOA: return modv[MS_LFOA] > 16384;
    case GS_LFOB: return modv[MS_LFOB] > 16384;
    default: return 0;
    }
}

