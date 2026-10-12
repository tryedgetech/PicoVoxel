// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// sk_fonts.h - SK Font definitions header file

#ifndef SK_FONTS_H
#define SK_FONTS_H

#include <stdint.h>

typedef struct {
    uint16_t width;       // width
    uint16_t height;      // height
    const uint8_t* data;  // SK font data
} sk_font_t;

extern sk_font_t skfont10;
extern sk_font_t skfont12;
extern sk_font_t skfont16;
  
#endif // SK_FONTS_H
