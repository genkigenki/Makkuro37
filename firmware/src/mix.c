/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* The mix: the control tick of every voice, each voice's colour (tanh, tone), the stereo bus (volume, pan,
 * width), master, the dual filter and effector (fx.c), Melodee's limiter and soft clip (analog.c). */
static int32_t vol_q15(int32_t v) { v = clamp(v, 0, 127); return v * v * 2; }

static void synth_tick(void)
{
    uint32_t v;
    int32_t fine = pt.g[G_FINE], glide = clamp(pt.g[G_GLIDE], 0, 127) * 3 / 4;
    syn.ticks++;
    mod_tick();
    kb_tick();
    for (v = 0; v < NV; v++) {
        vstate_t *s = &vs[v];
        const int16_t *p = pt.v[v];
        uint32_t gate = gate_in[v] | gate_src((uint32_t)p[VP_GSRC], v), vel = gate_vel[v];
        int32_t att = clamp(p[VP_ATT] + (100 - (int32_t)vel) / 3, 0, 127);   /* soft pad: slower swell */
        if (VTYPE[v] == VT_VCO) {
            uint32_t i = v - 3u;
            gate = gate_src((uint32_t)p[VP_GSRC], v) | kb.gate[i] | (kb.padhold[i] && kb.played) |
                   (free_vco && gate_in[v]);           /* (keys only; in FREE-PLAY the key that woke it) */
            if (s->trig != kb.trig[i] && kb.gate[i])
                s->gate = 0;                       /* a new key or arp step: attack again */
            s->trig = kb.trig[i];
            adsr(s, p, gate);
            vco_tick(v, s, p, fine, glide);
        } else {
            env_ar(s, gate, (uint32_t)att, (uint32_t)p[VP_RLS]);
            if (s->stage) {
                if (VTYPE[v] == VT_CLASSIC)
                    classic_tick(s, p, fine);
                else
                    noise_tick(s, p, fine);
            }
        }
        s->level = (((env_amp(s->env) * vol_q15(p[VP_VOL])) >> 15) * (16384 + (int32_t)vel * 129)) >> 15;
    }
    modv[MS_SH3] = vs[2].sh;
    modv[MS_SH8] = vs[7].sh;
}

static int32_t bus_l[CTL], bus_r[CTL], vbuf[NV][CTL], vbuf2[NV][CTL];

/* the boot chime: a few sine plings (draw.c boot animation) */
static struct { uint32_t ph, inc; int32_t env; } chime[4];
static void chime_hit(int32_t cents)
{
    static uint32_t k;
    chime[k & 3u].ph = 0;
    chime[k & 3u].inc = cents_inc(cents);
    chime[k & 3u].env = 30000;
    k++;
}
static void chime_run(uint32_t n)
{
    uint32_t c, i;
    for (c = 0; c < 4u; c++) {
        int32_t e = chime[c].env;
        if (!e)
            continue;
        for (i = 0; i < n; i++) {
            int32_t y = (SINE[(chime[c].ph += chime[c].inc) >> 22] * e) >> 16;
            bus_l[i] += y;
            bus_r[i] += y;
        }
        chime[c].env = (e * 31700) >> 15;             /* ~0.5 s */
    }
}

/* a voice's colour: a tanh knee (the mixer driven a little), then two one-poles (TONE): the saws blend
 * instead of buzzing */
static void voice_tone(int32_t *x, int32_t *lp, int32_t k, uint32_t n)
{
    uint32_t i;
    int32_t a = lp[0], b = lp[1];
    for (i = 0; i < n; i++) {
        int32_t y = (sat(x[i] + (x[i] >> 1)) * 21845) >> 15;      /* x 1.5 into tanh, back by 2/3 */
        a += ((y - a) * k) >> 15;
        b += ((a - b) * k) >> 15;
        x[i] = b;
    }
    lp[0] = a, lp[1] = b;
}

static void voice_out(const int32_t *x, int32_t lvl, int32_t pan, int32_t *g_l, int32_t *g_r, uint32_t n)
{
    int32_t p = clamp(pan, 0, 127) * 256 / 127;                    /* quarter sine: constant power */
    int32_t tl = (lvl * SINE[256 + p]) >> 15, tr = (lvl * SINE[p]) >> 15;
    int32_t dl = (tl - *g_l) >> 5, dr = (tr - *g_r) >> 5, gl = *g_l, gr = *g_r;   /* (n = CTL = 32) */
    uint32_t i;
    for (i = 0; i < n; i++) {
        gl += dl;
        gr += dr;
        bus_l[i] += (x[i] * gl) >> 15;
        bus_r[i] += (x[i] * gr) >> 15;
    }
    *g_l = tl, *g_r = tr;
}

/* one control block: n = CTL frames into out (L, R interleaved, Q15) */
static void synth_block(int32_t *out, uint32_t n)
{
    uint32_t v, i;
    int32_t m;
    uint8_t on[NV];
    synth_tick();
    for (i = 0; i < n; i++)
        bus_l[i] = bus_r[i] = 0;
    for (v = 0; v < NV; v++) {
        on[v] = vs[v].stage || vs[v].gl || vs[v].gr || vs[v].gl2 || vs[v].gr2;
        if (on[v])
            for (i = 0; i < n; i++)
                vbuf[v][i] = vbuf2[v][i] = 0;
    }
    for (v = 0; v < NV; v++)
        if (on[v]) {
            if (VTYPE[v] == VT_CLASSIC)
                classic_render(&vs[v], pt.v[v], vbuf[v], vbuf2[v], n);
            else if (VTYPE[v] == VT_NOISE)
                noise_render(&vs[v], pt.v[v], vbuf[v], n);
        }
    if (on[3] || on[4])
        vco_render(on[3] ? vbuf[3] : 0, on[4] ? vbuf[4] : 0, n);
    for (v = 0; v < NV; v++) {
        vstate_t *s = &vs[v];
        int32_t k = TONE_K[pt.v[v][VP_TONE] & 127], pan = pt.v[v][VP_PAN], w = pt.g[G_WIDTH];
        if (!on[v])
            continue;
        voice_tone(vbuf[v], s->lp[0], k, n);
        if (VTYPE[v] == VT_CLASSIC) {          /* its two halves (generators 1 3 5 / 2 4) apart: width */
            voice_tone(vbuf2[v], s->lp[1], k, n);
            voice_out(vbuf[v], s->level, pan - w, &s->gl, &s->gr, n);
            voice_out(vbuf2[v], s->level, pan + w, &s->gl2, &s->gr2, n);
        } else
            voice_out(vbuf[v], s->level, pan, &s->gl, &s->gr, n);
    }
    chime_run(n);
    m = vol_q15(pt.g[G_MASTER]);
    for (i = 0; i < n; i++) {
        bus_l[i] = (bus_l[i] * (m >> 4) * 3) >> 12; /* (x 1.5: a few drones already sound full; the limiter */
        bus_r[i] = (bus_r[i] * (m >> 4) * 3) >> 12; /* keeps many in hand) */
    }
    {   /* the dual filter (R follows L with LINK, a hair apart), then the effector */
        int32_t src = pt.g[G_FCVSRC], cv = src ? (modv[src % MS_N] * pt.g[G_FCVAMT]) >> 12 : 0;
        int32_t link = pt.g[G_FLINK], fd = pt.g[G_FMD] * pt.g[G_FMD] / 40, rw = (joy_y * 50) >> 15;
        static uint32_t fph;
        fph += LFO_A[pt.g[G_FMR] & 127] * 2u;   /* the filters' LFO: R a quarter on */
        flt_tick(&flt[0], pt.g[G_FFL], pt.g[G_FRL] + rw, cv + ((SINE[fph >> 22] * fd) >> 15));
        flt_tick(&flt[1], link ? pt.g[G_FFL] : pt.g[G_FFR], (link ? pt.g[G_FRL] : pt.g[G_FRR]) + rw,
                 cv + (link ? 6 : 0) + ((SINE[(fph + 0x40000000u) >> 22] * fd) >> 15));   /* (the mod wheel: + res) */
        flt_run(&flt[0], bus_l, n);
        flt_run(&flt[1], bus_r, n);
        fx_run(bus_l, bus_r, n);
    }
    tape_block(pt.g[G_TAPE]);
    for (i = 0; i < n; i++) {                       /* Melodee's master: DC, limiter, soft clip; the peak */
        int32_t l = bus_l[i], r = bus_r[i], a, b;
        master_out(&l, &r);
        a = l < 0 ? -l : l, b = r < 0 ? -r : r;
        if (a > syn.peak)
            syn.peak = a;
        if (b > syn.peak)
            syn.peak = b;
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}
