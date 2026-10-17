/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) */
/* The keys play voices 4 + 5: SINGLE (both on the last key), DUO (the last
 * two keys: 4 the lower, 5 the upper), SPLIT (left half -> 4, right half -> 5); an arpeggiator (up, down, up-down,
 * random, the recorded 16-step key sequence) on its own clock or the PULSER, with LATCH and octaves; portamento
 * (GLIDE); vibrato, deeper for harder keys. The UI only reports keys (kb_press / kb_release); this runs per tick. */
static int32_t kb_cents(int32_t k)                  /* a key -> cents in the tuning set (bottom key = C2) */
{
    uint32_t set = (uint32_t)pt.g[G_TSET], root = (uint32_t)pt.g[G_ROOT], n = TSETS[set % TS_N].n;
    return n == 12u ? tun_key(set, root, 36 + k) : tun_degree(set, root, 3 * (int32_t)n + k);
}

static void kb_press(int32_t k, uint32_t side, uint32_t vel)   /* (UI) */
{
    uint32_t i, j;
    for (i = j = 0; i < kb.n; i++)                  /* (a key held again moves to the end) */
        if (kb.k[i] != k)
            kb.k[j] = kb.k[i], kb.side[j] = kb.side[i], kb.vel[j] = kb.vel[i], j++;
    if (j == 8u) {
        for (i = 1; i < 8u; i++)
            kb.k[i - 1u] = kb.k[i], kb.side[i - 1u] = kb.side[i], kb.vel[i - 1u] = kb.vel[i];
        j = 7;
    }
    kb.k[j] = (int8_t)k, kb.side[j] = (uint8_t)side, kb.vel[j] = (uint8_t)vel;
    kb.n = (uint8_t)(j + 1u);
    if (pt.g[G_ALATCH]) {                           /* LATCH: a new chord replaces the old one */
        if (j == 0u)
            kb.ln = 0;
        if (kb.ln < 8u)
            kb.lk[kb.ln++] = (int8_t)k;
    }
    if (kb.rec && kb.seqlen < 16u)                  /* STEP REC: each key is the next step */
        kb.seq[kb.seqlen++] = (int8_t)k;
    kb.lvel = (uint8_t)vel;
    kb.played = 1;
    kb.press++;
}
static void kb_release(int32_t k)
{
    uint32_t i, j;
    for (i = j = 0; i < kb.n; i++)
        if (kb.k[i] != k)
            kb.k[j] = kb.k[i], kb.side[j] = kb.side[i], kb.vel[j] = kb.vel[i], j++;
    kb.n = (uint8_t)j;
}

static void kb_tick(void)
{
    uint32_t i, mode = (uint32_t)pt.g[G_KEYS], arp = (uint32_t)pt.g[G_ARP];
    int32_t vib, vel = kb.n ? kb.vel[kb.n - 1u] : 64;
    kb.vph += LFO_B[pt.g[G_VIBR] & 127];            /* vibrato: deeper for a harder key */
    vib = (SINE[kb.vph >> 22] * pt.g[G_VIBD] * (32 + vel)) >> 20;
    if (kb.press != kb.seen) {                      /* a new key: both VCOs attack again */
        kb.seen = kb.press;
        if (!arp)
            kb.trig[0]++, kb.trig[1]++;
    }
    if (!arp) {
        int32_t a = 0, b = 0;
        int ga = 0, gb = 0;
        if (mode == KM_SPLIT) {                     /* the last key of each half */
            for (i = kb.n; i-- > 0u;) {
                if (!kb.side[i] && !ga)
                    a = kb_cents(kb.k[i]), ga = 1;
                if (kb.side[i] && !gb)
                    b = kb_cents(kb.k[i]), gb = 1;
            }
        } else if (kb.n) {
            a = b = kb_cents(kb.k[kb.n - 1u]);
            if (mode == KM_DUO && kb.n > 1u) {
                b = kb_cents(kb.k[kb.n - 2u]);
                if (b < a) {
                    int32_t t = a;
                    a = b, b = t;
                }
            }
            ga = gb = 1;
        }
        if (ga)
            kb.cents[0] = a;
        if (gb)
            kb.cents[1] = b;
        kb.gate[0] = (uint8_t)ga, kb.gate[1] = (uint8_t)gb;
    } else {                                        /* the arpeggiator */
        int8_t src[16];
        uint32_t n = 0, step = 0, oct = (uint32_t)clamp(pt.g[G_AOCT], 1, 3), tot;
        if (arp == AR_SEQ) {
            for (i = 0; i < kb.seqlen; i++)
                src[n++] = kb.seq[i];
        } else {
            const int8_t *ks = kb.n ? kb.k : kb.lk;
            uint32_t m = kb.n ? kb.n : (pt.g[G_ALATCH] ? kb.ln : 0u), x, y;
            for (i = 0; i < m && n < 8u; i++)
                src[n++] = ks[i];
            for (x = 1; x < n; x++)                 /* sorted, low to high */
                for (y = x; y > 0u && src[y - 1u] > src[y]; y--) {
                    int8_t t = src[y];
                    src[y] = src[y - 1u], src[y - 1u] = t;
                }
        }
        if (!n) {
            kb.gate[0] = kb.gate[1] = 0;
            kb.ph = 0x7FFFFFFFu;                    /* (the next key starts at once) */
            return;
        }
        if (pt.g[G_ARATE])
            step = (kb.ph += PULSE[pt.g[G_ARATE] & 127]) < PULSE[pt.g[G_ARATE] & 127];
        else
            step = (clk_edges >> 2) & 1u;           /* RATE 0: on the PULSER */
        tot = arp == AR_SEQ ? n : n * oct;
        if (step) {
            uint32_t s = kb.step;
            if (arp == AR_RANDOM)
                s = xorshift(&md.rnd) % tot;
            else if (arp == AR_DOWN)
                s = s == 0u || s >= tot ? tot - 1u : s - 1u;
            else if (arp == AR_UPDOWN) {
                if (tot < 2u)
                    s = 0;
                else {
                    if (s >= tot - 1u)
                        kb.dir = 1;
                    else if (s == 0u)
                        kb.dir = 0;
                    s = kb.dir ? s - 1u : s + 1u;
                }
            } else
                s = s + 1u >= tot ? 0u : s + 1u;
            kb.step = (uint8_t)(s % tot);
            {
                int32_t c = kb_cents(src[kb.step % n] + (arp == AR_SEQ ? 0 : 12 * (int32_t)(kb.step / n)));
                kb.cents[0] = kb.cents[1] = c;
            }
            kb.trig[0]++, kb.trig[1]++;
            kb.on = 1;
        }
        if (pt.g[G_ARATE] ? kb.ph > 0x9999999Au : md.pp > 0x9999999Au)   /* the gate: 60 % of a step */
            kb.on = 0;
        kb.gate[0] = kb.gate[1] = kb.on;
    }
    kb.vib = vib;
}

/* ------------------------------------------------------------- the global key --- */
/* SETUP LOW KEYS: the lowest keys set the key of the whole sound. Every drone keeps its chord and moves by the
 * same interval (its VOICE PITCH, as KB tunes a voice, within +-2 octaves); the tuning's ROOT moves, so the keys
 * play the scale (any set: maqam, gamelan too) on the new root. All glide there (kg, synth.c): each voice's
 * offset from its old pitch falls to 0 on an S-curve in cents, every voice at its own pace (93..100 % of the
 * glide: they drift in, the last one on time). The UI writes with interrupts off (KG_LOCK). */
#ifndef KG_LOCK
#define KG_LOCK() ((void)0)
#define KG_UNLOCK() ((void)0)
#endif
static const uint8_t KG_PACE[NV + 1] = {255, 238, 248, 255, 255, 242, 250, 245, 255};   /* of 255 */

/* the new key: root r (0..11); dir 0 the nearest way (up to a tritone), +1 up, -1 down (0..11 semitones);
 * len the glide in control ticks (0: at once). Returns the interval, semitones. */
static int32_t key_move(uint32_t r, int32_t dir, uint32_t len)
{
    int32_t old = pt.g[G_ROOT], d = ((int32_t)(r % 12u) - old + 12) % 12;
    uint32_t v;
    if (dir < 0)
        d = d ? d - 12 : 0;
    else if (!dir && d > 6)
        d -= 12;
    KG_LOCK();
    for (v = 0; v < NV; v++)
        if (VTYPE[v] != VT_VCO) {                   /* (the VCOs: from the keys, in vco_tick) */
            int32_t p0 = pt.v[v][VP_PITCH], p1 = p0 + d * 100;
            while (p1 > 2400)
                p1 -= 1200;
            while (p1 < -2400)
                p1 += 1200;
            pt.v[v][VP_PITCH] = (int16_t)p1;
            kg.add[v] = (int16_t)clamp(kg.add[v] + p0 - p1, -4800, 4800);
        }
    kg.add[NV] = (int16_t)clamp(kg.add[NV] + (old - (int32_t)(r % 12u)) * 100, -4800, 4800);   /* (RESO) */
    pt.g[G_ROOT] = (int16_t)(r % 12u);
    kg.len = (uint16_t)(len > 65535u ? 65535u : len);
    kg.ev++;
    KG_UNLOCK();
    return d;
}
static void key_clear(void)                         /* (a preset loads: its own key, no glide) */
{
    uint32_t v;
    KG_LOCK();
    for (v = 0; v <= NV; v++)
        kg.add[v] = 0;
    kg.clear++;
    KG_UNLOCK();
}
static int key_gliding(void) { return kg.ev != kg.seen || kg.prog < 256u; }

/* per tick, first: a new key's offsets, then each voice's offset on its S-curve (3x^2 - 2x^3) */
static void kg_tick(void)
{
    uint32_t v, slow = 0;
    kg.fresh = 0;
    if (kg.clear != kg.cleared) {
        kg.cleared = kg.clear;
        for (v = 0; v <= NV; v++)
            kg.from[v] = kg.off[v] = 0;
        kg.prog = 256;
    }
    if (kg.ev != kg.seen) {
        kg.seen = kg.ev;
        kg.fresh = 1;
        kg.t = 0;
        for (v = 0; v <= NV; v++) {
            kg.n[v] = (uint32_t)kg.len * KG_PACE[v] / 255u;
            if (v == NV || VTYPE[v] != VT_VCO)
                kg.from[v] = kg.off[v] = kg.off[v] + kg.add[v] * 16;
            kg.add[v] = 0;
        }
        kg.prog = 0;
        return;
    }
    if (kg.t < 0xFFFFFFu)
        kg.t++;
    for (v = 0; v <= NV; v++) {
        int32_t x, sc;
        slow = kg.n[v] > slow ? kg.n[v] : slow;
        if (kg.t >= kg.n[v]) {
            kg.from[v] = kg.off[v] = 0;
            continue;
        }
        x = (int32_t)((kg.t << 15) / kg.n[v]);     /* 0..32767 */
        sc = (int32_t)((int64_t)x * x * (3 * 32768 - 2 * x) >> 30);   /* 0..32768, rising */
        kg.off[v] = kg.from[v] - (int32_t)(((int64_t)kg.from[v] * sc) >> 15);
    }
    kg.prog = (uint16_t)(kg.t >= slow ? 256u : (kg.t << 8) / slow);
}
