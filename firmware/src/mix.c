/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* The mix: the control tick of every voice, the voices into the stereo bus (volume, pan), master, the dual
 * filter and effector (fx.c), the soft limiter. */
static int32_t vol_q15(int32_t v) { v = clamp(v, 0, 127); return v * v * 2; }

static void synth_tick(void)
{
    uint32_t v, trig = key_trig;
    int32_t fine = pt.g[G_FINE], glide = clamp(pt.g[G_GLIDE], 0, 127) * 3 / 4;
    syn.ticks++;
    mod_tick();
    for (v = 0; v < NV; v++) {
        vstate_t *s = &vs[v];
        const int16_t *p = pt.v[v];
        uint32_t gate = gate_in[v] | gate_src((uint32_t)p[VP_GSRC]), vel = gate_vel[v];
        int32_t att = clamp(p[VP_ATT] + (100 - (int32_t)vel) / 3, 0, 127);   /* soft pad: slower swell */
        if (VTYPE[v] == VT_VCO) {
            gate |= key_gate;
            if (s->trig != (uint8_t)trig && key_gate)
                s->gate = 0;                       /* a new key: attack again */
            s->trig = (uint8_t)trig;
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

static int32_t bus_l[CTL], bus_r[CTL], vbuf[NV][CTL];

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
        on[v] = vs[v].stage || vs[v].gl || vs[v].gr;
        if (on[v])
            for (i = 0; i < n; i++)
                vbuf[v][i] = 0;
    }
    for (v = 0; v < NV; v++)
        if (on[v]) {
            if (VTYPE[v] == VT_CLASSIC)
                classic_render(&vs[v], pt.v[v], vbuf[v], n);
            else if (VTYPE[v] == VT_NOISE)
                noise_render(&vs[v], pt.v[v], vbuf[v], n);
        }
    if (on[3] || on[4])
        vco_render(on[3] ? vbuf[3] : 0, on[4] ? vbuf[4] : 0, n);
    for (v = 0; v < NV; v++) {
        vstate_t *s = &vs[v];
        int32_t pan = clamp(pt.v[v][VP_PAN], 0, 127) * 256 / 127;          /* quarter sine: constant power */
        int32_t tl = (s->level * SINE[256 + pan]) >> 15, tr = (s->level * SINE[pan]) >> 15;
        int32_t dl = (tl - s->gl) >> 5, dr = (tr - s->gr) >> 5, gl = s->gl, gr = s->gr;   /* (n = CTL = 32) */
        const int32_t *x = vbuf[v];
        if (!on[v])
            continue;
        for (i = 0; i < n; i++) {
            gl += dl;
            gr += dr;
            bus_l[i] += (x[i] * gl) >> 15;
            bus_r[i] += (x[i] * gr) >> 15;
        }
        s->gl = tl;
        s->gr = tr;
    }
    m = vol_q15(pt.g[G_MASTER]);
    for (i = 0; i < n; i++) {                       /* DC blocker (~7 Hz), master */
        int32_t xl = bus_l[i] << 8, xr = bus_r[i] << 8;
        syn.dl += xl - syn.xl - ((syn.dl + 512) >> 10);
        syn.dr += xr - syn.xr - ((syn.dr + 512) >> 10);
        syn.xl = xl, syn.xr = xr;
        bus_l[i] = ((syn.dl >> 8) * (m >> 4)) >> 11;
        bus_r[i] = ((syn.dr >> 8) * (m >> 4)) >> 11;
    }
    {   /* the dual filter (R follows L with LINK, a hair apart), then the effector */
        int32_t src = pt.g[G_FCVSRC], cv = src ? (modv[src % MS_N] * pt.g[G_FCVAMT]) >> 12 : 0;
        int32_t link = pt.g[G_FLINK];
        flt_tick(&flt[0], pt.g[G_FFL], pt.g[G_FRL], cv);
        flt_tick(&flt[1], link ? pt.g[G_FFL] : pt.g[G_FFR], link ? pt.g[G_FRL] : pt.g[G_FRR], cv + (link ? 6 : 0));
        flt_run(&flt[0], bus_l, n);
        flt_run(&flt[1], bus_r, n);
        fx_run(bus_l, bus_r, n);
    }
    for (i = 0; i < n; i++) {                       /* soft limiter, peak */
        int32_t l = soft(bus_l[i]), r = soft(bus_r[i]);
        int32_t a = l < 0 ? -l : l, b = r < 0 ? -r : r;
        if (a > syn.peak)
            syn.peak = a;
        if (b > syn.peak)
            syn.peak = b;
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}
