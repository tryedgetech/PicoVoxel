// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] block.h - Block type header file

#ifndef BLOCK_H
#define BLOCK_H

#include <stdbool.h>
#include <stdint.h>
#include "color.h"

typedef enum {
	BLOCK_NONE     = 0,
	BLOCK_SOIL     = 1,
	BLOCK_TRUNK    = 2,
	BLOCK_LEAF     = 3,
	BLOCK_CONCRETE = 4,
	BLOCK_STONE    = 5,
	BLOCK_SAND     = 6,
	BLOCK_SEA      = 7,
	BLOCK_FALL     = 8,
	BLOCK_RAINBOW  = 9,
	BLOCK_WHITE,
	BLOCK_BLACK,
	BLOCK_GRAY,
	BLOCK_RED,
	BLOCK_GREEN,
	BLOCK_BLUE,
	BLOCK_ORANGE,
	BLOCK_YELLOW,

	BLOCK_MAX,
} block_type_t;

bool block_type_is_solid(uint8_t block_type);
bool block_type_is_visible(uint8_t block_type);

static inline uint16_t block_type_to_rgb565(uint8_t block_type)
{
	switch (block_type) {
	case BLOCK_SOIL:
		return color_rgb565(202, 162, 132);
	case BLOCK_TRUNK:
		return color_rgb565(200, 160, 200);
	case BLOCK_LEAF:
		return color_rgb565(110, 180, 114);
	case BLOCK_CONCRETE:
		return color_rgb565(200, 202, 206);
	case BLOCK_STONE:
		return color_rgb565(120, 122, 126);
	case BLOCK_SAND:
		return color_rgb565(245, 231, 218);
	case BLOCK_SEA:
		return color_rgb565(102, 138, 210);
	case BLOCK_FALL:
		return color_rgb565(230, 255, 255);
	case BLOCK_RAINBOW:
		return color_rgb565(255,  60, 255);
	case BLOCK_WHITE:
		return color_rgb565(240, 240, 240);
	case BLOCK_BLACK:
		return color_rgb565( 50,  50,  50);
	case BLOCK_GRAY:
		return color_rgb565(180, 180, 180);
	case BLOCK_RED:
		return color_rgb565(255,  60,  60);
	case BLOCK_GREEN:
		return color_rgb565( 60, 255,  60);
	case BLOCK_BLUE:
		return color_rgb565( 60,  60, 255);
	case BLOCK_ORANGE:
		return color_rgb565(255, 165,  40);
	case BLOCK_YELLOW:
		return color_rgb565(255, 220,  40);
	default:
		return color_rgb565(  0,   0,   0);
	}
}

#endif // BLOCK_H
