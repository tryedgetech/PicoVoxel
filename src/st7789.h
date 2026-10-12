// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] st7789.h - ST7789 display driver header file

#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>


// Initialize the ST7735S LCD
void lcd_init(void);

// Send framebuffer to LCD via DMA
void lcd_send_framebuffer(const uint16_t* fb);

// Wait for DMA transfer
void lcd_wait_dma(void);

// Fill screen
void lcd_fill(uint16_t color);

void lcd_display(uint16_t* buff);

// Draw grayscale and color sample pattern
void lcd_draw_color_test_patterns(void);

#endif // ST7789_H
