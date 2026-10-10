/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
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
