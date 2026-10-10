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
#include "../firmware/src/keys.c"
#include "../firmware/src/mix.c"
#include "../firmware/src/params.c"
#include "../firmware/src/presets.c"
/* scenes.c stand-ins */
#define NSCENE 8u
static uint8_t scene_used = 5;
static struct { uint8_t bright, console, boot_scene, scene; } settings = {4, 1, 0, 0};
static int scene_save(uint32_t n) { (void)n; return 0; }
static int scene_load(uint32_t n) { (void)n; return 0; }
static int scene_load_to(uint32_t n, patch_t *d) { (void)n; patch_init(d); d->g[G_FFL] = 40; return 0; }
static void fm1_wdt_feed(void) {}
static void settings_save(void) {}
static const char *shot_dir;
static void shot(const char *dir, const char *name);
static void fm1_delay_ms(uint32_t ms) { fm1_ms += ms; }
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

/* run the engine for ms milliseconds with the UI's gates, the main loop every 15 ms, then draw */
static void run(uint32_t ms)
{
    int32_t out[2 * CTL];
    uint32_t b, nb = ms * 44100 / 1000 / CTL, acc = 0;
    for (b = 0; b < nb; b++) {
        synth_block(out, CTL);
        acc += 726;                                     /* (32 samples = 0.726 ms) */
        while (acc >= 1000) {
            acc -= 1000;
            fm1_ms++;
            if (fm1_ms % 15u == 0u) {
                fm1_in.frames += 10;
                ui_input();
                ui_leds();
                draw_frame();
            }
        }
    }
}

static void page(const char *dir, const char *name, uint32_t menu, uint32_t pg)
{
    ui.menu = (uint8_t)menu;
    ui.page = (uint8_t)pg;
    big.until = 0;
    ui.msg_until = 0;
    run(300);
    shot(dir, name);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    uint32_t t;
    shot_dir = dir;
    palette_set(2);
    preset_make(0, &pt);
    ui_init();
    fm1_in.frames = 400;
    for (t = 0; t <= 2000u; t += 100u) {               /* the boot scene, frame by frame */
        char n[32];
        boot_frame(t);
        snprintf(n, sizeof n, "boot_%04u", t);
        shot(dir, n);
    }
    lcd_fill(0, 0, 240, 240, C_BG);
    run(200);
    shot(dir, "s01_presets");
    ui.pre_sel = 1;
    preset_load(1);
    ui.menu = MN_HOME;
    run(2500);
    shot(dir, "s02_home");
    pitch_turn(2, 3);
    run(100);
    shot(dir, "s03_home_pitch");
    page(dir, "s04_voice1", MN_VOICE, 0);
    page(dir, "s05_voice3", MN_VOICE, 2);
    page(dir, "s06_voice4", MN_VOICE, 3);
    page(dir, "s07_env_attack", MN_ENV, 0);
    page(dir, "s08_env_gate", MN_ENV, 2);
    page(dir, "s09_mix_pan", MN_MIX, 0);
    page(dir, "s10_volume", MN_VOLUME, 0);
    page(dir, "s11_fx_filter", MN_FX, 0);
    page(dir, "s12_fx_lr", MN_FX, 1);
    page(dir, "s13_keys", MN_KEYS, 0);
    page(dir, "s14_seq", MN_SEQ, 0);
    page(dir, "s15_tuning", MN_TUNING, 0);
    page(dir, "s16_presets_save", MN_PRESETS, 2);
    ui.menu = MN_HOME;
    dlg_open(DLG_PRESET, 3);
    run(200);
    shot(dir, "s17_dialog");
    ui.dlg = 0;
    ui.help = 1;
    for (t = 0; t < 6u; t++) {
        char n[16];
        ui.help_panel = (uint8_t)t;
        ui.help_t = fm1_ms;
        fm1_ms += 300;
        draw_frame();
        snprintf(n, sizeof n, "s2%u_help", t);
        shot(dir, n);
    }
    return 0;
}
