// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] rle_comp.h - RLE compression and decompression header file

#ifndef RLE_COMP_H
#define RLE_COMP_H

#include <stdint.h>
#include <stddef.h>


// RLE compression
int rle_compress(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size);

// RLE decompression
int rle_decompress(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size);

#endif // RLE_COMP_H
