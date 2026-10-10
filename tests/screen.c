/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* Host render of the screen (the firmware's ui.c / draw.c on a 240 x 240 frame buffer) to PPM files.
 *   tests/screen OUTDIR        (tests/run_tests.sh converts them to PNG)
 *   tests/screen OUTDIR docs   every screen, named for docs/img (tools/docs_img.sh) */
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
/* scenes.c stand-ins: user presets 1 and 3 stored */
#define NUSER 16u
static uint16_t user_used = 5;
static struct { uint8_t bright, console, rsv0, user, speaker; } settings = {4, 1, 0, 0, 0};
static int user_save(uint32_t n) { user_used |= (uint16_t)(1u << n); return 0; }
static int user_load_to(uint32_t n, patch_t *d) { (void)n; patch_init(d); return 0; }
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

/* the control map from the table (ui.c): docs/CONTROLS.md */
static void controls_md(const char *path)
{
    FILE *f = fopen(path, "w");
    uint32_t m, p, k;
    if (!f) { perror(path); exit(1); }
    fprintf(f, "# Makkuro37: control map\n\nGenerated from the firmware's menu table (tools/docs_img.sh). "
               "On every page knob Ki sets column i; a value with two choices sits on the column's pads.\n\n"
               "| Button | Menu | Page | K1 | K2 | K3 | K4 | K5 | K6 | K7 | K8 |\n"
               "|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (m = MN_SOUND; m < MN_N; m++) {
        if (!MENUS[m].name)
            continue;
        for (p = 0; p < MENUS[m].n; p++) {
            ui.menu = (uint8_t)m, ui.page = (uint8_t)p, ui.altmask = 0;
            menu_build();
            fprintf(f, "| %s | %s | %s |", p ? "" : MENUS[m].button, p ? "" : MENUS[m].name, page_cur()->name);
            for (k = 0; k < 8u; k++) {
                char b[16] = "";
                if (ui.it[k].kind != IK_NONE)
                    it_label(&ui.it[k], b, sizeof b);
                fprintf(f, " %s%s%s%s |", b, ui.it[k].kind != IK_NONE && it_onpads(&ui.it[k]) ? " (pads)" : "",
                        ui.it[k].alt != NO ? " (pad: RATE)" : "",
                        ui.it[k].pid == CL_MUTE ? " (pads: on, mod)" : ui.it[k].pid == G_SGATES ? " (pad: on)" : "");
            }
            fprintf(f, "\n");
        }
    }
    fprintf(f, "\nFADER BANK: the banks (< > or top pads 1-%u). A fader sets its parameter when it moves.\n\n"
               "| Pad | Bank | F1 | F2 | F3 | F4 |\n|---|---|---|---|---|---|\n", (unsigned)NFBANK);
    for (m = 0; m < NFBANK; m++)
        fprintf(f, "| %u | %s | %s | %s | %s | %s |\n", m + 1, FBANK[m].name, FBANK[m].f[0].name, FBANK[m].f[1].name,
                FBANK[m].f[2].name, FBANK[m].f[3].name);
    fclose(f);
    printf("  controls %s\n", path);
}

static void docs(const char *dir)                       /* every screen, for the guide */
{
    uint32_t m, pg, t;
    char n[48];
    fm1_in.fader[0] = 112, fm1_in.fader[1] = 16, fm1_in.fader[2] = 0, fm1_in.fader[3] = 60;
    boot_frame(2000);
    shot(dir, "boot");
    lcd_fill(0, 0, 240, 240, C_BG);
    for (t = 0; t <= NTHEME; t++) {                    /* the picker, each theme (and MINE) */
        ui.menu = MN_PRESETS;
        ui.preset = (uint8_t)(t * PRESET_PAGE + (t < NTHEME ? 4u : 0u));
        ui.pre_sel = ui.preset;
        run(200);
        snprintf(n, sizeof n, "presets-%u", t + 1);
        shot(dir, n);
    }
    preset_load(1);
    ui.menu = MN_HOME;
    run(2500);
    shot(dir, "home");
    pitch_turn(2, 3);
    run(100);
    shot(dir, "home-pitch");
    big.until = 0, ui.msg_until = 0;
    fm1_in.fader[1] += 20;                             /* a fader moves: its value, big */
    run(100);
    shot(dir, "fader");
    big.until = 0, ui.msg_until = 0;
    ui.menu = MN_FADERS;                               /* FADER BANK: each bank */
    for (t = 0; t < NFBANK; t++) {
        fbank_go(t);
        run(200);
        snprintf(n, sizeof n, "faders-%u", t + 1);
        shot(dir, n);
    }
    fbank_go(0);
    ui.menu = MN_HOME;
    run(200);
    free_play(1);                                      /* FREE-PLAY: keys wake the drones */
    run(400);
    shot(dir, "free-play");
    free_play(0);
    big.until = 0, ui.msg_until = 0;
    chaos(0, 8);                                       /* CHAOS: three notes each, the third kept */
    run(500);
    shot(dir, "chaos");
    big.until = 0, ui.msg_until = 0, ui.cz.until = 0;
    ui.kbm = 1;                                        /* KB: select voices, then keys */
    run(100);
    shot(dir, "kb");
    ui.nsel = 0x05;
    run(100);
    shot(dir, "kb-selected");
    ui.kbm = ui.nsel = 0;
    for (m = MN_SOUND; m < MN_N; m++)                  /* every page of every menu */
        for (pg = 0; MENUS[m].name && pg < MENUS[m].n; pg++) {
            const char *nm = MENUS[m].name;
            char low[16];
            uint32_t i;
            for (i = 0; nm[i] && i < 15u; i++)
                low[i] = (char)(nm[i] >= 'A' && nm[i] <= 'Z' ? nm[i] + 32 : nm[i]);
            low[i] = 0;
            snprintf(n, sizeof n, "menu-%s-%u", low, pg + 1);
            page(dir, n, m, pg);
        }
    ui.menu = MN_PRESETS;
    ui.pre_sel = ui.preset = NPRESET;
    dlg_open(DLG_SAVE, 0);
    run(200);
    shot(dir, "dialog-save");
    ui.dlg = 0;
    ui.help = 1;
    for (t = 0; t < HELP_N; t++) {
        ui.help_panel = (uint8_t)t;
        ui.help_t = fm1_ms;
        fm1_ms += 300;
        draw_frame();
        snprintf(n, sizeof n, "help-%u", t + 1);
        shot(dir, n);
    }
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
    if (argc > 3 && !strcmp(argv[2], "docs")) {
        docs(dir);
        controls_md(argv[3]);
        return 0;
    }
    for (t = 0; t <= 2000u; t += 100u) {               /* the boot scene, frame by frame */
        char n[32];
        boot_frame(t);
        snprintf(n, sizeof n, "boot_%04u", t);
        shot(dir, n);
    }
    lcd_fill(0, 0, 240, 240, C_BG);
    run(200);
    shot(dir, "s01_presets");
    preset_load(1);
    ui.menu = MN_HOME;
    run(2500);
    shot(dir, "s02_home");
    pitch_turn(2, 3);
    run(100);
    shot(dir, "s03_home_pitch");
    page(dir, "s04_wave", MN_SOUND, 0);
    page(dir, "s05_voice1", MN_SOUND, 1);
    page(dir, "s06_voice3", MN_SOUND, 3);
    page(dir, "s06a_voice4", MN_SOUND, 4);
    ui.altmask = 0x0F;
    page(dir, "s06b_detune", MN_MOTION, 0);
    ui.altmask = 0;
    page(dir, "s07_attack", MN_ENV, 0);
    page(dir, "s08_gate", MN_ENV, 2);
    page(dir, "s09_volume", MN_MIX, 0);
    page(dir, "s11_fx", MN_FX, 0);
    page(dir, "s12_filter", MN_FX, 1);
    page(dir, "s13_keys", MN_KEYS, 0);
    page(dir, "s14_seq", MN_SEQ, 0);
    page(dir, "s15_tuning", MN_TUNING, 0);
    page(dir, "s16_setup", MN_SETUP, 0);
    ui.menu = MN_PRESETS;
    ui.pre_sel = NPRESET;
    run(200);
    shot(dir, "s17_mine");
    dlg_open(DLG_SAVE, 0);
    run(200);
    shot(dir, "s18_dialog");
    ui.dlg = 0;
    ui.help = 1;
    for (t = 0; t < HELP_N; t++) {
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
