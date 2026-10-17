/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* The user presets (scenes.c) and the settings on a simulated NOR: a sound with the played sounds (VOICE,
 * BOWED, GLASS) and a key picked by LOW KEYS comes back as it was saved; SETUP LOW KEYS / GLIDE come back; a
 * store of an earlier version reads with the defaults; nothing outside FL_DATA is written. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fm1_flash_map.h"
#include "mk_tables.h"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (!FL_STORE_OK(off, 0x1000u))
        return -8;
    memset(nor + off, 0xFF, 0x1000u);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    if (!FL_STORE_OK(off, n))
        return -8;
    for (i = 0; i < n; i++)
        nor[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
#define FELUCCA_DATA_FAMILY "MAKKURO"
#define FELUCCA_DATA_UNTAGGED_OK 0
#include "../firmware/src/storage.c"
#include "../firmware/src/synth.h"
#include "../firmware/src/tuning.c"
#include "../firmware/src/synth.c"
#include "../firmware/src/analog.c"
#include "../firmware/src/fx.c"
#include "../firmware/src/keys.c"
#include "../firmware/src/mix.c"
#include "../firmware/src/params.c"
#include "../firmware/src/presets.c"
/* scenes.c: the board bits persist_boot uses */
static uint8_t flash_ok = 1;
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
#define FL_FAR(fn) fn
#include "../firmware/src/scenes.c"

static int bad;
static void check(const char *what, int ok) { printf("presets: %-60s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static int outside_kept(void)
{
    uint32_t i;
    for (i = 0; i < sizeof nor; i++)
        if ((i < FL_DATA_LO || i >= FL_DATA_HI) && nor[i] != (uint8_t)(i * 7u))
            return 0;
    return 1;
}

int main(void)
{
    static patch_t saved, back;
    uint32_t i, k;
    for (i = 0; i < sizeof nor; i++)
        nor[i] = (uint8_t)(i * 7u);                 /* (a pattern: what is outside FL_DATA must stay) */
    for (i = FL_DATA_LO; i < FL_DATA_HI; i++)
        nor[i] = 0xFF;
    persist_boot();
    check("an empty store: LOW KEYS off, GLIDE default (stored 0)", settings.lowkeys == 0u && settings.kglide == 0u);
    for (k = 0; k < 3u; k++) {                      /* each played sound in a MINE slot */
        preset_make(k * 17u, &pt);
        pt.v[3][VC_WAVE] = (int16_t)(W_VOICE + k);
        pt.v[4][VC_WAVE] = (int16_t)(W_GLASS - k);
        pt.v[3][VC_SHAPE] = (int16_t)(20 + 40 * k);
        pt.g[G_TSET] = (int16_t)(k == 0u ? TS_RAST : k == 1u ? TS_SLENDRO : TS_PELOG);
        key_move(2u + 3u * k, k == 2u ? -1 : 0, 0);  /* a key picked: the drones' VOICE PITCH, the ROOT */
        saved = pt;
        check("save MINE (a played sound, a picked key)", user_save(5u + k) == 0);
        preset_make(0, &pt);
        check("load it back", user_load_to(5u + k, &back) == 0);
        check("  the same sound, parameter for parameter", !memcmp(&back, &saved, sizeof back));
        check("  its waves and key", back.v[3][VC_WAVE] == (int16_t)(W_VOICE + k) &&
                                       back.v[4][VC_WAVE] == (int16_t)(W_GLASS - k) &&
                                       back.g[G_ROOT] == (int16_t)(2u + 3u * k));
    }
    settings.lowkeys = 2;
    settings.kglide = 1 + 35;                       /* 3.5 s */
    settings_save();
    memset(&settings, 0, sizeof settings);
    persist_boot();
    check("settings: LOW KEYS 24 and GLIDE 3.5 s come back", settings.lowkeys == 2u && settings.kglide == 36u);
    settings.lowkeys = 7, settings.kglide = 200;    /* a damaged record: the defaults */
    settings_save();
    persist_boot();
    check("settings out of range: LOW KEYS off, GLIDE default", settings.lowkeys == 0u && settings.kglide == 0u);
    check("nothing outside FL_DATA written", outside_kept());
    return bad ? 1 : 0;
}
