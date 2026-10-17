/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) */
/* Makkuro37 in the browser: the firmware's engine, UI and screen (the same sources) against a simulated
 * SMK-37 Pro, compiled to WebAssembly (web/sim/build.sh). The page's AudioWorklet calls sim_render(n): the device
 * clock runs with the audio, the main loop (input, LEDs, screen) every 15 ms of it. */
#include <stdint.h>
#include <string.h>
#define EXPORT __attribute__((used, visibility("default")))
#define memset mk_memset
#define memcpy mk_memcpy
#define memcmp mk_memcmp
#include "../../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
#include "mk_tables.h"
#define BRAND_NAME "MAKKURO37"
#define MK_VERSION "sim"
static uint32_t fm1_ms;

/* ---- the simulated panel ---- */
#define FM1_NENC 8u
#define FM1_NBTN 18u
#define FM1_OCT_BOTH 0x30u
static struct { uint8_t fader[4]; uint32_t frames, buttons; uint64_t keys; uint16_t wheel[2]; uint8_t key_vel[37]; } fm1_in;
static uint32_t btn_pressed, btn_released;
static int16_t enc_steps[8];
#define PADQ 64u
static uint16_t padq[PADQ];
static uint32_t padq_w, padq_r;
static int32_t smk_pad_take(void) { return padq_r == padq_w ? -1 : padq[padq_r++ % PADQ]; }
static uint32_t fm1_input_edges(uint32_t *r)
{
    uint32_t p = btn_pressed;
    *r = btn_released;
    btn_pressed = btn_released = 0;
    return p;
}
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = enc_steps[e & 7u];
    enc_steps[e & 7u] = 0;
    return s;
}
static uint32_t btn_leds;
static void fm1_led_key(uint32_t id, int on)
{
    if (id < 32u)
        btn_leds = on ? btn_leds | 1u << id : btn_leds & ~(1u << id);
}
static uint32_t pad_rgb[16];
static void padled_set(uint32_t p, uint8_t r, uint8_t g, uint8_t b)
{
    if (p < 16u)
        pad_rgb[p] = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
}
static void fm1_wdt_feed(void) {}
static void fm1_delay_ms(uint32_t ms) { fm1_ms += ms; }
#define MQ 64u
static uint32_t midi_in_q[MQ], mi_w, mi_r;
#define RING_PUBLISH() ((void)0)

/* ---- the panel's screen: RGB565 ---- */
static uint16_t fb[240 * 240];
static uint32_t fb_draws;
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) {
                uint16_t c = px[j * w + i];
                fb[(y + j) * 240u + x + i] = (uint16_t)(c >> 8 | c << 8);
            }
    fb_draws++;
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = c;
    fb_draws++;
}

#include "../../firmware/src/gfx.c"
#include "../../firmware/src/synth.h"
#include "../../firmware/src/tuning.c"
#include "../../firmware/src/synth.c"
#include "../../firmware/src/analog.c"
#include "../../firmware/src/fx.c"
#include "../../firmware/src/keys.c"
#include "../../firmware/src/mix.c"
#include "../../firmware/src/params.c"
#include "../../firmware/src/presets.c"

/* ---- the user presets (MINE) in RAM (this session) ---- */
#define NUSER 16u
static uint16_t user_used;
static struct { uint8_t bright, console, start, user, speaker, hinted, lowkeys, kglide; } settings = {4, 0, 0, 0, 0, 0, 0, 0};
static patch_t users[NUSER];
static int user_save(uint32_t n)
{
    if (n >= NUSER) return -1;
    users[n] = pt;
    user_used |= (uint16_t)(1u << n);
    return 0;
}
static int user_load_to(uint32_t n, patch_t *d)
{
    if (n >= NUSER || !((user_used >> n) & 1u)) return -2;
    *d = users[n];
    return 0;
}
static void settings_save(void) {}

#include "../../firmware/src/ui.c"
#include "../../firmware/src/draw.c"

/* ---- the device's clock ---- */
static float out_l[1024], out_r[1024];
static int32_t blk[2 * CTL];
static uint32_t blk_pos = CTL, sub_us, loop_ms;

static void main_step(void)                     /* one pass of the firmware's main loop */
{
    fm1_in.frames += 10u;
    ui_input();
    midi_poll();
    ui_leds();
    draw_frame();
}

EXPORT void sim_init(void)
{
    uint32_t i;
    palette_set(2);
    patch_init(&pt);
    ui_init();
    fm1_in.wheel[0] = 540;                       /* (the wheels at rest) */
    fm1_in.wheel[1] = 615;
    for (i = 0; i < 4u; i++)                     /* (as the page's faders start) */
        fm1_in.fader[i] = (uint8_t)(i == 0u ? 112 : i == 1u ? 16 : i == 2u ? 0 : 60);
    lcd_fill(0, 0, 240, 240, C_BG);
    main_step();
}
EXPORT float *sim_out_l(void) { return out_l; }
EXPORT float *sim_out_r(void) { return out_r; }
EXPORT uint32_t sim_rate(void) { return FS; }
EXPORT void sim_render(uint32_t n)               /* n frames (<= 1024) */
{
    uint32_t i;
    for (i = 0; i < n && i < 1024u; i++) {
        if (blk_pos >= CTL) {
            synth_block(blk, CTL);
            blk_pos = 0;
        }
        out_l[i] = (float)blk[2u * blk_pos] * (1.0f / 32768.0f);
        out_r[i] = (float)blk[2u * blk_pos + 1u] * (1.0f / 32768.0f);
        blk_pos++;
        if ((sub_us += 22676u) >= 1000000u) {    /* 1 / 44100 s in ns (x 1000) */
            sub_us -= 1000000u;
            fm1_ms++;
            if (++loop_ms >= 15u) {
                loop_ms = 0;
                main_step();
            }
        }
    }
}
EXPORT uint16_t *sim_fb(void) { return fb; }
EXPORT uint32_t sim_draws(void) { return fb_draws; }
EXPORT uint32_t *sim_pads(void) { return pad_rgb; }
EXPORT uint32_t sim_leds(void) { return btn_leds; }
EXPORT uint32_t sim_ui(void) { return (uint32_t)ui.menu << 16 | (uint32_t)ui.page << 8 | ui.preset; }   /* (tests) */
EXPORT uint32_t sim_user_used(void) { return user_used; }
EXPORT uint32_t sim_lowkeys(void) { return settings.lowkeys; }   /* (the page: the key picker's keys) */
EXPORT uint32_t sim_key_state(void)              /* (tests) the root, the key before, the glide 0..256 */
{
    return (uint32_t)pt.g[G_ROOT] | (uint32_t)ui.key_from << 8 | (uint32_t)(kg.prog > 256u ? 256u : kg.prog) << 16;
}
EXPORT int32_t sim_vparam(uint32_t v, uint32_t id) { return v < NV && id < VP_N ? pt.v[v][id] : 0; }   /* (tests) */
EXPORT uint32_t sim_cpu_peak(void) { return (uint32_t)syn.peak; }
EXPORT void sim_pad(uint32_t pad, uint32_t vel)  /* vel 0 = release */
{
    if (pad < 16u)
        padq[padq_w++ % PADQ] = (uint16_t)(pad << 8 | (vel > 127u ? 127u : vel));
}
EXPORT void sim_key(uint32_t k, uint32_t down)   /* keybed key 0..36; down: 0 = release, 1 = 100, 2..127 its velocity */
{
    if (k >= 37u) return;
    if (down) {
        fm1_in.key_vel[k] = (uint8_t)(down > 127u ? 127u : down > 1u ? down : 100u);
        fm1_in.keys |= (uint64_t)1u << k;
    } else
        fm1_in.keys &= ~((uint64_t)1u << k);
}
EXPORT void sim_button(uint32_t b, uint32_t down)
{
    if (b >= FM1_NBTN) return;
    if (down) {
        fm1_in.buttons |= 1u << b;
        btn_pressed |= 1u << b;
    } else {
        fm1_in.buttons &= ~(1u << b);
        btn_released |= 1u << b;
    }
}
EXPORT void sim_enc(uint32_t e, int32_t steps) { enc_steps[e & 7u] = (int16_t)(enc_steps[e & 7u] + steps); }
EXPORT void sim_fader(uint32_t i, uint32_t v) { if (i < 4u) fm1_in.fader[i] = (uint8_t)(v > 127u ? 127u : v); }
EXPORT void sim_wheel(uint32_t i, uint32_t raw) { if (i < 2u) fm1_in.wheel[i] = (uint16_t)raw; }
EXPORT void sim_midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t cin = st >> 4;
    if (cin == 8u || cin == 9u)
        midi_in_q[mi_w++ % MQ] = cin | st << 8 | d1 << 16 | d2 << 24;
}
