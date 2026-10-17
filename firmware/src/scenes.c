/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) */
/* Settings and the 16 user presets (PRESETS, theme MINE) in the SMK data area (storage.c, owner SMK37DATAMAKKURO),
 * two presets per stored object. A record carries its parameter counts, so a later firmware with more parameters
 * still reads it; a scene of earlier versions (one per object) reads as the first of the pair. */
#define SET_MAGIC 0x4D4B5331u              /* "MKS1" */
#define USR_MAGIC 0x4D4B5532u              /* "MKU2": two presets */
#define OLD_MAGIC 0x4D4B4331u              /* "MKC1": one scene (earlier versions) */
#define NUSER 16u
typedef struct {
    uint32_t magic;
    uint8_t bright;                        /* pad LEDs 1..8 */
    uint8_t console;                       /* USB serial console (CDC) */
    uint8_t start;                         /* SETUP START: a preset's voices start (0 PLAY) or wait for their pads (1) */
    uint8_t user;                          /* the user preset loaded last (1..16, 0 = none) */
    uint8_t speaker;                       /* output for the little speaker (analog.c) */
    uint8_t hinted;                        /* the HOME hint "TAP A TOP PAD" seen (a top pad tapped once) */
    uint8_t lowkeys;                       /* SETUP LOW KEYS: 0 off, 1 / 2 the lowest 12 / 24 keys pick the key */
    uint8_t kglide;                        /* SETUP GLIDE: the key's glide in tenths of a second + 1, 0 = 6 s */
    uint8_t rsv[4];
} settings_t;
static settings_t settings = {SET_MAGIC, 4, 1, 0, 0, 0, 0, 0, 0, {0}};
static uint16_t user_used;                 /* bit n: user preset n + 1 is stored */

typedef struct {
    uint8_t nv, vpn, gn, used;
    patch_t p;
} urec_t;
static struct {
    uint32_t magic;
    urec_t r[2];
} uobj;                                    /* (RAM: the driver programs from RAM) */

static void settings_save(void)
{
    if (flash_ok)
        st_save(OBJ_SETTINGS, &settings, sizeof settings);
}

/* parameters out of range (a damaged or foreign record) are clamped */
static void patch_sanitize(patch_t *p)
{
    uint32_t v, k;
    for (v = 0; v < NV; v++)
        for (k = 0; k < VP_N; k++)
            p->v[v][k] = VDESC[VTYPE[v]][k].label ? vp_clamp(v, k, p->v[v][k]) : 0;
    for (k = 0; k < G_COUNT; k++)
        p->g[k] = (int16_t)clamp(p->g[k], GDESC[k].min, GDESC[k].max);
}

static int uobj_read(uint32_t o)           /* object o into uobj; 0 = there */
{
    int len = st_load(OBJ_SCENE0 + o, &uobj, sizeof uobj);
    if (len >= 8 && uobj.magic == OLD_MAGIC) {
        uobj.r[0].used = 1;                /* (its rsv byte, 0: the one scene it holds) */
        uobj.r[1].used = 0;
        return 0;
    }
    return len >= 8 && uobj.magic == USR_MAGIC ? 0 : -1;
}

static int user_save(uint32_t n)           /* the sound into user preset n (0..15); 0 = saved */
{
    urec_t *r;
    int rc;
    if (!flash_ok || n >= NUSER)
        return -1;
    if (uobj_read(n / 2u))
        memset(&uobj, 0, sizeof uobj);
    uobj.magic = USR_MAGIC;
    r = &uobj.r[n & 1u];
    r->nv = NV, r->vpn = VP_N, r->gn = G_COUNT, r->used = 1;
    r->p = pt;
    rc = st_save(OBJ_SCENE0 + n / 2u, &uobj, sizeof uobj);
    if (!rc)
        user_used |= (uint16_t)(1u << n);
    return rc;
}

static int user_load_to(uint32_t n, patch_t *dst)   /* 0 = loaded */
{
    static patch_t tmp;
    const urec_t *r = &uobj.r[n & 1u];
    uint32_t v, k;
    if (!flash_ok || n >= NUSER || uobj_read(n / 2u) || !r->used || r->nv != NV || r->vpn > VP_N || r->gn > G_COUNT)
        return -2;
    patch_init(&tmp);                       /* parameters the record does not have keep their defaults */
    if (r->vpn == VP_N) {
        for (v = 0; v < NV; v++)
            for (k = 0; k < VP_N; k++)
                tmp.v[v][k] = r->p.v[v][k];
        for (k = 0; k < G_COUNT && k < r->gn; k++)
            tmp.g[k] = r->p.g[k];
    } else {                                /* an older layout: the voice rows sit closer together */
        const int16_t *q = &r->p.v[0][0];
        for (v = 0; v < NV; v++)
            for (k = 0; k < r->vpn; k++)
                tmp.v[v][k] = q[v * r->vpn + k];
        q += NV * r->vpn;
        for (k = 0; k < r->gn; k++)
            tmp.g[k] = q[k];
    }
    patch_sanitize(&tmp);
    *dst = tmp;
    return 0;
}

static void persist_boot(void)
{
    settings_t s;
    uint32_t o, f = irq_save();
    flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());
    irq_restore(f);
    if (!flash_ok)
        return;
    st_owner_check();                       /* another firmware's store: cleared (storage.c) */
    if (st_load(OBJ_SETTINGS, &s, sizeof s) == (int)sizeof s && s.magic == SET_MAGIC) {
        settings = s;
        if (settings.bright < 1u || settings.bright > 8u)
            settings.bright = 4;
        if (settings.user > NUSER)
            settings.user = 0;
        settings.start = settings.start == 1u;
        settings.hinted = settings.hinted == 1u;
        if (settings.lowkeys > 2u)
            settings.lowkeys = 0;
        if (settings.kglide > 81u)
            settings.kglide = 0;
    }
    for (o = 0; o < NUSER / 2u; o++)
        if (!uobj_read(o))
            user_used |= (uint16_t)((uobj.r[0].used ? 1u : 0u) << (2u * o) | (uobj.r[1].used ? 2u : 0u) << (2u * o));
}
