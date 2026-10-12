// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] block.c - Block type utility functions

#include "block.h"
#include "color.h"

bool block_type_is_solid(uint8_t block_type)
{
	return block_type != BLOCK_NONE && block_type != BLOCK_SEA && block_type != BLOCK_FALL;
}

bool block_type_is_visible(uint8_t block_type)
{
	return block_type != BLOCK_NONE;
}
