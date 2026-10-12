// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] rle_comp.c - RLE compression and decompression implementation

#include "rle_comp.h"


int rle_compress(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size)
{
    size_t srcidx = 0;
    size_t dstidx = 0;

    while (srcidx < src_size) {
        size_t run = 1;

        while (((srcidx + run) < src_size) && (src[srcidx + run] == src[srcidx]) && (run < 127)) {
            // count the number of consecutive identical bytes
            run++;
        }

        if (run >= 2) {
            // If it occurs two or more times in a row, it is stored as RLE
            if ((dstidx + 2) > dst_size) {
                return -1;
            }
            dst[dstidx++] = (uint8_t)(0x80 | run);
            dst[dstidx++] = src[srcidx];

            srcidx += run;
        }
        else {
            // If it occurs only once, it is stored as a literal
            size_t literal_start = srcidx;
            size_t literal_count = 0;

            while (((srcidx + literal_count) < src_size) && (literal_count < 127)) {
                if (((srcidx + 1) < src_size) && (src[srcidx] == src[srcidx + 1])) {
                    // If the next byte is the same, we stop the literal and switch to RLE
                    break;
                }

                srcidx++;
                literal_count++;
            }
            if (literal_count == 0) {
                return -2;
            }

            if ((dstidx + 1 + literal_count) > dst_size) {
                return -1;
            }

            dst[dstidx++] = (uint8_t)literal_count;

            for (size_t i = 0; i < literal_count; i++) {
                dst[dstidx++] = src[literal_start + i];
            }
        }
    }

    return (int)dstidx;
}


int rle_decompress(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size)
{
    size_t srcidx = 0;
    size_t dstidx = 0;

    while (srcidx < src_size) {
        uint8_t control = src[srcidx++];
        if (control & 0x80) {
            // RLE
            size_t count = control & 0x7F;
            if (count == 0) {
                // Invalid control byte (0x80) indicates
                return -2;
            }

            if (srcidx >= src_size) {
                return -2;
            }
            uint8_t value = src[srcidx++];

            if ((dstidx + count) > dst_size) {
                return -1;
            }
            for (size_t i = 0; i < count; i++) {
                dst[dstidx++] = value;
            }
        }
        else {
            // Literal
            size_t count = control;
            if (count == 0) {
                return -2;
            }
            if (srcidx + count > src_size) {
                return -2;
            }

            if ((dstidx + count) > dst_size) {
                return -1;
            }
            for (size_t i = 0; i < count; i++) {
                dst[dstidx++] = src[srcidx++];
            }
        }
    }

    return (int)dstidx;
}