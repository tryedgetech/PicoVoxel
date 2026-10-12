// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] world.c - World utility functions implementation

#include "world.h"

#include <math.h>
#include <string.h>
#include "pico.h"

#include "app_config.h"
#include "block.h"
#include "rle_comp.h"

uint8_t world_blocks[WORLD_SIZE_Y][WORLD_SIZE_Z][WORLD_SIZE_X];

#ifdef DO_MACROCELL_SKIP
#if (MACRO_CELL_SIZE * MACRO_CELL_SIZE * MACRO_CELL_SIZE) > 255
uint16_t world_macrocell_count[MACRO_SIZE_Y][MACRO_SIZE_Z][MACRO_SIZE_X];  // Macrocell occupancy counts
#else
uint8_t world_macrocell_count[MACRO_SIZE_Y][MACRO_SIZE_Z][MACRO_SIZE_X];  // Macrocell occupancy counts
#endif

static void macrocell_rebuild(void)
{
	memset(world_macrocell_count, 0, sizeof(world_macrocell_count));
	for (int y = 0; y < WORLD_SIZE_Y; y++) {
		for (int z = 0; z < WORLD_SIZE_Z; z++) {
			for (int x = 0; x < WORLD_SIZE_X; x++) {
				if (world_blocks[y][z][x] != BLOCK_NONE) {
					world_macrocell_count[y >> MACRO_CELL_SHIFT][z >> MACRO_CELL_SHIFT][x >> MACRO_CELL_SHIFT]++;
				}
			}
		}
	}
}
#endif // DO_MACROCELL_SKIP


#ifdef DO_BLOCK_SHADOW
uint16_t world_block_shadow[WORLD_SIZE_Z][WORLD_SIZE_X];

// Returns the Y coordinate of the topmost block in column (x,z)
static int shadow_column_top_y(int x, int z)
{
	if (x < 0 || x >= WORLD_SIZE_X || z < 0 || z >= WORLD_SIZE_Z) {
		return 0;
	}
	for (int y = WORLD_SIZE_Y - 1; y >= 0; y--) {
		if (world_blocks[y][z][x] != BLOCK_NONE) {
			return y;
		}
	}
	return 0;
}

// Recalculates the shadow value for column (x,z) (low 8 bits: top Y, high 8 bits: neighbor flags)
static void shadow_update_column(int x, int z)
{
	if (x < 0 || x >= WORLD_SIZE_X || z < 0 || z >= WORLD_SIZE_Z) {
		return;
	}
	int current_top = shadow_column_top_y(x, z);
	uint16_t shadow_value = (uint16_t)current_top & WORLD_SHADOW_LOW_MASK;

	if (shadow_column_top_y(x - 1, z) > current_top) {
		shadow_value |= WORLD_SHADOW_NEG_X_BIT; // -X side: neighbor column is higher
	}
	if (shadow_column_top_y(x + 1, z) > current_top) {
		shadow_value |= WORLD_SHADOW_POS_X_BIT; // +X side
	}
	if (shadow_column_top_y(x, z - 1) > current_top) {
		shadow_value |= WORLD_SHADOW_NEG_Z_BIT; // -Z side
	}
	if (shadow_column_top_y(x, z + 1) > current_top) {
		shadow_value |= WORLD_SHADOW_POS_Z_BIT; // +Z side
	}
    world_block_shadow[z][x] = shadow_value;
}

static void shadow_rebuild(void)
{
    for (int z = 0; z < WORLD_SIZE_Z; z++) {
        for (int x = 0; x < WORLD_SIZE_X; x++) {
            shadow_update_column(x, z);
        }
    }
}
#endif // DO_BLOCK_SHADOW


void world_load_initial_model(const uint8_t *initial_model, int count)
{
	if (initial_model == NULL || count <= 0 || count > (int)sizeof(world_blocks)) {
		return;
	}

	memset(world_blocks, 0, sizeof(world_blocks));
	memcpy(world_blocks, initial_model, count);

#ifdef DO_MACROCELL_SKIP
	macrocell_rebuild();
#endif // DO_MACROCELL_SKIP
#ifdef DO_BLOCK_SHADOW
	shadow_rebuild();
#endif // DO_BLOCK_SHADOW
}

void world_load_compressed_model(const uint8_t *compressed_model, int count)
{
	if (compressed_model == NULL || count <= 0 || count > (int)sizeof(world_blocks)) {
		return;
	}

	memset(world_blocks, 0, sizeof(world_blocks));
	size_t decompressed_size = WORLD_SIZE_Y * WORLD_SIZE_Z * WORLD_SIZE_X;
	int result = rle_decompress(compressed_model, count, (uint8_t*)world_blocks, decompressed_size);
	if (result < 0) {
		// Handle decompression error
		memset(world_blocks, 0, sizeof(world_blocks));
	}

#ifdef DO_MACROCELL_SKIP
	macrocell_rebuild();
#endif // DO_MACROCELL_SKIP
#ifdef DO_BLOCK_SHADOW
	shadow_rebuild();
#endif // DO_BLOCK_SHADOW

}

uint8_t __not_in_flash_func(world_get_block)(int block_x, int block_y, int block_z)
{
	if (block_x < 0 || block_x >= WORLD_SIZE_X ||
		block_y < 0 || block_y >= WORLD_SIZE_Y ||
		block_z < 0 || block_z >= WORLD_SIZE_Z) {
		return BLOCK_NONE;
	}
	return world_blocks[block_y][block_z][block_x];
}

bool world_set_block(int block_x, int block_y, int block_z, uint8_t block_type)
{
	if (block_x < 0 || block_x >= WORLD_SIZE_X ||
		block_y < 0 || block_y >= WORLD_SIZE_Y ||
		block_z < 0 || block_z >= WORLD_SIZE_Z) {
		return false;
	}

#ifdef DO_MACROCELL_SKIP
	uint8_t old_type = world_blocks[block_y][block_z][block_x];
#endif // DO_MACROCELL_SKIP
	world_blocks[block_y][block_z][block_x] = block_type;
	
#ifdef DO_MACROCELL_SKIP
	// Maintain macrocell occupancy count
	if (old_type == BLOCK_NONE && block_type != BLOCK_NONE) {
		world_macrocell_count[block_y >> MACRO_CELL_SHIFT][block_z >> MACRO_CELL_SHIFT][block_x >> MACRO_CELL_SHIFT]++;
	}
	else if (old_type != BLOCK_NONE && block_type == BLOCK_NONE) {
		world_macrocell_count[block_y >> MACRO_CELL_SHIFT][block_z >> MACRO_CELL_SHIFT][block_x >> MACRO_CELL_SHIFT]--;
	}
#endif // DO_MACROCELL_SKIP

#ifdef DO_BLOCK_SHADOW
    // Recalculate the shadow bits for the changed column and its 4 neighbors
    shadow_update_column(block_x, block_z);
    shadow_update_column(block_x - 1, block_z);
    shadow_update_column(block_x + 1, block_z);
    shadow_update_column(block_x, block_z - 1);
    shadow_update_column(block_x, block_z + 1);
#endif // DO_BLOCK_SHADOW

	return true;
}

bool world_block_is_solid_at(int block_x, int block_y, int block_z)
{
	return block_type_is_solid(world_get_block(block_x, block_y, block_z));
}

bool world_block_is_visible_at(int block_x, int block_y, int block_z)
{
	return block_type_is_visible(world_get_block(block_x, block_y, block_z));
}

float world_find_ground_eye_y(float position_x, float position_z, float position_y)
{
	int block_x = (int)floorf(position_x);
	int block_z = (int)floorf(position_z);
	int start_y = (int)floorf(position_y);
	if (start_y >= WORLD_SIZE_Y) {
		start_y = WORLD_SIZE_Y - 1;
	}

	if (block_x < 0 || block_x >= WORLD_SIZE_X || block_z < 0 || block_z >= WORLD_SIZE_Z) {
		return PLAYER_EYE_HEIGHT;
	}

	for (int block_y = start_y; block_y >= 0; --block_y) {
		if (world_block_is_solid_at(block_x, block_y, block_z)) {
			return (float)(block_y + 1) + PLAYER_EYE_HEIGHT;
		}
	}

	return PLAYER_EYE_HEIGHT;
}
