/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Settings and the 8 scenes in the SMK data area (storage.c, owner SMK37DATAMAKKURO). A stored scene
 * carries its parameter counts, so a later firmware with more parameters still reads it. */
#define SET_MAGIC 0x4D4B5331u              /* "MKS1" */
#define SCN_MAGIC 0x4D4B4331u              /* "MKC1" */
#define NSCENE 8u
typedef struct {
    uint32_t magic;
    uint8_t bright;                        /* pad LEDs 1..8 */
    uint8_t console;                       /* USB serial console (CDC) */
    uint8_t boot_scene;                    /* 0 = the power-on sound, 1..8 = that scene */
    uint8_t scene;                         /* the scene loaded last (1..8, 0 = none) */
    uint8_t rsv[8];
} settings_t;
static settings_t settings = {SET_MAGIC, 4, 1, 0, 0, {0}};
static uint8_t scene_used;                 /* bit n: scene n + 1 is stored */

typedef struct {
    uint32_t magic;
    uint8_t nv, vpn, gn, rsv;
    patch_t p;
} scene_rec_t;
static scene_rec_t scn;                    /* (RAM: the driver programs from RAM) */

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
            if (VDESC[VTYPE[v]][k].label)
                p->v[v][k] = vp_clamp(v, k, p->v[v][k]);
            else
                p->v[v][k] = 0;
    for (k = 0; k < G_COUNT; k++)
        p->g[k] = (int16_t)clamp(p->g[k], GDESC[k].min, GDESC[k].max);
}

static int scene_save(uint32_t n)          /* 0..7; 0 = saved */
{
    int rc;
    if (!flash_ok || n >= NSCENE)
        return -1;
    scn.magic = SCN_MAGIC;
    scn.nv = NV;
    scn.vpn = VP_N;
    scn.gn = G_COUNT;
    scn.rsv = 0;
    scn.p = pt;
    rc = st_save(OBJ_SCENE0 + n, &scn, sizeof scn);
    if (!rc)
        scene_used |= (uint8_t)(1u << n);
    return rc;
}

static int scene_load(uint32_t n)          /* 0 = loaded */
{
    static patch_t tmp;
    uint32_t v, k;
    int len;
    if (!flash_ok || n >= NSCENE)
        return -1;
    len = st_load(OBJ_SCENE0 + n, &scn, sizeof scn);
    if (len < 8 || scn.magic != SCN_MAGIC || scn.nv != NV || scn.vpn > VP_N || scn.gn > G_COUNT)
        return -2;
    patch_init(&tmp);                       /* parameters the record does not have keep their defaults */
    for (v = 0; v < NV; v++)
        for (k = 0; k < VP_N && k < scn.vpn; k++)
            tmp.v[v][k] = scn.p.v[v][k];
    for (k = 0; k < G_COUNT && k < scn.gn; k++)
        tmp.g[k] = scn.p.g[k];
    if (scn.vpn != VP_N) {                  /* an older layout: the voice rows sit closer together */
        const int16_t *r = &scn.p.v[0][0];
        for (v = 0; v < NV; v++)
            for (k = 0; k < VP_N && k < scn.vpn; k++)
                tmp.v[v][k] = r[v * scn.vpn + k];
        r += NV * scn.vpn;
        for (k = 0; k < G_COUNT && k < scn.gn; k++)
            tmp.g[k] = r[k];
    }
    patch_sanitize(&tmp);
    pt = tmp;
    return 0;
}

static void persist_boot(void)
{
    settings_t s;
    uint32_t n, f = irq_save();
    flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());
    irq_restore(f);
    if (!flash_ok)
        return;
    st_owner_check();                       /* another firmware's store: cleared (storage.c) */
    if (st_load(OBJ_SETTINGS, &s, sizeof s) == (int)sizeof s && s.magic == SET_MAGIC) {
        settings = s;
        if (settings.bright < 1u || settings.bright > 8u)
            settings.bright = 4;
        if (settings.boot_scene > NSCENE)
            settings.boot_scene = 0;
    }
    for (n = 0; n < NSCENE; n++) {
        st_hdr_t h;
        if (st_current(OBJ_SCENE0 + n, &h) >= 0)
            scene_used |= (uint8_t)(1u << n);
    }
}
