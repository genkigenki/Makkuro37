/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Tuning sets: cents of each degree above the root. Sets of 12 map the keys as a piano (maqam Rast: E and B
 * a quarter tone flat); the others give every key the next degree. */
typedef struct {
    const char *name;
    uint8_t n;
    int16_t c[24];
} tset_t;
static const tset_t TSETS[TS_N] = {
    {"12-TET", 12, {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100}},
    {"JUST", 12, {0, 112, 204, 316, 386, 498, 590, 702, 814, 884, 1018, 1088}},
    {"PYTHAG", 12, {0, 90, 204, 294, 408, 498, 612, 702, 792, 906, 996, 1110}},
    {"24-TET", 24, {0, 50, 100, 150, 200, 250, 300, 350, 400, 450, 500, 550,
                    600, 650, 700, 750, 800, 850, 900, 950, 1000, 1050, 1100, 1150}},
    {"RAST", 12, {0, 100, 200, 300, 350, 500, 600, 700, 800, 900, 1000, 1050}},
    {"SLENDRO", 5, {0, 231, 474, 717, 955}},
    {"PELOG", 7, {0, 120, 270, 540, 670, 785, 950}},
};
static const char *const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static int32_t fdiv(int32_t a, int32_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }   /* floor */

/* cents (above MIDI 0) of degree d counted from the root in octave 0 (d may be negative) */
static int32_t tun_degree(uint32_t set, uint32_t root, int32_t d)
{
    const tset_t *t = &TSETS[set % TS_N];
    int32_t o = fdiv(d, t->n);
    return (int32_t)root * 100 + o * 1200 + t->c[d - o * t->n];
}

/* a key (semitones above C of octave 0 for 12-sets, degrees above the root otherwise) -> cents */
static int32_t tun_key(uint32_t set, uint32_t root, int32_t k)
{
    const tset_t *t = &TSETS[set % TS_N];
    if (t->n == 12u)
        return tun_degree(set, root, k - (int32_t)root);
    return tun_degree(set, root, k);
}

/* the scale pitch next to c in direction dir (+1 / -1), strictly beyond c */
static int32_t tun_next(uint32_t set, uint32_t root, int32_t c, int32_t dir)
{
    const tset_t *t = &TSETS[set % TS_N];
    int32_t d = fdiv(c - (int32_t)root * 100, 1200) * t->n - 1, p;
    for (;; d++) {                       /* the first degree above c */
        p = tun_degree(set, root, d);
        if (p > c)
            break;
    }
    if (dir > 0)
        return p;
    while ((p = tun_degree(set, root, --d)) >= c)
        ;
    return p;
}
