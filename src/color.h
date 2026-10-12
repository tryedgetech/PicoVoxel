// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] color.h - Color utility functions header file

#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>
#include <stdbool.h>

// macros for converting RGB values to 16-bit RGB565 format
#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

static inline uint16_t color_rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
	return  (uint16_t)((red   & 0xF8u) << 8) |
			(uint16_t)((green & 0xFCu) << 3) |
			(uint16_t)( blue           >> 3);
}

static inline uint16_t color_scale_rgb565(uint16_t color, uint8_t scale)
{
	uint16_t red   = (uint16_t)((color >> 11) & 0x1Fu);
	uint16_t green = (uint16_t)((color >>  5) & 0x3Fu);
	uint16_t blue  = (uint16_t)( color        & 0x1Fu);

	red   = (uint16_t)((red   * scale) >> 8);
	green = (uint16_t)((green * scale) >> 8);
	blue  = (uint16_t)((blue  * scale) >> 8);

	return (uint16_t)((red << 11) | (green << 5) | blue);
}

#endif // COLOR_H
