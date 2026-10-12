// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] renderer.c - Renderer implementation

#include "renderer.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pico/rand.h"
#include "pico/multicore.h"
#include "hardware/interp.h"

#include "block.h"
#include "world.h"


typedef struct {
	bool hit;
	uint8_t block_type;
	fix distance;
	int face_axis;
	fix hit_x;
	fix hit_y;
	fix hit_z;
	int block_x;
	int block_y;
	int block_z;
#ifdef DO_TRANSPARENT
	uint8_t trans_cnt;
	uint8_t trans_block;
	fix trans_dist;
#endif // DO_TRANSPARENT
} ray_hit_t;


typedef struct {
	bool hit;
	uint8_t block_type;
	fix distance;
	int face_axis;
	fix hit_x;
	fix hit_y;
	fix hit_z;
	// coordinates of the block(hit by the ray)
	int block_x;
	int block_y;
	int block_z;
	// normal vector of the hit face
	int normal_x;
	int normal_y;
	int normal_z;
} block_raycast_hit_t;


#ifdef DO_DEPTH_QUANTUM
#define DEPTH_TYPE uint8_t
#define DEPTH_EDGE_THRESHOLD_Q   3u
#else
#define DEPTH_TYPE fix
#define DEPTH_EDGE_THRESHOLD     FLOAT_TO_FIX(0.75f)
#endif // DO_DEPTH_QUANTUM
static DEPTH_TYPE depth_buffer[DRAW_WIDTH * DRAW_HEIGHT];

int	target_pos_x = -1;
int target_pos_y = -1;
int target_pos_z = -1;
int target_face = -1;
bool display_target = false;

#ifdef DO_DDA_STATISTICS
render_stats_t g_render_stats[2];
#endif // DO_DDA_STATISTICS


static shader_params_t shader_params = {
	.fog_start = FOG_START,
	.fog_end = FOG_END,
	.fog_full = FOG_FULL,
	.fog_color = FOG_COLOR,
	.face_scale = { 210, 235, 255, 185 },  // X, Z, TOP, BOTTOM
	.distance_scale = true,
#ifdef DO_BLOCK_SHADOW
    .shadow_end   = FLOAT_TO_FIX(20.0f),
    .shadow_color = RGB565(40, 40, 60),
	.light_dir_x = FLOAT_TO_FIX(-0.5f),
	.light_dir_z = FLOAT_TO_FIX(-0.866f),
	.shadow_length_scale = FLOAT_TO_FIX(1.4f),
#endif // DO_BLOCK_SHADOW
};


#ifdef DO_TRANSPARENT
// opacity reaches TRANS_RATIO_MAX at this travel distance inside transparent blocks
#define TRANS_FULL_DIST FLOAT_TO_FIX(6.0f)
#define TRANS_RATIO_MIN 80
#define TRANS_RATIO_MAX 224
#endif // DO_TRANSPARENT


#define TEX_SIZE 16

#ifdef DO_TEXTURE_SAMPLING
#define TEX_MASK (TEX_SIZE - 1)
#if TEX_SIZE != 16
#error renderer_init_texture_interp() assumes TEX_SIZE == 16 (4-bit u/v fields)
#endif
#define RAINBOW_BAND_WIDTH 2
#define RAINBOW_SCROLL_PPS 8.0f

#ifdef DO_PASSTHROUGH_BLOCK
#define PASSTHROUGH_COLOR (RGB565(230, 110, 55))
#endif // DO_PASSTHROUGH_BLOCK

uint16_t tex_blocks[][TEX_SIZE * TEX_SIZE] = {
	[BLOCK_NONE] = { 0 },
	[BLOCK_SOIL] = { 0 },
	[BLOCK_TRUNK] = { 0 },
	[BLOCK_LEAF] = { 0 },
	[BLOCK_CONCRETE] = { 0 },
	[BLOCK_STONE] = { 0 },
	[BLOCK_SAND] = { 0 },
	[BLOCK_SEA] = { 0 },
	[BLOCK_FALL] = { 0 },
	[BLOCK_RAINBOW] = { 0 },
};

void __not_in_flash_func(renderer_update_animation_texture)(float delta_seconds)
{
	// rainbow texture
	static float rainbow_offset_pixels = 0.0f;
	static uint16_t rainbow_palette[] = {
		RGB565(255,  50,  50), RGB565(255, 101,  50), RGB565(255, 153,  50),
		RGB565(255, 204,  50), RGB565(255, 255,  50), RGB565(204, 255,  50),
		RGB565(153, 255,  50), RGB565(101, 255,  50), RGB565( 50, 255,  50),
		RGB565( 50, 255, 101), RGB565( 50, 255, 153), RGB565( 50, 255, 204),
		RGB565( 50, 255, 255), RGB565( 50, 204, 255), RGB565( 50, 153, 255),
		RGB565( 50, 101, 255), RGB565( 50,  50, 255), RGB565(101,  50, 255),
		RGB565(153,  50, 255), RGB565(204,  50, 255), RGB565(255,  50, 255),
		RGB565(255,  50, 204), RGB565(255,  50, 153), RGB565(255,  50, 101),
	};

	const int palette_count = (int)(sizeof(rainbow_palette) / sizeof(rainbow_palette[0]));
	const int cycle_pixels = palette_count * RAINBOW_BAND_WIDTH;
	if (delta_seconds > 0.0f) {
		rainbow_offset_pixels += RAINBOW_SCROLL_PPS * delta_seconds;
	}

	while (rainbow_offset_pixels >= (float)cycle_pixels) {
		rainbow_offset_pixels -= (float)cycle_pixels;
	}

	while (rainbow_offset_pixels < 0.0f) {
		rainbow_offset_pixels += (float)cycle_pixels;
	}

	int phase = (int)rainbow_offset_pixels;
	for (int y = 0; y < TEX_SIZE; ++y) {
		for (int x = 0; x < TEX_SIZE; ++x) {
			int diagonal = x + y + phase;
			int band = (diagonal / RAINBOW_BAND_WIDTH) % palette_count;
			tex_blocks[BLOCK_RAINBOW][y * TEX_SIZE + x] = rainbow_palette[band];
		}
	}

	// flow texture
	uint16_t sea_top_row[TEX_SIZE];
	uint16_t fall_top_row[TEX_SIZE];
	for (int x = 0; x < TEX_SIZE; ++x) {
		sea_top_row[x] = tex_blocks[BLOCK_SEA][x];
		fall_top_row[x] = tex_blocks[BLOCK_FALL][x];
	}
	for (int y = 1; y < TEX_SIZE; ++y) {
		for (int x = 0; x < TEX_SIZE; ++x) {
			tex_blocks[BLOCK_SEA][(y - 1) * TEX_SIZE + x] = tex_blocks[BLOCK_SEA][y * TEX_SIZE + x];
			tex_blocks[BLOCK_FALL][(y - 1) * TEX_SIZE + x] = tex_blocks[BLOCK_FALL][y * TEX_SIZE + x];
		}
	}
	for (int x = 0; x < TEX_SIZE; ++x) {
		tex_blocks[BLOCK_SEA][(TEX_SIZE - 1) * TEX_SIZE + x] = sea_top_row[x];
		tex_blocks[BLOCK_FALL][(TEX_SIZE - 1) * TEX_SIZE + x] = fall_top_row[x];
	}
}

void finalize_textures(void)
{
	int color_base = 90;

	for (int y = 0; y < TEX_SIZE; y++) {
		for (int x = 0; x < TEX_SIZE; x++) {
			int i = y * TEX_SIZE + x;
			// cyan/magenta diagonal stripes
			uint8_t stripe1 = (((x + y) / 3) & 1);
			tex_blocks[BLOCK_NONE][i] = stripe1
				? color_rgb565( 64,  64, 255)
				: color_rgb565( 32,  32, 255);

			// soil
			uint8_t soil_r = color_base + 112 + ((x * 5 + y * 7) & 15);
			uint8_t soil_g = color_base + 72 + ((x * 3 + y * 5) & 15);
			uint8_t soil_b = color_base + 40 + ((x * 4 + y * 2) & 11);
			uint8_t soil_noise = (x * 13 + y * 7 + ((x * y) & 15)) & 31;
			if (soil_noise < 8) {
				soil_r -= 18;
				soil_g -= 12;
				soil_b -= 8;
			}
			else if (soil_noise > 24) {
				soil_r += 12;
				soil_g += 8;
				soil_b += 4;
			}
			if (((x + y + (x >> 1) + (y >> 2)) & 7) < 2) {
				soil_r -= 10;
				soil_g -= 8;
				soil_b -= 4;
			}
			tex_blocks[BLOCK_SOIL][i] = color_rgb565(soil_r, soil_g, soil_b);

			// trunk
			uint8_t stripe2 = (x + (y >> 2)) & 3;
			uint8_t r = color_base + 110 + stripe2 * 10;
			uint8_t g = color_base + 70 + stripe2 * 5;
			tex_blocks[BLOCK_TRUNK   ][i] = ((r  >> 3) << 11) | ((g >> 2) << 5) | (r >> 3);

			// leaf
			uint8_t leaf_base = 90 + ((x * 5 + y * 3) & 23);
			uint8_t leaf_noise = (x * 11 + y * 13 + ((x ^ y) * 3)) & 31;
			uint8_t leaf_r = color_base + 20 + (leaf_noise < 12 ? 6 : 0);
			uint8_t leaf_g = color_base + leaf_base + (leaf_noise > 20 ? 10 : 0);
			uint8_t leaf_b = color_base + 24 + (leaf_noise < 8 ? 8 : 0);
#ifdef DO_PASSTHROUGH_BLOCK
			if (((x + y) & 3) == 0) {
				tex_blocks[BLOCK_LEAF][i] = PASSTHROUGH_COLOR;
			}
			else {
				tex_blocks[BLOCK_LEAF][i] = color_rgb565(leaf_r, leaf_g, leaf_b);
			}
#else
			if (((x + y) & 3) == 0) {
				leaf_g += 8;
			}
			tex_blocks[BLOCK_LEAF][i] = color_rgb565(leaf_r, leaf_g, leaf_b);
#endif // DO_PASSTHROUGH_BLOCK
			// concrete
			uint8_t concrete_noise = get_rand_32() % 30;
			uint8_t concrete_r = 200 + concrete_noise;
			uint8_t concrete_g = 202 + concrete_noise;
			uint8_t concrete_b = 206 + concrete_noise;
			tex_blocks[BLOCK_CONCRETE][i] = color_rgb565(concrete_r, concrete_g, concrete_b);

			// stone
			uint8_t stone_noise = get_rand_32() % 40;
			uint8_t stone_r = 120 + stone_noise;
			uint8_t stone_g = 122 + stone_noise;
			uint8_t stone_b = 126 + stone_noise;
			tex_blocks[BLOCK_STONE][i] = color_rgb565(stone_r, stone_g, stone_b);

			// sandy beach
			uint8_t water_noise = (x * 7 + y * 11 + ((x ^ y) * 3)) & 31;
			uint8_t water_r = 245 + (water_noise < 8 ? 4 : 0);
			uint8_t water_g = 231 + (water_noise > 20 ? 10 : 0);
			uint8_t water_b = 218 + (water_noise > 12 ? 14 : 0);
			if (((x + y) & 3) == 0) {
				water_g += 6;
				water_b += 4;
			}
			tex_blocks[BLOCK_SAND][i] = color_rgb565(water_r, water_g, water_b);

			// sea water
			uint8_t sea_noise = (x * 5 + y * 9 + ((x + y) * 2)) & 31;
			uint8_t sea_r = color_base + 12 + (sea_noise < 10 ? 3 : 0);
			uint8_t sea_g = color_base + 48 + (sea_noise > 18 ? 8 : 0);
			uint8_t sea_b = color_base + 120 + (sea_noise > 12 ? 16 : 0);
			if (((x ^ y) & 5) == 0) {
				sea_r += 4;
				sea_g += 6;
				sea_b += 8;
			}
			tex_blocks[BLOCK_SEA][i] = color_rgb565(sea_r, sea_g, sea_b);

			// fall
			static const uint8_t fall_pattern[TEX_SIZE][TEX_SIZE] = {
				{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0 },
				{ 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0 },
				{ 0, 0, 0, 2, 2, 2, 2, 1, 1, 2, 2, 2, 2, 0, 0, 0 },
				{ 0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0 },
				{ 0, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0 },
				{ 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1 },
				{ 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2 },
				{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
				{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			};
			static const uint16_t fall_palette[3] = {
				RGB565(255, 255, 255),
				RGB565(230, 255, 255),
				RGB565(204, 255, 255),
			};
			tex_blocks[BLOCK_FALL][y * TEX_SIZE + x] = fall_palette[fall_pattern[y][x]];
		}
	}

#ifdef DO_TEXTURE_ANIMATION
	renderer_update_animation_texture(0.0f);
#endif // DO_TEXTURE_ANIMATION
}

#ifdef DO_TEXSAMPLING_INTERP
// Configures this core's interp0 for sample_texture()'s u/v texel-index extraction.
void renderer_init_texture_interp(void)
{
    interp_config cfg = interp_default_config();
    interp_config_set_shift(&cfg, FIX_SHIFT - 4);
    interp_config_set_mask(&cfg, 0, 3);
    interp_set_config(interp0, 0, &cfg);

    cfg = interp_default_config();
    interp_config_set_shift(&cfg, FIX_SHIFT - 8);
    interp_config_set_mask(&cfg, 4, 7);
    interp_set_config(interp0, 1, &cfg);

    interp0->base[0] = 0;
    interp0->base[1] = 0;
    interp0->base[2] = 0;
}
#endif // DO_TEXSAMPLING_INTERP

static inline uint16_t sample_texture(
	uint8_t block,
	int face,
	fix hx, fix hy, fix hz)
{
#ifdef DO_TEXSAMPLING_INTERP
	// interpo lane0/lanel are pre-configured (see renderer_init_texture_interp) to pull!
	// bits [12:15] straight out of a raw 016.16 coordinate as u (lanel, bits 0-3) and
	// v<<4 (lanel, bits 4-7), so FULL VTEX SIZE+u without any software mask/shift/combine.
	fix a, b;

	switch (face) {
	case FACE_X:
		a = hz;
		b = hy;
		break;
	case FACE_Z:
		a = hx;
		b = hy;
		break;
	case FACE_TOP:
	case FACE_BOTTOM:
	default:
		a = hx;
		b = hz;
		break;
	}
	interp0->accum[0] = (uint32_t)a;
	interp0->accum[1] = (uint32_t)b;

	int idx;
	if (face == FACE_X && hx < 0) {
		int u = TEX_MASK - (int)interp0->peek[0];
		idx = u | (int)interp0->peek[1];
	}
	else {
		idx = (int)interp0->peek[2];
	}

	if (block <= BLOCK_RAINBOW) {
		return tex_blocks[block][idx];
	}
	else {
		return block_type_to_rgb565(block);
	}
#else
	int u, v;
	fix fx, fy;

	switch (face) {
	case FACE_X:
		fx = hz & (FIX_ONE - 1);
		fy = hy & (FIX_ONE - 1);
		break;
	case FACE_Z:
		fx = hx & (FIX_ONE - 1);
		fy = hy & (FIX_ONE - 1);
		break;
	case FACE_TOP:
	case FACE_BOTTOM:
	default:
		fx = hx & (FIX_ONE - 1);
		fy = hz & (FIX_ONE - 1);
		break;
	}
	u = (fx * TEX_SIZE) >> FIX_SHIFT;
	v = (fy * TEX_SIZE) >> FIX_SHIFT;

	if (face == FACE_X && hx < 0) u = TEX_MASK - u;

	if (block <= BLOCK_RAINBOW) {
		return tex_blocks[block][v * TEX_SIZE + u];
	}
	else {
		return block_type_to_rgb565(block);
	}
#endif // DO_TEXSAMPLING_INTERP
}
#endif // DO_TEXTURE_SAMPLING


#define RAY_EPSILON_FIX (1)
#define RAY_BIG_DISTANCE (1 << 30)


#ifdef DO_MACROCELL_SKIP
#define MC_SKIP_THRESHOLD (RAY_BIG_DISTANCE >> 2)

static inline fix sat_add_side_dist(fix a, fix b)
{
	if (a >= RAY_BIG_DISTANCE || b >= RAY_BIG_DISTANCE) {
		return RAY_BIG_DISTANCE;
	}
	if (a > (RAY_BIG_DISTANCE - b)) {
		return RAY_BIG_DISTANCE;
	}
	return a + b;
}

#ifdef DO_MACROCELL_OPTIMIZE
// Advance a non-jump axis' side_dist/map position up to dist, closed-form
// (replaces the previous per-block catch-up loop with a single division).
static inline void mc_catchup_axis(int* map, fix* side_dist, fix delta_dist, int step, fix dist)
{
	if (*side_dist > dist) {
		return;
	}
	fix diff = dist - *side_dist;
	int32_t n = (int32_t)(diff / delta_dist) + 1;
	if (n > MACRO_CELL_SIZE) {
		n = MACRO_CELL_SIZE;
	}
	*map += step * n;
	*side_dist = sat_add_side_dist(*side_dist, (fix)n * delta_dist);
}
#endif // DO_MACROCELL_OPTIMIZE
#endif // DO_MACROCELL_SKIP


#define DIST_LUT_SIZE 256
static uint8_t distance_scale_lut[DIST_LUT_SIZE];
static uint8_t fog_lut[DIST_LUT_SIZE];

// Q32 reciprocal of fog_full * (DIST_LUT_SIZE - 1), so the per-pixel LUT index
// (a 64-bit division on cd/fog_full) reduces to a single 32x32->64 multiply,
// taking the high word (Cortex-M33 has no hardware 64-bit divider).
static int32_t fog_full_recip_q32;

static void update_fog_full_recip(void)
{
    fog_full_recip_q32 = (int32_t)(((int64_t)(DIST_LUT_SIZE - 1) << 32) / shader_params.fog_full);
}


void init_luts(void)
{
	update_fog_full_recip();

	for (int i = 0; i < DIST_LUT_SIZE; i++) {
		float t = (float)i / (DIST_LUT_SIZE - 1);
		float ds = 255.0f - 95.0f * t;
		if (ds < 0)
			ds = 0;
		distance_scale_lut[i] = (uint8_t)ds;

		float distance = t * ((float)shader_params.fog_full / (float)FIX_ONE);
		float fog_start = (float)shader_params.fog_start / (float)FIX_ONE;
		float fog_end = (float)shader_params.fog_end / (float)FIX_ONE;
		float f;
		if (distance <= fog_start) {
			f = 0.0f;
		}
		else if (distance >= fog_end) {
			f = 1.0f;
		}
		else {
			f = (distance - fog_start) / (fog_end - fog_start);
		}

		if (f < 0) f = 0;
		if (f > 1) f = 1;
		fog_lut[i] = (uint8_t)(f * 255.0f);
	}
}

static inline fix ray_delta_distance(fix dir)
{
#if 0
	if (FIX_ABS(dir) < RAY_EPSILON_FIX) {
		return RAY_BIG_DISTANCE;
	}
	return FIX_ABS(FIX_DIV(FIX_ONE, dir));
#else
	uint32_t a = (uint32_t)((dir < 0) ? -dir : dir);
	if (a < 4u) {
		return RAY_BIG_DISTANCE;
	}
	return (fix)(0xFFFFFFFFu / a);
#endif
}

static inline void setup_dda_axis(
	fix origin,
	fix dir,
	int map,
	int* step,
	fix* side_dist,
	fix* delta_dist)
{
	*delta_dist = ray_delta_distance(dir);

	if (dir < 0) {
		*step = -1;
		*side_dist = FIX_MUL(origin - (map << FIX_SHIFT), *delta_dist);
	}
	else {
		*step = 1;
		*side_dist = FIX_MUL(((map + 1) << FIX_SHIFT) - origin, *delta_dist);
	}
}


static inline uint16_t color_lerp565(uint16_t c1, uint16_t c2, uint8_t t)
{
	uint32_t r1 = (c1 >> 11) & 0x1F;
	uint32_t g1 = (c1 >> 5) & 0x3F;
	uint32_t b1 = c1 & 0x1F;

	uint32_t r2 = (c2 >> 11) & 0x1F;
	uint32_t g2 = (c2 >> 5) & 0x3F;
	uint32_t b2 = c2 & 0x1F;

	r1 = r1 + (((r2 - r1) * t) >> 8);
	g1 = g1 + (((g2 - g1) * t) >> 8);
	b1 = b1 + (((b2 - b1) * t) >> 8);

	return (r1 << 11) | (g1 << 5) | b1;
}

static inline uint16_t avg565_2(uint16_t a, uint16_t b)
{
	uint32_t rb = (a & 0xF81F) + (b & 0xF81F);
	uint32_t g = (a & 0x07E0) + (b & 0x07E0);

	rb = (rb >> 1) & 0xF81F;
	g= (g >> 1) & 0x07E0;

	return (uint16_t)(rb | g);
}

static inline uint16_t avg565_4(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
	uint32_t rb = (a & 0xF81F) + (b & 0xF81F) + (c & 0xF81F) + (d & 0xF81F);
	uint32_t g = (a & 0x07E0) + (b & 0x07E0) + (c & 0x07E0) + (d & 0x07E0);

	rb = (rb >> 2) & 0xF81F;
	g = (g >> 2) & 0x07E0;

	return (uint16_t)(rb | g);
}

static inline uint16_t color_weighted_avg8(
	uint16_t cL, uint16_t cR, uint16_t cU, uint16_t cD,
	uint16_t cUL, uint16_t cUR, uint16_t cDL, uint16_t cDR)
{	
	uint32_t r =
		(((cL >> 11) & 0x1F) << 1) +
		(((cR >> 11) & 0x1F) << 1) +
		(((cU >> 11) & 0x1F) << 1) +
		(((cD >> 11) & 0x1F) << 1) +
		((cUL >> 11) & 0x1F) +
		((cUR >> 11) & 0x1F) +
		((cDL >> 11) & 0x1F) +
		((cDR >> 11) & 0x1F);
	uint32_t g =
		(((cL >> 5) & 0x3F) << 1) +
		(((cR >> 5) & 0x3F) << 1) +
		(((cU >> 5) & 0x3F) << 1) +
		(((cD >> 5) & 0x3F) << 1) +
		((cUL >> 5) & 0x3F) +
		((cUR >> 5) & 0x3F) +
		((cDL >> 5) & 0x3F) +
		((cDR >> 5) & 0x3F);
	uint32_t b =
		(((cL) & 0x1F) << 1) +
		(((cR) & 0x1F) << 1) +
		(((cU) & 0x1F) << 1) +
		(((cD) & 0x1F) << 1) +
		((cUL) & 0x1F) +
		((cUR) & 0x1F) +
		((cDL) & 0x1F) +
		((cDR) & 0x1F);

	r = (r * 341) >> 12;
	g = (g * 341) >> 12;
	b = (b * 341) >> 12;

	if(r > 31) r = 31;
	if(g > 63) g = 63;
	if(b > 31) b = 31;

	return (r << 11) | (g << 5) | b;
}


static inline bool ray_fog_terminate(
	fix dist,
	int face,
#ifdef DO_DDA_STATISTICS
	uint32_t dda_steps,
	render_stats_t *stats,
#endif // DO_DDA_STATISTICS
	ray_hit_t *out_hit)
{
	if (dist <= shader_params.fog_full) {
		return false;
	}

	*out_hit = (ray_hit_t){
		false,
		0,
		dist,
		face,
		0,0,0,
		0,0,0,
#ifdef DO_TRANSPARENT
		0,0,0,
#endif // DO_TRANSPARENT
	};

#ifdef DO_DDA_STATISTICS
	stats->fogfull_rays++;
	stats->total_dist += dist;
	stats->dda_steps += dda_steps;
	if (dda_steps > stats->max_dda_steps) {
		stats->max_dda_steps = dda_steps;
	}
#endif // DO_DDA_STATISTICS

	return true;
}


static ray_hit_t __not_in_flash_func(trace_ray_fog)(
	fix ox, fix oy, fix oz,
	fix dx, fix dy, fix dz)
{
	int mx = ox >> FIX_SHIFT;
	int my = oy >> FIX_SHIFT;
	int mz = oz >> FIX_SHIFT;

	int sx, sy, sz;
	fix sx_d, sy_d, sz_d;
	fix dx_d, dy_d, dz_d;
	int fx = -1;
	int fy = -1;
	int fz = -1;

	setup_dda_axis(ox, dx, mx, &sx, &sx_d, &dx_d);
	setup_dda_axis(oy, dy, my, &sy, &sy_d, &dy_d);
	setup_dda_axis(oz, dz, mz, &sz, &sz_d, &dz_d);

	fix dist = 0;
	int face = FACE_Z;
#ifdef DO_TRANSPARENT
	uint8_t trans_cnt = 0;
	uint8_t trans_block = BLOCK_NONE;
	fix trans_enter_dist = 0;
#endif // DO_TRANSPARENT
	uint8_t block = BLOCK_NONE;

#ifdef DO_DDA_STATISTICS
	render_stats_t* stats = &g_render_stats[get_core_num()];
	stats->rays++;
	uint32_t dda_steps = 0;
#endif // DO_DDA_STATISTICS

	for (int i = 0; i < RENDER_MAX_DDA_STEPS; i++) {
#ifdef DO_DDA_STATISTICS
		dda_steps++;
#endif // DO_DDA_STATISTICS

		if (my >= WORLD_SIZE_Y && sy > 0) break;
		if (my < 0 && sy < 0) break;

		if (dist > shader_params.fog_full) {
			ray_hit_t hitb = {
				false, 0, dist, face, 0, 0, 0, 0, 0, 0,
#ifdef DO_TRANSPARENT
				0, 0, 0,
#endif // DO_TRANSPARENT
			};

#ifdef DO_DDA_STATISTICS
			stats->fogfull_rays++;
			stats->total_dist += dist;
			stats->dda_steps += dda_steps;
			if (dda_steps > stats->max_dda_steps) {
				stats->max_dda_steps = dda_steps;
			}
#endif // DO_DDA_STATISTICS

			return hitb;
		}

		if ((unsigned)mx >= WORLD_SIZE_X ||
			(unsigned)mz >= WORLD_SIZE_Z)
			break;

#ifdef DO_MACROCELL_SKIP
		if (my >= WORLD_SIZE_Y && sy < 0) {
			int n = my - (WORLD_SIZE_Y - 1);
			fix jump = sat_add_side_dist(sy_d, (fix)(n - 1) * dy_d);
			if (jump > shader_params.fog_full) break;

			dist = jump;
			my = WORLD_SIZE_Y - 1;
			sy_d = sat_add_side_dist(sy_d, (fix)n * dy_d);
			face = FACE_TOP;

			fix px = ox + FIX_MUL(dx, dist);
			fix pz = oz + FIX_MUL(dz, dist);
			mx = px >> FIX_SHIFT;
			mz = pz >> FIX_SHIFT;
			sx_d = dist + FIX_MUL((sx > 0) ? (((mx + 1) << FIX_SHIFT) - px) 
											: (px - (mx << FIX_SHIFT)), dx_d);
			sz_d = dist + FIX_MUL((sz > 0) ? (((mz + 1) << FIX_SHIFT) - pz)
											: (pz - (mz << FIX_SHIFT)), dz_d);
			continue;
		}

		// Мacrocell empty-space skip
		if ((unsigned) my < (unsigned)WORLD_SIZE_Y) {
			int mcx = mx >> MACRO_CELL_SHIFT;
			int mcy = my >> MACRO_CELL_SHIFT;
			int mcz = mz >> MACRO_CELL_SHIFT;
			if (world_macrocell_count[mcy][mcz][mcx] == 0) {
				// Compute number of block-steps to exit macrocell per axis
				int skip_nx, skip_ny, skip_nz;
				fix mc_sx, mc_sy, mc_sz;

				if (dx_d >= MC_SKIP_THRESHOLD) {
					skip_nx = 1; mc_sx = (fix)RAY_BIG_DISTANCE;
				}
				else if (sx > 0) {
					skip_nx = ((mcx + 1) << MACRO_CELL_SHIFT) - mx;
					mc_sx = sx_d + (fix)(skip_nx - 1) * dx_d;
				}
				else {
					skip_nx = mx - (mcx << MACRO_CELL_SHIFT) + 1;
					mc_sx = sx_d + (fix)(skip_nx - 1) * dx_d;
				}

				if (dy_d >= MC_SKIP_THRESHOLD) {
					skip_ny = 1; mc_sy = (fix)RAY_BIG_DISTANCE;
				}
				else if (sy > 0) {
					skip_ny = ((mcy + 1) << MACRO_CELL_SHIFT) - my;
					mc_sy = sy_d + (fix)(skip_ny - 1) * dy_d;
				}
				else {
					skip_ny = my - (mcy << MACRO_CELL_SHIFT) + 1;
					mc_sy = sy_d + (fix)(skip_ny - 1) * dy_d;
				}

				if (dz_d >= MC_SKIP_THRESHOLD) {
					skip_nz = 1; mc_sz = (fix) RAY_BIG_DISTANCE;
				}
				else if (sz > 0) {
					skip_nz = ((mcz + 1) << MACRO_CELL_SHIFT) - mz;
					mc_sz = sz_d + (fix)(skip_nz - 1) * dz_d;
				}
				else {
					skip_nz = mz - (mcz << MACRO_CELL_SHIFT) + 1;
					mc_sz = sz_d + (fix)(skip_nz - 1) * dz_d;
				}

				// Jump along the axis that exits the macrocell first
				if (mc_sx <= mc_sy && mc_sx <= mc_sz) {
					dist = mc_sx;
					face = FACE_X;
				    ray_hit_t fog_hit;
					if (ray_fog_terminate(dist, face,
#ifdef DO_DDA_STATISTICS
						dda_steps, stats,
#endif // DO_DDA_STATISTICS
						&fog_hit)) {
						return fog_hit;
					}

					mx += sx * skip_nx;
					sx_d = sat_add_side_dist(sx_d, (fix)skip_nx * dx_d);
#ifdef DO_MACROCELL_OPTIMIZE
					mc_catchup_axis(&my, &sy_d, dy_d, sy, dist);
					mc_catchup_axis(&mz, &sz_d, dz_d, sz, dist);
#else
					for (int k = 0; k < MACRO_CELL_SIZE && sy_d <= dist; k++) {
						my += sy;
						sy_d = sat_add_side_dist(sy_d, dy_d);
					}
					for (int k = 0; k < MACRO_CELL_SIZE && sz_d <= dist; k++) {
						mz += sz;
						sz_d = sat_add_side_dist(sz_d, dz_d);
					}
#endif // DO_MACROCELL_OPTIMIZE
#ifdef DO_DDA_STATISTICS
					stats->macro_skip_cells += skip_nx;
#endif // DO_DDA_STATISTICS
				}
				else if (mc_sy <= mc_sz) {
					dist = mc_sy;
					face = (sy > 0) ? FACE_BOTTOM : FACE_TOP;
				    ray_hit_t fog_hit;
					if (ray_fog_terminate(dist, face,
#ifdef DO_DDA_STATISTICS
						dda_steps, stats,
#endif // DO_DDA_STATISTICS
						&fog_hit)) {
						return fog_hit;
					}

					my += sy * skip_ny;
					sy_d = sat_add_side_dist(sy_d, (fix)skip_ny * dy_d);
#ifdef DO_MACROCELL_OPTIMIZE
					mc_catchup_axis(&mx, &sx_d, dx_d, sx, dist);
					mc_catchup_axis(&mz, &sz_d, dz_d, sz, dist);
#else
					for (int k = 0; k < MACRO_CELL_SIZE && sx_d <= dist; k++) {
						mx += sx;
						sx_d = sat_add_side_dist(sx_d, dx_d);
					}
					for (int k = 0; k < MACRO_CELL_SIZE && sz_d <= dist; k++) {
						mz += sz;
						sz_d = sat_add_side_dist(sz_d, dz_d);
					}
#endif // DO_MACROCELL_OPTIMIZE
#ifdef DO_DDA_STATISTICS
					stats->macro_skip_cells += skip_ny;
#endif // DO_DDA_STATISTICS
				}
				else {
					dist = mc_sz;
					face = FACE_Z;
				    ray_hit_t fog_hit;
					if (ray_fog_terminate(dist, face,
#ifdef DO_DDA_STATISTICS
						dda_steps, stats,
#endif // DO_DDA_STATISTICS
						&fog_hit)) {
						return fog_hit;
					}

					mz += sz * skip_nz;
					sz_d = sat_add_side_dist(sz_d, (fix)skip_nz * dz_d);
#ifdef DO_MACROCELL_OPTIMIZE
					mc_catchup_axis(&mx, &sx_d, dx_d, sx, dist);
					mc_catchup_axis(&my, &sy_d, dy_d, sy, dist);
#else
					for (int k = 0; k < MACRO_CELL_SIZE && sx_d <= dist; k++) {
						mx += sx;
						sx_d = sat_add_side_dist(sx_d, dx_d);
					}
					for (int k = 0; k < MACRO_CELL_SIZE && sy_d <= dist; k++) {
						my += sy;
						sy_d = sat_add_side_dist(sy_d, dy_d);
					}
#endif // DO_MACROCELL_OPTIMIZE
#ifdef DO_DDA_STATISTICS
					stats->macro_skip_cells += skip_nz;
#endif // DO_DDA_STATISTICS
				}

#ifdef DO_DDA_STATISTICS
				stats->macro_hits++;
#endif // DO_DDA_STATISTICS

				continue; // re-check fog/bounds at new position
			}
		}
#endif // DO_MACROCELL_SKIP

		if ((unsigned)my >= WORLD_SIZE_Y) {
			block = BLOCK_NONE;
		}
		else {
			block = world_blocks[my][mz][mx];
		}

#ifdef DO_TRANSPARENT
		if (((block == BLOCK_SEA) || (block == BLOCK_FALL)) &&
			!(display_target && (target_face == face) && (target_pos_x == mx) && (target_pos_y == my) && (target_pos_z == mz))) {
			trans_cnt++;
			trans_block = trans_block == BLOCK_NONE ? block : trans_block;
			if (fx == -1) {
				fx = mx;
				fy = my;
				fz = mz;
				trans_enter_dist = dist;
			}
		}
		else if (block != BLOCK_NONE) {
#else
		if (block != BLOCK_NONE) {
#endif // DO_TRANSPARENT
			fix hit_x = ox + FIX_MUL(dx, dist);
			fix hit_y = oy + FIX_MUL(dy, dist);
			fix hit_z = oz + FIX_MUL(dz, dist);

#if defined(DO_TEXTURE_SAMPLING) && defined(DO_PASSTHROUGH_BLOCK)
			if (sample_texture(block, face, hit_x, hit_y, hit_z) != PASSTHROUGH_COLOR) {
#endif // defined(DO_TEXTURE_SAMPLING) && defined(DO_PASSTHROUGH_BLOCK)
				ray_hit_t hitb = {
					true, block, dist, face,
					hit_x,
					hit_y,
					hit_z,
					fx == -1 ? mx : fx,
					fy == -1 ? my : fy,
					fz == -1 ? mz : fz,
#ifdef DO_TRANSPARENT
					trans_cnt,
					trans_block,
					(trans_cnt && dist > trans_enter_dist) ? (dist - trans_enter_dist) : 0,
#endif // DO_TRANSPARENT
				};

#ifdef DO_DDA_STATISTICS
				stats->hit_rays++;
				stats->total_dist += dist;
				stats->dda_steps += dda_steps;
				if (dda_steps > stats->max_dda_steps) {
					stats->max_dda_steps = dda_steps;
				}
#endif // DO_DDA_STATISTICS

				return hitb;
#if defined(DO_TEXTURE_SAMPLING) && defined(DO_PASSTHROUGH_BLOCK)
			}
#endif // defined(DO_TEXTURE_SAMPLING) && defined(DO_PASSTHROUGH_BLOCK)
		}

		if (sx_d < sy_d) {
			if (sx_d < sz_d) {
				mx += sx; dist = sx_d;
				sx_d += dx_d; face = FACE_X;
			}
			else {
				mz += sz; dist = sz_d;
				sz_d += dz_d; face = FACE_Z;
			}
		}
		else {
			if (sy_d < sz_d) {
				my += sy; dist = sy_d;
				sy_d += dy_d; face = (sy > 0) ? FACE_BOTTOM : FACE_TOP;
			}
			else {
				mz += sz; dist = sz_d;
				sz_d += dz_d; face = FACE_Z;
			}
		}
	}

	ray_hit_t hitb = {
		false, 0, dist, face, 0, 0, 0, 0, 0, 0,
#ifdef DO_TRANSPARENT
		0, 0, 0,
#endif // DO_TRANSPARENT
	};

#ifdef DO_DDA_STATISTICS
	stats->miss_rays++;
	stats->total_dist += dist;
	stats->dda_steps += dda_steps;
	if (dda_steps > stats->max_dda_steps) {
		stats->max_dda_steps = dda_steps;
	}
#endif // DO_DDA_STATISTICS

	return hitb;
}


void renderer_set_shader_params(const shader_params_t* params)
{
	shader_params = *params;
	update_fog_full_recip();
}


static uint16_t __not_in_flash_func(shade_with_fog)(
	ray_hit_t* p_hit,
	fix ray_x, fix ray_z,
	fix fx, fix fz)
{
#ifdef DO_TEXTURE_SAMPLING
	uint16_t base = sample_texture(
		p_hit->block_type, p_hit->face_axis,
		p_hit->hit_x, p_hit->hit_y, p_hit->hit_z);
#else
	uint16_t base = block_type_to_rgb565(p_hit->block_type);
#endif // DO_TEXTURE_SAMPLING

	uint8_t face_scale = shader_params.face_scale[
		(p_hit->face_axis >= 0 && p_hit->face_axis < 4) ? p_hit->face_axis : FACE_Z];

	// fisheye
	fix dot = FIX_MUL(ray_x, fx) + FIX_MUL(ray_z, fz);
	fix cd = FIX_MUL(p_hit->distance, dot);
	if (cd < 0) cd = 0;

#if 0
	// distance scale LUT
	int idx = (FIX_DIV(cd, shader_params.fog_full) * (DIST_LUT_SIZE - 1)) >> FIX_SHIFT;
#else
	// distance scale LUT (reciprocal multiply instead of a 64-bit divide)
	int idx = (int)(((int64_t)cd * fog_full_recip_q32) >> 32);
#endif

	// dx = cd * LUT_SCALE >> shift
	if (idx < 0) idx = 0;
	if (idx >= DIST_LUT_SIZE) idx = DIST_LUT_SIZE - 1;
	
	uint8_t ds = distance_scale_lut[idx];
	uint8_t scale;
	if (shader_params.distance_scale) {
		scale = (face_scale * ds) >> 8;
	}
	else {
		scale = face_scale;
	}

	uint16_t color;
	if ((p_hit->block_type == BLOCK_RAINBOW)) {
		color = base;
	}
	else {
		color = color_scale_rgb565(base, scale);
	}

#ifdef DO_BLOCK_SHADOW
	if (p_hit->face_axis == FACE_TOP && p_hit->distance < shader_params.shadow_end) {
		uint16_t shadow_info = world_get_block_shadow(p_hit->block_x, p_hit->block_z);
		if (p_hit->block_y < (int)(shadow_info & WORLD_SHADOW_LOW_MASK)) {
			color = color_lerp565(color, shader_params.shadow_color, 64);
		}
		else if (shadow_info & WORLD_SHADOW_HIGH_MASK) {
			// Position within the block, clamped to [0, FIX_ONE]
			fix local_x = p_hit->hit_x - ((fix)p_hit->block_x << FIX_SHIFT);
			fix local_z = p_hit->hit_z - ((fix)p_hit->block_z << FIX_SHIFT);
			if (local_x < 0) local_x = 0; else if (local_x > FIX_ONE) local_x = FIX_ONE;
			if (local_z < 0) local_z = 0; else if (local_z > FIX_ONE) local_z = FIX_ONE;

			// Only the neighbor standing between this block and the pseudo light
			// source can cast a shadow here; a taller neighbor on the far side
			// (facing away from the light) is ignored.
			uint16_t x_bit = (shader_params.light_dir_x < 0) ? WORLD_SHADOW_NEG_X_BIT : WORLD_SHADOW_POS_X_BIT;
			uint16_t z_bit = (shader_params.light_dir_z < 0) ? WORLD_SHADOW_NEG_Z_BIT : WORLD_SHADOW_POS_Z_BIT;
			fix weight_x = FIX_ABS(shader_params.light_dir_x);
			fix weight_z = FIX_ABS(shader_params.light_dir_z);

			// Gradient strength per shadow-casting edge, strongest at the edge and
			// fading inward; take the max so a shadowed corner blends smoothly.
			// shadow_length_scale stretches the reach to approximate a lower light elevation.
			fix shadow_strength = 0;
			if ((shadow_info & x_bit) && weight_x > 0) {
				fix s = (x_bit == WORLD_SHADOW_NEG_X_BIT) ? (FIX_ONE - local_x) : local_x;
				s = FIX_MUL(FIX_MUL(s, weight_x), shader_params.shadow_length_scale);
				if (s > shadow_strength) shadow_strength = s;
			}
			if ((shadow_info & z_bit) && weight_z > 0) {
				fix s = (z_bit == WORLD_SHADOW_NEG_Z_BIT) ? (FIX_ONE - local_z) : local_z;
				s = FIX_MUL(FIX_MUL(s, weight_z), shader_params.shadow_length_scale);
				if (s > shadow_strength) shadow_strength = s;
			}
			if (shadow_strength > FIX_ONE) shadow_strength = FIX_ONE;

			if (shadow_strength > 0) {
				uint32_t shadow_t = (uint32_t)shadow_strength >> (FIX_SHIFT - 6);
				if (shadow_t > 64) shadow_t = 64;
				color = color_lerp565(color, shader_params.shadow_color, (uint8_t)shadow_t);
			}
		}
	}
#endif // DO_BLOCK_SHADOW

	if (display_target && (target_face == p_hit->face_axis) &&
		(target_pos_x == p_hit->block_x) && (target_pos_y == p_hit->block_y) && (target_pos_z == p_hit->block_z)) {
		// target block
#ifdef DO_TEXTURE_SAMPLING
		uint16_t target_color = sample_texture(BLOCK_NONE, p_hit->face_axis, p_hit->hit_x, p_hit->hit_y, p_hit->hit_z);
#else
		uint16_t target_color = RGB565(128, 128, 255);
#endif // DO_TEXTURE_SAMPLING
		color = color_lerp565(color, target_color, 200);
	}
	else {

#ifdef DO_TRANSPARENT
		if (p_hit->trans_cnt) {
			int ratio = TRANS_RATIO_MIN +
				(int)(((int64_t)p_hit->trans_dist * (TRANS_RATIO_MAX - TRANS_RATIO_MIN)) / TRANS_FULL_DIST);
			if (ratio > TRANS_RATIO_MAX) ratio = TRANS_RATIO_MAX;
#ifdef DO_TEXTURE_SAMPLING
			uint16_t trans_color = sample_texture(p_hit->trans_block, p_hit->face_axis, p_hit->hit_x, p_hit->hit_y, p_hit->hit_z);
#else
			uint16_t trans_color = block_type_to_rgb565(p_hit->trans_block);
#endif // DO_TEXTURE_SAMPLING
			if (p_hit->trans_block == BLOCK_FALL) {
				color = color_lerp565(color, trans_color, 160);
			}
			else {
				color = color_lerp565(color, trans_color, ratio);
			}
		}
#endif // DO_TRANSPARENT

		// Smooth Volumetric Fog without pop-in
		uint8_t fog_t = fog_lut[idx];
		if (fog_t > 0) {
			color = color_lerp565(color, shader_params.fog_color, fog_t);
		}

	}

	return color;
}


#ifdef DO_DEPTH_QUANTUM
static inline bool depth_is_edge(uint8_t d0, uint8_t d1)
{
	uint8_t diff = (d0 > d1) ? (d0 - d1) : (d1 - d0);
	return (diff > DEPTH_EDGE_THRESHOLD_Q);
}

static inline uint8_t depth_pack_from_fix(fix depth_fix) {
	if (depth_fix <= 0) {
		return 0;
	}
	if (depth_fix >= shader_params.fog_full) {
		return UINT8_MAX;
	}
	// Round to nearest while mapping [0, shader_params.fog_full] into [0, 255].
	return (uint8_t)((((uint32_t)depth_fix * UINT8_MAX) + (shader_params.fog_full >> 1)) / shader_params.fog_full);
}
#else
static inline bool depth_is_edge(fix d0, fix d1)
{
	fix diff = d0 - d1;
	if (diff < 0) {
		diff = -diff;
	}
	return (diff > DEPTH_EDGE_THRESHOLD);
}
#endif // DO_DEPTH_QUANTUM


void __not_in_flash_func(renderer_interpolate_partial)(
	uint16_t* fb,
	int y_start,
	int y_end,
	bool bspan)
{
	for (int y = y_start; y < y_end; y++) {

		uint16_t* row = &fb[y * DRAW_WIDTH];
		DEPTH_TYPE* depth_row = &depth_buffer[y * DRAW_WIDTH];
		uint16_t* row_up = (y > 0) ? &fb[(y - 1) * DRAW_WIDTH] : row;
		uint16_t* row_dn = (y < DRAW_HEIGHT - 1) ? &fb[(y + 1) * DRAW_WIDTH] : row;
		DEPTH_TYPE* depth_up = (y > 0) ? &depth_buffer[(y - 1) * DRAW_WIDTH] : depth_row;
		DEPTH_TYPE* depth_dn = (y < DRAW_HEIGHT - 1) ? &depth_buffer[(y + 1) * DRAW_WIDTH] : depth_row;
		int y_parity = y & 1;
		int start_x = bspan ? y_parity : (1 - y_parity);

		for (int x = start_x; x < DRAW_WIDTH; x += 2) {

			uint16_t cL = (x > 0) ? row[x - 1] : row[x];
			uint16_t cR = (x < DRAW_WIDTH - 1) ? row[x + 1] : row[x];
			uint16_t cU = row_up[x];
			uint16_t cD = row_dn[x];

			DEPTH_TYPE dL = (x > 0) ? depth_row[x - 1] : depth_row[x];
			DEPTH_TYPE dR = (x < DRAW_WIDTH - 1) ? depth_row[x + 1] : depth_row[x];
			DEPTH_TYPE dU = depth_up[x];
			DEPTH_TYPE dD = depth_dn[x];

			bool lr_edge = depth_is_edge(dL, dR);
			bool ud_edge = depth_is_edge(dU, dD);

			if (lr_edge && ud_edge) {
				bool lu_edge = depth_is_edge(dL, dU);
				bool rd_edge = depth_is_edge(dR, dD);
				bool ld_edge = depth_is_edge(dL, dD);
				bool ur_edge = depth_is_edge(dU, dR);
				if ((!lu_edge && !rd_edge) || (!ld_edge && !ur_edge)) {
					row[x] = avg565_4(cL, cR, cU, cD);
					DEPTH_TYPE nearest = dL;
					if (dR < nearest) {
						nearest = dR;
					}
					if (dU < nearest) {
						nearest = dU;
					}
					if (dD < nearest) {
						nearest = dD;
					}
					depth_row[x] = nearest;
				}
				else if (!lu_edge) {
					// slant edge
					row[x] = avg565_2(cL, cU);
					// depth interpolation
#ifdef DO_DEPTH_QUANTUM
					depth_row[x] = (uint8_t)(((uint16_t)dL + dU) >> 1);
#else
					depth_row[x] = (dL + dU) >> 1;
#endif // DO_DEPTH_QUANTUM
				}
				else if (!rd_edge) {
					// slant edge
					row[x] = avg565_2(cR, cD);
					// depth interpolation
#ifdef DO_DEPTH_QUANTUM
					depth_row[x] = (uint8_t)(((uint16_t)dR + dD) >> 1);
#else
					depth_row[x] = (dR + dD) >> 1;
#endif // DO_DEPTH_QUANTUM
				}
				else if (!ld_edge) {
					// slant edge
					row[x] = avg565_2(cL, cD);
					// depth interpolation
#ifdef DO_DEPTH_QUANTUM
					depth_row[x] = (uint8_t)(((uint16_t)dL + dD) >> 1);
#else
					depth_row[x] = (dL + dD) >> 1;
#endif // DO_DEPTH_QUANTUM
				}
				else if (!ur_edge) {
					// slant edge
					row[x] = avg565_2(cR, cU);
					// depth interpolation
#ifdef DO_DEPTH_QUANTUM
					depth_row[x] = (uint8_t)(((uint16_t)dR + dU) >> 1);
#else
					depth_row[x] = (dR + dU) >> 1;
#endif // DO_DEPTH_QUANTUM
				}
				else {
					// corner edge
					DEPTH_TYPE nearest = dL;
					uint16_t nearest_color = cL;
					if (dR < nearest) {
						nearest = dR;
						nearest_color = cR;
					}
					if (dU < nearest) {
						nearest = dU;
						nearest_color = cU;
					}
					if (dD < nearest) {
						nearest = dD;
						nearest_color = cD;
					}
//					row[x] = nearest_color;
					row[x] = avg565_4(cL, cR, cU, cD);
					depth_row[x] = nearest;
				}
			}
			else if (lr_edge && !ud_edge) {
				// vertical edge
				row[x] = avg565_2(cU, cD);
				// depth interpolation
#ifdef DO_DEPTH_QUANTUM
				depth_row[x] = (uint8_t)(((uint16_t)dU + dD) >> 1);
#else
				depth_row[x] = (dU + dD) >> 1;
#endif // DO_DEPTH_QUANTUM
			}
			else if (!lr_edge && ud_edge) {
				// horizonal edge
				row[x] = avg565_2(cL, cR);
				// depth interpolation
#ifdef DO_DEPTH_QUANTUM
				depth_row[x] = (uint8_t)(((uint16_t)dL + dR) >> 1);
#else
				depth_row[x] = (dL + dR) >> 1;
#endif // DO_DEPTH_QUANTUM
			}
			else {
				// every one nearest
				row[x] = avg565_4(cL, cR, cU, cD);
				// depth interpolation
#ifdef DO_DEPTH_QUANTUM
				depth_row[x] = (uint8_t)(((uint16_t)dL + dR + dU + dD) >> 2);
#else
				depth_row[x] = (dL + dR + dU + dD) >> 2;
#endif // DO_DEPTH_QUANTUM
			}
		}
	}
}


void __not_in_flash_func(renderer_render_partial)(
	const player_t* restrict p,
	uint16_t* restrict fb,
	int y_start,
	int y_end,
	bool bspan)
{
	float yaw = p->yaw_radians;
	float pitch = p->pitch_radians;
	float roll = p->roll_radians;
	
	fix cos_y = FLOAT_TO_FIX(cosf(yaw));
	fix sin_y = FLOAT_TO_FIX(sinf(yaw));

	fix cos_p = FLOAT_TO_FIX(cosf(pitch));
	fix sin_p = FLOAT_TO_FIX(sinf(pitch));
	fix cos_r = FLOAT_TO_FIX(cosf(roll));
	fix sin_r = FLOAT_TO_FIX(sinf(roll));

	// camera forward vector
	fix fx = FIX_MUL(cos_p, cos_y);
	fix fy = sin_p;
	fix fz = FIX_MUL(cos_p, sin_y);

	fix rx0 = -sin_y;
	fix ry0 = 0;
	fix rz0 = cos_y;

	fix ux0 = -FIX_MUL(cos_y, sin_p);
	fix uy0 = cos_p;
	fix uz0 = -FIX_MUL(sin_y, sin_p);

	// apply roll rotation
	fix rx = FIX_MUL(rx0, cos_r) + FIX_MUL(ux0, sin_r);
	fix ry = FIX_MUL(ry0, cos_r) + FIX_MUL(uy0, sin_r);
	fix rz = FIX_MUL(rz0, cos_r) + FIX_MUL(uz0, sin_r);

	fix ux = FIX_MUL(ux0, cos_r) - FIX_MUL(rx0, sin_r);
	fix uy = FIX_MUL(uy0, cos_r) - FIX_MUL(ry0, sin_r);
	fix uz = FIX_MUL(uz0, cos_r) - FIX_MUL(rz0, sin_r);

	// FOV
	float fov = RENDER_HORIZONTAL_FOV_DEGREES * 0.01745f;

	fix scale_x = FLOAT_TO_FIX(tanf(fov * 0.5f));
	fix aspect = FLOAT_TO_FIX((float) DRAW_HEIGHT / DRAW_WIDTH);
	fix scale_y = FIX_MUL(scale_x, aspect);

	fix sx = -scale_x;
	fix step = FIX_DIV(FIX_MUL(FLOAT_TO_FIX(2.0f), scale_x), FLOAT_TO_FIX((float)DRAW_WIDTH));
	fix dx = FIX_MUL(rx, step);
	fix dy = FIX_MUL(ry, step);
	fix dz = FIX_MUL(rz, step);
	fix dx2 = dx + dx;
	fix dy2 = dy + dy;
	fix dz2 = dz + dz;

	fix ox = FLOAT_TO_FIX(p->position_x);
	fix oy = FLOAT_TO_FIX(p->eye_y);
	fix oz = FLOAT_TO_FIX(p->position_z);

	
	for (int y = y_start; y < y_end; y++) {
	
		uint16_t* row = &fb[y * DRAW_WIDTH];
	
		// normalized screen coordinates (NDC) [-1, 1]
		fix sy = FIX_MUL(FLOAT_TO_FIX(1.0f) -
		         FIX_MUL(FLOAT_TO_FIX((float)(y + 0.5f) * 2.0f / DRAW_HEIGHT), FIX_ONE), scale_y);

		fix ray_x0 = fx + FIX_MUL(rx, sx) + FIX_MUL(ux, sy);
		fix ray_y0 = fy + FIX_MUL(ry, sx) + FIX_MUL(uy, sy);
		fix ray_z0 = fz + FIX_MUL(rz, sx) + FIX_MUL(uz, sy);
		int y_parity = y & 1;
		int start_x = bspan ? (1 - y_parity) : y_parity;

		fix ray_x = (start_x == 0) ? ray_x0 : (ray_x0 + dx);
		fix ray_y = (start_x == 0) ? ray_y0 : (ray_y0 + dy);
		fix ray_z = (start_x == 0) ? ray_z0 : (ray_z0 + dz);

		for (int x = start_x; x < DRAW_WIDTH; x += 2) {

			ray_hit_t hit = trace_ray_fog(
				ox, oy, oz,
				ray_x, ray_y, ray_z);

			if (hit.hit) {
				fix cd = FIX_MUL(hit.distance, FIX_MUL(ray_x, fx) + FIX_MUL(ray_z, fz));
				if (cd < 0) {
					cd = 0;
				}
#ifdef DO_DEPTH_QUANTUM
				depth_buffer[y * DRAW_WIDTH + x] = depth_pack_from_fix(cd);
#else
				depth_buffer[y * DRAW_WIDTH + x] = cd;
#endif // DO_DEPTH_QUANTUM

				row[x] = shade_with_fog(
					&hit,
					ray_x, ray_z,
					fx, fz);
			}
			else {
#ifdef DO_DEPTH_QUANTUM
				depth_buffer[y * DRAW_WIDTH + x] = UINT8_MAX;
#else
				depth_buffer[y * DRAW_WIDTH + x] = shader_params.fog_full;
#endif // DO_DEPTH_QUANTUM
				row[x] = shader_params.fog_color;				
			}
			ray_x += dx2;
			ray_y += dy2;
			ray_z += dz2;
		}
	}
}


static block_raycast_hit_t trace_ray_place(
	fix ox,
	fix oy,
	fix oz,
	fix dx,
	fix dy,
	fix dz,
	fix max_dist)
{
	int mx = ox >> FIX_SHIFT;
	int my = oy >> FIX_SHIFT;
	int mz = oz >> FIX_SHIFT;
	int sx, sy, sz;

	fix sx_d;
	fix sy_d;
	fix sz_d;
	fix dx_d;
	fix dy_d;
	fix dz_d;

	setup_dda_axis(ox, dx, mx, &sx, &sx_d, &dx_d);
	setup_dda_axis(oy, dy, my, &sy, &sy_d, &dy_d);
	setup_dda_axis(oz, dz, mz, &sz, &sz_d, &dz_d);

	fix dist = 0;
	int face_axis = FACE_Z;
	int nx = 0 ;
	int ny = 0 ;
	int nz = 0 ;

	for (int i = 0; i < RENDER_MAX_DDA_STEPS; i++) {
		if (dist > max_dist) {
			break;
		}
		if ((unsigned)mx >= WORLD_SIZE_X ||
			(unsigned)mz >= WORLD_SIZE_Z) {
			break;
		}

		uint8_t block = world_get_block(mx, my, mz);
		if (block != BLOCK_NONE) {
			block_raycast_hit_t hit = {
				true,
				block,
				dist,
				face_axis,
				ox + FIX_MUL(dx, dist),
				oy + FIX_MUL(dy, dist),
				oz + FIX_MUL(dz, dist),
				mx,
				my,
				mz,
				nx,
				ny,
				nz
			};
			return hit;
		}

		if (sx_d < sy_d) {
			if (sx_d < sz_d) {
				mx += sx;
				dist = sx_d;
				sx_d += dx_d;
				face_axis = FACE_X;
				nx = -sx;
				ny = 0;
				nz = 0;
			}
			else {
				mz += sz;
				dist = sz_d;
				sz_d += dz_d;
				face_axis = FACE_Z;
				nx = 0;
				ny = 0;
				nz = -sz;
			}
		}
		else {
			if (sy_d < sz_d) {
				my += sy;
				dist = sy_d;
				sy_d += dy_d;
				face_axis = (sy > 0) ? FACE_BOTTOM : FACE_TOP;
				nx = 0;
				ny = -sy;
				nz = 0;
			}
			else {
				mz += sz;
				dist = sz_d;
				sz_d += dz_d;
				face_axis = FACE_Z;
				nx = 0;
				ny = 0;
				nz = -sz;
			}
		}
	}
	block_raycast_hit_t miss = { 0 };
	return miss;
}

block_raycast_hit_t get_target_block(
	const player_t* p)
{
	// view direction vector
	float yaw = p->yaw_radians;
	float pitch = p->pitch_radians;
	fix cos_y = FLOAT_TO_FIX(cosf(yaw));
	fix sin_y = FLOAT_TO_FIX(sinf(yaw));
	fix cos_p = FLOAT_TO_FIX(cosf(pitch));
	fix sin_p = FLOAT_TO_FIX(sinf(pitch));
	fix fx = FIX_MUL(cos_p, cos_y);
	fix fy = sin_p;
	fix fz = FIX_MUL(cos_p, sin_y);

	// eye position
	fix ox = FLOAT_TO_FIX(p->position_x);
	fix oy = FLOAT_TO_FIX(p->eye_y);
	fix oz = FLOAT_TO_FIX(p->position_z);

	// Execute DDA
	return trace_ray_place(ox, oy, oz, fx, fy, fz, FLOAT_TO_FIX(6.0f));
}

bool update_target(const player_t* p)
{
	// get target
	block_raycast_hit_t hit = get_target_block(p);
	if (hit.hit) {
		target_pos_x = hit.block_x;
		target_pos_y = hit.block_y;
		target_pos_z = hit.block_z;
		target_face = hit.face_axis;
		return true;
	}
	else {
		target_pos_x = -1;
		target_pos_y = -1;
		target_pos_z = -1;
		target_face = -1;
		return false;
	}
}

void set_target_display(bool disp)
{
	display_target = disp;
}

bool __not_in_flash_func(get_target_display)(void)
{
	return display_target;
}

bool place_block(
	const player_t* p,
	uint8_t place_block)
{
	if (place_block == BLOCK_NONE) {
		return false;
	}

	// get target
	block_raycast_hit_t hit = get_target_block(p);
	if (!hit.hit) {
		return false;
	}

	// new block placed position
	int place_x = hit.block_x + hit.normal_x;
	int place_y = hit.block_y + hit.normal_y;
	int place_z = hit.block_z + hit.normal_z;
	if ((unsigned)place_x >= WORLD_SIZE_X ||
		(unsigned)place_y >= WORLD_SIZE_Y ||
		(unsigned)place_z >= WORLD_SIZE_Z) {
		return false;
	}

	// Check if the block is empty
	if (world_get_block(place_x, place_y, place_z) != BLOCK_NONE) {
		return false;
	}

	// Check if the new block collides with the player
	int player_x = (int)floorf(p->position_x);
	int player_z = (int)floorf(p->position_z);

	// The player's feet are at eye_y - PLAYER_EYE_HEIGHT,
	// so the block at that position is considered the foot block.
	int player_y0 = (int)floorf(p->eye_y - PLAYER_EYE_HEIGHT);

	// The block above the player's feet is considered the upper body block
	int player_y1 = player_y0 + 1;

	bool collide_player =
		(place_x == player_x) && (place_z == player_z) &&
		(place_y == player_y0 || place_y == player_y1);
	if (collide_player) {
		return false;
	}
	// Place the block
	world_set_block(place_x, place_y, place_z, place_block);
	return true;
}

bool remove_block(
	const player_t* p)
{
	block_raycast_hit_t hit = get_target_block(p);
	if (hit.hit) {
		world_set_block(hit.block_x, hit.block_y, hit.block_z, BLOCK_NONE);
		return true;
	}
	return false;
}

#ifdef DO_REDUCE_STORECMD
static inline void store2(uint16_t* p, uint16_t lo, uint16_t hi)
{
	*(uint32_t*)p = (uint32_t)lo | ((uint32_t)hi << 16); // little endian
}
#endif // DO_REDUCE_STORECMD

void __not_in_flash_func(upscale_2x_aa_partial)(const uint16_t* restrict src, uint16_t* restrict dst, int y_start, int y_end)
{
	if (y_start < 0) {
		y_start = 0;
	}
	if (y_end > DRAW_HEIGHT) {
		y_end = DRAW_HEIGHT;
	}
	if (y_start >= y_end) {
		return;
	}

	int y_main_end = (y_end > DRAW_HEIGHT - 1) ? (DRAW_HEIGHT - 1) : y_end;

	for (int y = y_start; y < y_main_end; y++) {
		const uint16_t* row0 = &src[y * DRAW_WIDTH];
		const uint16_t* row1 = &src[(y + 1) * DRAW_WIDTH];

		const DEPTH_TYPE* dep0 = &depth_buffer[y * DRAW_WIDTH];
		const DEPTH_TYPE* dep1 = &depth_buffer[(y + 1) *DRAW_WIDTH];

		uint16_t* dst_row0 = &dst[(y * 2) * LCD_WIDTH];
		uint16_t* dst_row1 = &dst[(y * 2 + 1) * LCD_WIDTH];

		for (int x = 0; x < DRAW_WIDTH - 1; x++) {

			uint16_t c00 = row0[x];
			uint16_t c10 = row0[x + 1];
			uint16_t c01 = row1[x];
			uint16_t c11 = row1[x + 1];

#ifndef DO_OPTIMIZE_UPSCALE_AA

			int dx = x << 1;

			// generate 4 pixels with anti-aliasing
			dst_row0[dx    ] = c00;
			dst_row0[dx + 1] = avg565_2(c00, c10);
			dst_row1[dx    ] = avg565_2(c00, c01);
			dst_row1[dx + 1] = avg565_4(c00, c10, c01, c11);
#else
			// d00|d10
			// ---+---
			// d01|d11
			DEPTH_TYPE d00 = dep0[x];
			DEPTH_TYPE d10 = dep0[x + 1] ;
			DEPTH_TYPE d01 = dep1[x];
			DEPTH_TYPE d11 = dep1[x + 1];

			// d00 <-h0-> d10
			//  |          |
			// v0         v1
			//  |          |
			// d01 <-h1-> d11
			bool h0 = depth_is_edge(d00, d10);
			bool h1 = depth_is_edge(d01, d11);
			bool v0 = depth_is_edge(d00, d01);
			bool v1 = depth_is_edge(d10, d11);
			bool h_any = h0 || h1;
			bool v_any = v0 || v1;

			int dx = x << 1;
			dst_row0[dx] = c00;

			if (h_any && !v_any) {
				// vertical edge
//				dst_row0[dx + 1] = avg565_2(c00, c10);
				dst_row0[dx + 1] = (d00 < d10) ? c00 : c10;
#ifndef DO_REDUCE_STORECMD
				dst_row1[dx + 1] = dst_row0[dx + 1];
				dst_row1[dx] = avg565_2(c00, c01);
#else
				store2(&dst_row1[dx], avg565_2(c00, c01), dst_row0[dx + 1]);
#endif // DO_REDUCE_STORECMD
			}
			else if (v_any && !h_any) {
				// horizonal edge
				dst_row0[dx + 1] = avg565_2(c00, c10);
#ifndef DO_REDUCE_STORECMD
//				dst_row1[dx] = avg565_2(c00, c01);
				dst_row1[dx] = (d00 < d01) ? c00: c01;
				dst_row1[dx + 1] = dst_row1[dx];
#else
				store2(&dst_row1[dx], (d00 < d01) ? c00: c01,  (d00 < d01) ? c00: c01);
#endif // DO_REDUCE_STORECMD
			}
			else if ((v0 && h0) && (!v1 && !h1)) {
				// o*
				// **
#ifndef DO_REDUCE_STORECMD
				dst_row0[dx] = avg565_2(c00, c11);
				dst_row0[dx + 1] = (d00 < d10) ? c00: c10;
				dst_row1[dx] = (d00 < d01) ? c00 : c01;
				dst_row1[dx + 1] = (d00 < d11) ? c00 : c11;
#else
				store2(&dst_row0[dx], avg565_2(c00, c11), (d00 < d10) ? c00: c10);
				store2(&dst_row1[dx], (d00 < d01) ? c00 : c01, (d00 < d11) ? c00 : c11);
#endif // DO_REDUCE_STORECMD
			}
			else if ((h0 && v1) && (!v0 && !h1)) {
				// *o
				// **
//				dst_row0[dx + 1] = (d00 < d10) ? c00 : c10;
				dst_row0[dx + 1] = avg565_2(c10, c01);
#ifndef DO_REDUCE_STORECMD
				dst_row1[dx] = (d00 < d01) ? c00 : c01;
				dst_row1[dx + 1] = (d00 < d11) ? c00 : c11;
#else
				store2(&dst_row1[dx], (d00 < d01) ? c00 : c01, (d00 < d11) ? c00 : c11);
#endif // DO_REDUCE_STORECMD
			}
			else if ((v0 && h1) && (!v1 && !h0)) {
				// **
				// o*
				dst_row0[dx + 1] = (d00 < d10) ? c00 : c10;
#ifndef DO_REDUCE_STORECMD
//				dst_row1[dx] = (d00 < d01)? c00 : c01;
				dst_row1[dx] = avg565_2(c10, c01);
				dst_row1[dx + 1] = (d00 < d11) ? c00 : c11;
#else
				store2(&dst_row1[dx], avg565_2(c10, c01), (d00 < d11) ? c00 : c11);
#endif // DO_REDUCE_STORECMD
			}
			else if ((v1 && h1) && (!v0 && !h0)) {
				// **
				// *o
				dst_row0[dx + 1] = (d00 < d10) ? c00 : c10;
#ifndef DO_REDUCE_STORECMD
				dst_row1[dx] = (d00 < d01) ? c00 : c01;
//				dst_row1[dx + 1] = (d00 < d11) ? c00 : c11;
				dst_row1[dx + 1] = avg565_2(c00, c11);
#else
				store2(&dst_row1[dx], (d00 < d01) ? c00 : c01, avg565_2(c00, c11));
#endif // DO_REDUCE_STORECMD
			}
			else {
				dst_row0[dx + 1] = avg565_2(c00, c10);
#ifndef DO_REDUCE_STORECMD
				dst_row1[dx] = avg565_2(c00, c01);
				dst_row1[dx + 1] = avg565_4(c00, c10, c01, c11);
#else
				store2(&dst_row1[dx], avg565_2(c00, c01), avg565_4(c00, c10, c01, c11));
#endif // DO_REDUCE_STORECMD
			}
			
#endif // DO_OPTIMIZE_UPSCALE_AA
		}

		// right edge completion
		int x = DRAW_WIDTH - 1;
		uint16_t c = row0[x];
		uint16_t d = row1[x];

#ifndef DO_REDUCE_STORECMD
		dst_row0[(x << 1)    ] = c;
		dst_row0[(x << 1) + 1] = c;
		dst_row1[(x << 1)    ] = avg565_2(c, d);
		dst_row1[(x << 1) + 1] = avg565_2(c, d);
#else
		store2(&dst_row0[(x << 1)], c, c);
		store2(&dst_row1[(x << 1)], avg565_2(c, d), avg565_2(c, d));
#endif // DO_REDUCE_STORECMD
	}

	// bottom row completion
	if (y_end == DRAW_HEIGHT) {
		int y = DRAW_HEIGHT - 1;
		const uint16_t* row = &src[y * DRAW_WIDTH];
		uint16_t* dst_row0 = &dst[(y * 2) * LCD_WIDTH];
		uint16_t* dst_row1 = &dst[(y * 2 + 1) * LCD_WIDTH];

		for (int x = 0; x < DRAW_WIDTH; x++) {
			uint16_t c = row[x];
#ifndef DO_REDUCE_STORECMD
			dst_row0[(x << 1)    ] = c;
			dst_row0[(x << 1) + 1] = c;
			dst_row1[(x << 1)    ] = c;
			dst_row1[(x << 1) + 1] = c;
#else
			store2(&dst_row0[(x << 1)], c, c);
			store2(&dst_row1[(x << 1)], c, c);
#endif // DO_REDUCE_STORECMD
		}
	}
}

void __not_in_flash_func(draw_block_select)(uint16_t* screen_buffer)
{
	static uint8_t start_idx = BLOCK_SOIL;

	for (int y = LCD_HEIGHT - 20; y < LCD_HEIGHT; y++) {
		for (int x = 0; x < LCD_WIDTH; x++) {
			screen_buffer[y * LCD_WIDTH + x] = 0xFFFF;
		}
	}

	for (int y = 0; y < TEX_SIZE; y++) {
		int buffy = LCD_HEIGHT - 19 + y;
		for (int idx = start_idx; idx < (start_idx + 17); idx++) {
			for (int x = 0; x < TEX_SIZE; x++) {
				int buffx = (idx - 1) * 18 + 2 + x;

#ifdef DO_TEXTURE_SAMPLING
				if (idx <= BLOCK_RAINBOW) {
					screen_buffer[buffy * LCD_WIDTH + buffx] = tex_blocks[idx][(TEX_SIZE - y - 1) * TEX_SIZE + x];
				}
				else {
					screen_buffer[buffy * LCD_WIDTH + buffx] = block_type_to_rgb565(idx);
				}
#else
				screen_buffer[buffy * LCD_WIDTH + buffx] = block_type_to_rgb565(idx);
#endif // DO_TEXTURE_SAMPLING
			}


		}
	}
	uint16_t select_color = color_rgb565(0, 128, 255);
	for (int x = 0; x < TEX_SIZE; x++) {
		int buffx = (current_block - 1) * 18 + 2 + x;
		screen_buffer[(LCD_HEIGHT - 2) * LCD_WIDTH + buffx] = select_color;
		screen_buffer[(LCD_HEIGHT - 1) * LCD_WIDTH + buffx] = select_color;
	}
}


void renderer_draw_string(draw_string_t attr, const char* str, uint16_t* buffer, int width, int height)
{
	if (str == NULL || buffer == NULL || attr.fontdata == NULL || attr.opacity == 0) {
		return;
	}

	const sk_font_t* font = attr.fontdata;
	const int fw = (int)font->width;
	const int fh = (int)font->height;
	const int bytes_per_row = (fw + 7) / 8;
	const int bytes_per_char = bytes_per_row * fh;

	int pen_x = attr.posx;
	int pen_y = attr.posy;

	for (const char* p = str; *p != '\0'; p++) {
		unsigned char ch = (unsigned char)*p;

		if (ch == '\n') {
			pen_x = attr.posx;
			pen_y += fh;
			continue;
		}
		if (ch < ' ' || ch > '~') {
			ch = ' ';
		}
		if (pen_y >= height) {
			break;
		}

		if (pen_x + fw > 0 && pen_x < width && pen_y + fh > 0) { 
			const uint8_t* glyph = &font->data[(ch - ' ') * bytes_per_char];

			for (int row = 0; row < fh; row++) {
				int dy = pen_y + row;
				if (dy < 0 || dy >= height) {
					continue;
				}

				const uint8_t* line = &glyph [row * bytes_per_row];
				uint16_t* dst_row = &buffer[dy * width];

				for (int col = 0; col < fw; col++) {
					if ((line[col >> 3] & (0x80 >> (col & 7))) == 0) {
						continue;
					}
	
					int dx = pen_x + col;
					if (dx < 0 || dx >= width) {
						continue;
					}

					dst_row[dx] = (attr.opacity == 255)
						? attr.color
						: color_lerp565(dst_row[dx], attr.color, attr.opacity);
				}
			}
		}
		pen_x += fw;
	}
}				