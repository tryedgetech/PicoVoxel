// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] renderer.h - Renderer header file

#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include "pico.h"

#include "app_config.h"
#include "player.h"
#include "sk_fonts.h"
#include "color.h"


// fixed-point arithmetic
typedef int32_t fix;
#define FIX_SHIFT 16
#define FIX_ONE (1 << FIX_SHIFT)
#define FLOAT_TO_FIX(x) ((fix)((x) * FIX_ONE))
#define FIX_MUL(a,b) ((fix)(((int64_t)(a)*(b)) >> FIX_SHIFT))
#define FIX_DIV(a,b) ((fix)(((int64_t)(a) << FIX_SHIFT) / (b)))
#define FIX_ABS(x) ((x) < 0 ? -(x) : (x))


#ifdef DO_DDA_STATISTICS
typedef struct {
	uint64_t rays;
	uint64_t dda_steps;
	uint32_t max_dda_steps;
	uint64_t total_dist;
	uint64_t macro_hits;
	uint64_t macro_skip_cells;
	uint64_t hit_rays;
	uint64_t miss_rays;
	uint64_t fogfull_rays;
} render_stats_t;

extern render_stats_t g_render_stats[2];
#endif // DO_DDA_STATISTICS


// fog
#define FOG_START    FLOAT_TO_FIX(14.0f) // 14.0f 12.0f  2.0f
#define FOG_END      FLOAT_TO_FIX(58.0f) // 58.0f 28.0f 12.0f
#define FOG_FULL     FLOAT_TO_FIX(64.0f) // 64.0f 36.0f 14.0f
#define FOG_COLOR    RGB565(255, 229, 204)

enum {
	FACE_X      = 0,
	FACE_Z      = 1,
	FACE_TOP    = 2,
	FACE_BOTTOM = 3,
	FACE_MAX,
};

typedef struct {
	fix fog_start;
	fix fog_end;
	fix fog_full;
	uint16_t fog_color;
	uint8_t face_scale[FACE_MAX]; // X, Z, TOP, BOTTOM
	bool distance_scale;
#ifdef DO_BLOCK_SHADOW
	fix shadow_end;
	uint16_t shadow_color;
	fix light_dir_x;
	fix light_dir_z;
	fix shadow_length_scale;
#endif // DO_BLOCK_SHADOW
} shader_params_t;

typedef struct {
	int posx;
	int posy;
	const sk_font_t* fontdata;
	uint16_t color;
	uint8_t opacity;
} draw_string_t;

void renderer_set_shader_params(const shader_params_t* params);

void init_luts(void);


#ifdef DO_TEXTURE_SAMPLING
void finalize_textures(void);
#ifdef DO_TEXTURE_ANIMATION
void __not_in_flash_func(renderer_update_animation_texture)(float delta_seconds);
#endif // DO_TEXTURE_ANIMATION
#ifdef DO_TEXSAMPLING_INTERP
void renderer_init_texture_interp(void);
#endif // DO_TEXSAMPLING_INTERP
#endif // DO_TEXTURE_SAMPLING


void __not_in_flash_func(renderer_interpolate_partial)(
	uint16_t* fb,
	int y_start,
	int y_end,
	bool bspan);

void __not_in_flash_func(renderer_render_partial)(
	const player_t* p, 
	uint16_t* fb,
	int y_start,
	int y_end,
	bool bspan);

void __not_in_flash_func(upscale_2x_aa_partial)(
	const uint16_t* src,
	uint16_t* dst,
	int y_start,
	int y_end);

void renderer_draw_string(draw_string_t attr, const char* str, uint16_t* buffer, int width, int height);

bool update_target(const player_t* p);

void set_target_display(bool disp);

bool  __not_in_flash_func(get_target_display)(void);

bool place_block(const player_t* p, uint8_t place_block);

bool remove_block(const player_t* p);


void draw_block_select(uint16_t* screen_buffer);

#endif // RENDERER_H
