/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Host render of the screen (the firmware's ui.c / draw.c on a 240 x 240 frame buffer) to PPM files.
 *   tests/screen OUTDIR      (tests/run_tests.sh converts them to PNG) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define memset mk_memset
#define memcpy mk_memcpy
#define memcmp mk_memcmp
#include "../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
#include "mk_tables.h"
#define BRAND_NAME "MAKKURO37"
#define MK_VERSION "host"
static uint32_t fm1_ms;
/* the HAL bits ui.c uses */
#define FM1_NENC 8u
#define FM1_NBTN 18u
#define FM1_OCT_BOTH 0x30u
static struct { uint8_t fader[4]; uint32_t frames, buttons; uint64_t keys; uint16_t wheel[2]; uint8_t key_vel[37]; } fm1_in;
static int32_t smk_pad_take(void) { return -1; }
static uint32_t fm1_input_edges(uint32_t *r) { *r = 0; return 0; }
static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_led_key(uint32_t id, int on) { (void)id, (void)on; }
static uint32_t padled[16];
static void padled_set(uint32_t p, uint8_t r, uint8_t g, uint8_t b) { padled[p] = (uint32_t)r << 16 | g << 8 | b; }
#define MQ 64u
static uint32_t midi_in_q[MQ], mi_w, mi_r;
#define RING_PUBLISH() ((void)0)
/* the panel */
static uint16_t fb[240 * 240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240 && y + j < 240) {
                uint16_t c = px[j * w + i];
                fb[(y + j) * 240 + x + i] = (uint16_t)(c >> 8 | c << 8);
            }
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = y; j < y + h && j < 240; j++)
        for (i = x; i < x + w && i < 240; i++)
            fb[j * 240 + i] = c;
}
#include "../firmware/src/gfx.c"
#include "../firmware/src/synth.h"
#include "../firmware/src/tuning.c"
#include "../firmware/src/synth.c"
#include "../firmware/src/analog.c"
#include "../firmware/src/fx.c"
#include "../firmware/src/mix.c"
#include "../firmware/src/params.c"
/* scenes.c stand-ins */
#define NSCENE 8u
static uint8_t scene_used = 5;
static struct { uint8_t bright, console, boot_scene, scene; } settings = {4, 1, 0, 0};
static int scene_save(uint32_t n) { (void)n; return 0; }
static int scene_load(uint32_t n) { (void)n; return 0; }
static int scene_load_to(uint32_t n, patch_t *d) { (void)n; patch_init(d); d->g[G_FFL] = 40; return 0; }
static void fm1_wdt_feed(void) {}
static const char *shot_dir;
static uint32_t boot_shot_at[3] = {500, 950, 1900}, boot_shots;
static void shot(const char *dir, const char *name);
static void fm1_delay_ms(uint32_t ms)
{
    fm1_ms += ms;
    if (boot_shots < 3 && fm1_ms >= boot_shot_at[boot_shots]) {
        char n[32];
        snprintf(n, sizeof n, "boot_%u", boot_shots++);
        shot(shot_dir, n);
    }
}
static void settings_save(void) {}
#include "../firmware/src/ui.c"
#include "../firmware/src/draw.c"

static void shot(const char *dir, const char *name)
{
    char path[512];
    FILE *f;
    uint32_t i;
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240 * 240; i++) {
        uint16_t c = fb[i];
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3 | (c >> 13)), (uint8_t)(((c >> 5) & 63) << 2 | ((c >> 9) & 3)),
                          (uint8_t)((c & 31) << 3 | ((c >> 2) & 7))};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("  screen %s\n", path);
}

/* run the engine for ms milliseconds with the gates as set, then draw */
static void run(uint32_t ms)
{
    int32_t out[2 * CTL];
    uint32_t b, nb = ms * 44100 / 1000 / CTL;
    for (b = 0; b < nb; b++) {
        uint32_t v;
        for (v = 0; v < NV; v++)
            gate_in[v] = ui.pad_gate[v];
        synth_block(out, CTL);
        if (b % 44 == 0) {
            fm1_ms += 32;
            draw_frame();
        }
    }
    ui.dirty = 1;
    fm1_ms += 300;
    draw_frame();
}

static void page(const char *dir, const char *name, uint32_t pg, uint32_t sub, uint32_t voice)
{
    ui.page = (uint8_t)pg;
    ui.sub = (uint8_t)sub;
    ui.voice = (uint8_t)voice;
    big.until = 0;
    run(200);
    shot(dir, name);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    shot_dir = dir;
    palette_set(2);
    patch_init(&pt);
    ui_init();
    boot_anim();
    lcd_fill(0, 0, 240, 240, C_BG);
    ui.pad_gate[0] = ui.latched[0] = 1;
    ui.pad_gate[2] = ui.latched[2] = 1;
    ui.pad_gate[5] = 1;
    run(1200);
    vs[5].hits++;                                   /* a bonk in progress on voice 6 */
    ui.pad_gate[6] = 1;
    run(110);
    shot(dir, "screen_main");
    pt.g[G_RUN] = 1;
    page(dir, "page_voice_classic", PG_VOICE, 0, 0);
    page(dir, "page_mutemod", PG_MUTEMOD, 1, 0);
    page(dir, "page_voice_noise", PG_VOICE, 0, 2);
    page(dir, "page_voice_vco", PG_VOICE, 0, 3);
    page(dir, "page_env_vco", PG_ENV, 0, 3);
    page(dir, "page_env", PG_ENV, 0, 0);
    page(dir, "page_mix", PG_MIX, 0, 0);
    page(dir, "page_tuning", PG_TUNING, 0, 0);
    page(dir, "page_lfo", PG_LFO, 0, 0);
    page(dir, "page_seq", PG_SEQ, 0, 0);
    page(dir, "page_filter", PG_FILTER, 0, 0);
    page(dir, "page_fx", PG_FX, 0, 0);
    page(dir, "page_scenes", PG_SCENES, 0, 0);
    ui.page = PG_VOICE, ui.sub = 0, ui.voice = 0;
    page_build();
    big_show(1, "TUNE 2", "G2+12");
    run(100);
    shot(dir, "overlay_value");
    return 0;
}
