// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] app_config.h - Application configuration header file

#ifndef APP_CONFIG_H
#define APP_CONFIG_H


// DO_* feature macros are split out into config/features_*.h.
// PICOVOXEL_FEATURE_CONFIG is passed by CMake as a compile definition (e.g. "config/features_minimal.h").
#ifndef PICOVOXEL_FEATURE_CONFIG
#include "config/features_default.h"
#else
#include PICOVOXEL_FEATURE_CONFIG
#endif
// Cross-preset dependency checks (apply regardless of which preset is selected)
#if defined (DO_MACROCELL_OPTIMIZE) && !defined(DO_MACROCELL_SKIP)
#error "DO_MACROCELL_OPTIMIZE requires DO_MACROCELL_SKIP"
#endif
#if defined(DO_TEXTURE_ANIMATION) && !defined(DO_TEXTURE_SAMPLING)
#error "DO_TEXTURE_ANIMATION requires DO_TEXTURE_SAMPLING"
#endif
#if defined(DO_TEXSAMPLING_INTERP) && !defined(DO_TEXTURE_SAMPLING)
#error "DO_TEXSAMPLING_INTERP requires DO_TEXTURE_SAMPLING"
#endif
#if defined(DO_PASSTHROUGH_BLOCK) && !defined(DO_TEXTURE_SAMPLING)
#error "DO_PASSTHROUGH_BLOCK requires DO_TEXTURE_SAMPLING"
#endif


#define LCD_WIDTH       320
#define LCD_HEIGHT      240
#define DRAW_WIDTH      160
#define DRAW_HEIGHT     120

#define LCD_PIN_DC       17 //  8   // (LCD_DC ) Data/Command
#define LCD_PIN_CS       20 //  9   // (LCD_CS ) Chip Select,low active
#define LCD_PIN_SCK      18 // 10   // (LCD_CLK) Serial Clock 
#define LCD_PIN_MOSI     19 // 11   // (LCD_DIN) Master Out Slave In 
#define LCD_PIN_RST      21 // 12   // (LCD_RST) Reset
#define LCD_PIN_BL       22 // 13   // (LCD_BL ) Backlight

#define LCD_SPI_BAUDRATE (75 * 1000 * 1000) // 75 MHz - SPI baudrate
#define LCD_SPI spi0     // SPI instance

#define JOYSTICK_ADC_X_INPUT    0
#define JOYSTICK_ADC_X_GPIO    26
#define JOYSTICK_ADC_Y_INPUT    1
#define JOYSTICK_ADC_Y_GPIO    27
#define JOYSTICK_BUTTON_GPIO   15
#define JOYSTICK_ADC_CENTER  2800.0f
#define JOYSTICK_DEAD_ZONE      0.14f
#define JOYSTICK_X_INVERT       0
#define JOYSTICK_Y_INVERT       1

#define PSCON_ANALOG_CENTER  2048.0f
#define PSCON_DEAD_ZONE         0.14f
#define PSCON_X_INVERT          0
#define PSCON_Y_INVERT          1

#define WORLD_SIZE_X 64
#define WORLD_SIZE_Z 64
#define WORLD_SIZE_Y 32

#ifdef DO_MACROCELL_SKIP
// Macrocell (empty-space skipping)
#define MACRO_CELL_SHIFT 2
#define MACRO_CELL_SIZE (1 << MACRO_CELL_SHIFT)
#define MACRO_SIZE_X    (WORLD_SIZE_X >> MACRO_CELL_SHIFT)  // 16
#define MACRO_SIZE_Y    (WORLD_SIZE_Y >> MACRO_CELL_SHIFT)  //  4
#define MACRO_SIZE_Z    (WORLD_SIZE_Z >> MACRO_CELL_SHIFT)  // 16
#endif // DO_MACROCELL_SKIP

#define RENDER_HORIZONTAL_FOV_DEGREES 90.0f
#define RENDER_MAX_DDA_STEPS 192

#define PLAYER_START_X     28.0f
#define PLAYER_START_Z     28.0f
#define PLAYER_START_YAW    0.67f
#define PLAYER_START_PITCH  0.0f
#define PLAYER_START_ROLL   0.0f
#define PLAYER_EYE_HEIGHT   1.62f
#define PLAYER_BODY_HEIGHT  1.80f
#define PLAYER_RADIUS       0.26f
#define PLAYER_MOVE_SPEED   4.2f
#define PLAYER_TURN_SPEED   2.8f
#define PLAYER_JUMP_SPEED   9.2f   // 5.2f
#define PLAYER_GRAVITY     13.0f

#define AUDIO_PIN   16 // PWM audio output pin

#define WATCHDOG_TIMEOUT_MS  2000
#define WATCHDOG_TRACE_START   (0x01 << 31)
#define WATCHDOG_TRACE_END     (0x01 << 30)
#define WATCHDOG_TRACE_RENDER  (0x01 <<  8)
#define WATCHDOG_TRACE_INTERP  (0x01 <<  9)
#define WATCHDOG_TRACE_UPSCALE (0x01 << 10)

#endif // APP_CONFIG_H
