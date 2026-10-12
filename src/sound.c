// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] sound.c - Sound implementation

#include "sound.h"

#include <string.h>

#include "app_config.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/float.h"
#include "pico/critical_section.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"

typedef struct {
    const uint8_t *pcm;    // PCM data pointer (Flash address)
    size_t length;         // Total length in samples
    size_t position;       // Current playback position
    uint8_t volume;        // 0-255 fixed
    uint8_t start_vol;     // Initial volume at start
    uint8_t playing;       // 1: playing, 0: stopped
    bool loop;             // loop play
} sfx_channel_t;


static sfx_channel_t ch0, ch1, ch2, ch3;
static uint32_t pwm_slice;
static uint pwm_channel;
repeating_timer_t timer_handle;

static critical_section_t sound_crit;

// Timer alarm callback for 16kHz sample rate
// This is called every 62.5us (= 1000000us / 16000 samples)
static bool __not_in_flash_func(timer_callback)(repeating_timer_t* rt)
{
    (void)rt;

    int32_t acc = 0;
    int8_t cnt = 0;

    // Enter critical section
    critical_section_enter_blocking(&sound_crit);

    // ch0
    if (ch0.playing) {
        int16_t s = ((int16_t)ch0.pcm[ch0.position] - 128);
        acc += (s * ch0.volume) >> 8;
        if (++ch0.position >= ch0.length) {
            if (ch0.loop) {
                ch0.position = 0;
            }
            else {
                ch0.playing = 0;
            }
        }
        cnt++;
    }

    // ch1
    if (ch1.playing) {
        int16_t s = ((int16_t)ch1.pcm[ch1.position] - 128);
        acc += (s * ch1.volume) >> 8;
        if (++ch1.position >= ch1.length) {
            if (ch1.loop) {
                ch1.position = 0;
            }
            else {
                ch1.playing = 0;
            }
        }
        cnt++;
    }

    // ch2
    if (ch2.playing) {
        int16_t s = ((int16_t)ch2.pcm[ch2.position] - 128);
        acc += (s * ch2.volume) >> 8;
        if (++ch2.position >= ch2.length) {
            if (ch2.loop) {
                ch2.position = 0;
            }
            else {
                ch2.playing = 0;
            }
        }
        cnt++;
    }

    // ch3
    if (ch3.playing) {
        int16_t s = ((int16_t)ch3.pcm[ch3.position] - 128);
        acc += (s * ch3.volume) >> 8;
        if (++ch3.position >= ch3.length) {
            if (ch3.loop) {
                ch3.position = 0;
            }
            else {
                ch3.playing = 0;
            }
        }
        cnt++;
    }

    // Exit critical section
    critical_section_exit(&sound_crit);

    int32_t mixed = acc;
    if (acc >= 128 || acc <= -128) {
        switch (cnt) {
            case 2:
                mixed = acc >> 1;
                break;
            case 3:
                    mixed = (acc * 0x5556) >> 16;
                break;
            case 4:
                mixed = acc >> 2;
                break;
            default:
                mixed = acc;
                break;
        }        
    }

    int32_t out = mixed + 128;

    if (out > 255) out = 255;
    else if (out < 0) out = 0;

    pwm_set_chan_level(pwm_slice, pwm_channel, (uint16_t)out);
    return true;
}

void sound_init(void)
{
    // Initialize critical section
    critical_section_init(&sound_crit);

    memset(&ch0, 0, sizeof(ch0));
    memset(&ch1, 0, sizeof(ch1));
    memset(&ch2, 0, sizeof(ch2));
    memset(&ch3, 0, sizeof(ch3));

    ch0.volume = ch1.volume = ch2.volume = ch3.volume = 255;

    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    pwm_slice = pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_channel = pwm_gpio_to_channel(AUDIO_PIN);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 255);
    pwm_config_set_clkdiv(&cfg, 36.0f);
    pwm_init(pwm_slice, &cfg, true);

    pwm_set_chan_level(pwm_slice, pwm_channel, 128);

    add_repeating_timer_us(62, timer_callback, NULL, &timer_handle);
}

static sfx_channel_t* get_ch(int c)
{
    switch (c) {
        case 0: return &ch0;
        case 1: return &ch1;
        case 2: return &ch2;
        case 3: return &ch3;
    }
    return NULL;
}

void sound_play(int channel, const uint8_t* pcm, size_t len, uint8_t volume, bool loop)
{
    sfx_channel_t *ch = get_ch(channel);
    if (!ch || !pcm || len == 0) return;

    critical_section_enter_blocking(&sound_crit);
    ch->pcm = pcm;
    ch->length = (uint32_t)len;
    ch->position = 0;
    ch->volume = volume;
    ch->start_vol = volume;
    ch->playing = 1;
    ch->loop = loop;
    critical_section_exit(&sound_crit);
}

void sound_set_volume(int channel, uint8_t volume)
{
    sfx_channel_t *ch = get_ch(channel);
    if (!ch) return;

    critical_section_enter_blocking(&sound_crit);
    ch->volume = volume;
    critical_section_exit(&sound_crit);
}

uint8_t sound_get_volume(int channel)
{
    sfx_channel_t *ch = get_ch(channel);
    if (!ch) return 0;

    uint8_t volume;
    
    critical_section_enter_blocking(&sound_crit);
    volume = ch->volume;
    critical_section_exit(&sound_crit);

    return volume;
}

uint8_t sound_get_start_volume(int channel)
{
    sfx_channel_t *ch = get_ch(channel);
    if (!ch) return 0;

    uint8_t volume;
    
    critical_section_enter_blocking(&sound_crit);
    volume = ch->start_vol;
    critical_section_exit(&sound_crit);

    return volume;
}

void sound_stop(int channel)
{
    sfx_channel_t *ch = get_ch(channel);
    if (!ch) return;

    critical_section_enter_blocking(&sound_crit);
    ch->playing = 0;
    critical_section_exit(&sound_crit);
}

bool sound_is_playing(int channel)
{
    sfx_channel_t *ch = get_ch(channel);
    if (!ch) return false;

    bool playing;
    critical_section_enter_blocking(&sound_crit);
    playing = ch->playing;
    critical_section_exit(&sound_crit);

    return playing;
}

bool sound_is_pcm_playing(int channel, const uint8_t* pcm)
{
    sfx_channel_t *ch = get_ch(channel);
    if (!ch) return false;

    bool result;
    critical_section_enter_blocking(&sound_crit);
    result = (ch->playing && ch->pcm == pcm);
    critical_section_exit(&sound_crit);

    return result;
}

void sound_stop_all(void)
{
    critical_section_enter_blocking(&sound_crit);
    ch0.playing = ch1.playing = ch2.playing = ch3.playing = 0;
    critical_section_exit(&sound_crit);
}

static inline float dist3d(float x1, float y1, float z1,
                                float x2, float y2, float z2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

void sound_init_environment(environment_sound_info_t* info, environment_sound_t* env, int count)
{
    if (info == NULL) {
        return;
    }
    info->environment = env;
    info->environment_count = count;
}

void sound_play_environment(environment_sound_info_t* info,
    absolute_time_t current_time, float pos_x, float pos_y, float pos_z)
{
    if (info == NULL || info->environment == NULL) {
        return;
    }
    environment_sound_t* env = info->environment;

    for (int idx = 0; idx < info->environment_count; idx++) {
        if (env[idx].fix_dist >= env[idx].att_dist) {
            continue;
        }
        float dist = dist3d(env[idx].pos_x, env[idx].pos_y, env[idx].pos_z, pos_x, pos_y, pos_z);
        if (dist <= env[idx].att_dist) {
            int volume = env[idx].volume;
            if (sound_is_pcm_playing(env[idx].channel, env[idx].pcm)) {
                if (env[idx].flag & SOUND_ENV_FLAG_RANDOM) {
                    volume = sound_get_start_volume(env[idx].channel);
                    if (dist > env[idx].fix_dist) {
                        volume = volume * (env[idx].att_dist - dist) / (env[idx].att_dist - env[idx].fix_dist);
                    }
                }
                else if (dist > env[idx].fix_dist) {
                    volume = env[idx].volume *
                        (env[idx].att_dist - dist) / (env[idx].att_dist - env[idx].fix_dist);
                }
                if (volume > 255) volume = 255;
                else if (volume < 0) volume = 0;
                sound_set_volume(env[idx].channel, (uint8_t)volume);
            }
            else {
                int base_volume = env[idx].volume;
                if (env[idx].flag & SOUND_ENV_FLAG_RANDOM) {
                    base_volume = (get_rand_32() & 0xF) * (base_volume >> 4);
                }
                volume = base_volume;
                if (dist > env[idx].fix_dist) {
                    volume = base_volume *
                        (env[idx].att_dist - dist) / (env[idx].att_dist - env[idx].fix_dist);
                }
                if (volume > 255) volume = 255;
                else if (volume < 0) volume = 0;

                if (!(env[idx].flag & SOUND_ENV_FLAG_LOOP)) {
                    if (((current_time - env[idx].prev_time) > env[idx].interval) && (get_rand_32() & 0xFF) > env[idx].prob) {
                        sound_play(env[idx].channel, env[idx].pcm, env[idx].len, (uint8_t)base_volume, false);
                        sound_set_volume(env[idx].channel, (uint8_t)volume);
                        env[idx].prev_time = current_time;
                    }
                }
                else {
                    sound_play(env[idx].channel, env[idx].pcm, env[idx].len, (uint8_t)base_volume, true);
                    sound_set_volume(env[idx].channel, (uint8_t)volume);
                }
            }
        }
        else {
            if (sound_is_pcm_playing(env[idx].channel, env[idx].pcm)) {
                sound_stop(env[idx].channel);
            }
        }
    }
}
