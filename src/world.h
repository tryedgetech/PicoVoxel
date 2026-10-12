// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] world.h - World utility functions header file

#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>
#include <stdint.h>
#include "pico.h"

#include "app_config.h"

extern uint8_t world_blocks[WORLD_SIZE_Y][WORLD_SIZE_Z][WORLD_SIZE_X];

#ifdef DO_MACROCELL_SKIP
#if (MACRO_CELL_SIZE * MACRO_CELL_SIZE * MACRO_CELL_SIZE) > 255
extern uint16_t world_macrocell_count[MACRO_SIZE_Y][MACRO_SIZE_Z][MACRO_SIZE_X];
#else
extern uint8_t world_macrocell_count[MACRO_SIZE_Y][MACRO_SIZE_Z][MACRO_SIZE_X];
#endif
#endif // DO_MACROCELL_SKIP

#ifdef DO_BLOCK_SHADOW
// Bit layout of one world_block_shadow element
#define WORLD_SHADOW_LOW_MASK   0x00FFu // low 8 bits: Y coordinate of the topmost block
#define WORLD_SHADOW_HIGH_MASK  0xFF00u // high 8 bits: neighbor flags
#define WORLD_SHADOW_NEG_X_BIT  0x0100u // -X side: neighbor column is higher
#define WORLD_SHADOW_POS_X_BIT  0x0200u // +X side: neighbor column is higher
#define WORLD_SHADOW_NEG_Z_BIT  0x0400u // -Z side: neighbor column is higher
#define WORLD_SHADOW_POS_Z_BIT  0x0800u // +Z side: neighbor column is higher

extern uint16_t world_block_shadow[WORLD_SIZE_Z][WORLD_SIZE_X];

static inline uint16_t world_get_block_shadow(int block_x, int block_z)
{
    if (block_x < 0 || block_x >= WORLD_SIZE_X || block_z < 0 || block_z >= WORLD_SIZE_Z) {
        return WORLD_SHADOW_LOW_MASK;
    }
    return world_block_shadow[block_z][block_x];
}
#endif // DO_BLOCK_SHADOW

void world_load_initial_model(const uint8_t *initial_model, int count);
void world_load_compressed_model(const uint8_t *compressed_model, int count);
uint8_t __not_in_flash_func(world_get_block)(int block_x, int block_y, int block_z);
bool world_set_block(int block_x, int block_y, int block_z, uint8_t block_type);
bool world_block_is_solid_at(int block_x, int block_y, int block_z);
bool world_block_is_visible_at(int block_x, int block_y, int block_z);
float world_find_ground_eye_y(float position_x, float position_z, float position_y);

#endif // WORLD_H
