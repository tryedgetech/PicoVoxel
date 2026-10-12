// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] sound.h - Sound header file

#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "hardware/clocks.h"

#define SOUND_ENV_FLAG_LOOP   (0x01)
#define SOUND_ENV_FLAG_RANDOM (0x02)

typedef struct {
   	float pos_x;
	float pos_y;
	float pos_z;
    float fix_dist;
    float att_dist;
    uint8_t volume;
    int channel;
    absolute_time_t prev_time;
    absolute_time_t interval;
    uint8_t prob;
    const uint8_t *pcm;
    size_t len;
    uint8_t flag;
} environment_sound_t;

typedef struct {
    environment_sound_t* environment;
    int environment_count;
} environment_sound_info_t;


// Initialize audio subsystem (PWM + Timer IRQ mixer)
// PWM GPIOO, 16kHz, 8-bit samples
// - Timer alarm set for 16kHz sample rate
// Must be called once during initialization
void sound_init(void);

void sound_play(int channel, const uint8_t* pcm, size_t len, uint8_t volume, bool loop);

void sound_set_volume(int channel, uint8_t volume);

uint8_t sound_get_volume(int channel);

void sound_stop(int channel);

void sound_stop_all(void);

void sound_init_environment(environment_sound_info_t* info, environment_sound_t* env, int count);

void sound_play_environment(environment_sound_info_t* info,
    absolute_time_t current_time, float pos_x, float pos_y, float pos_z);

#endif // SOUND_H