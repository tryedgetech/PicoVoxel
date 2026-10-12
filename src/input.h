// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] input.h - Input handling header file

#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/adc.h"
#include "pico/stdlib.h"

// PS4 controller report data indices and button masks
#define PSCON_IDX_ANALOG_LX   (1)
#define PSCON_IDX_ANALOG_LY   (2)
#define PSCON_IDX_ANALOG_RX   (3)
#define PSCON_IDX_ANALOG_RY   (4)
#define PSCON_IDX_BUTTON_1    (5)
#define PSCON_IDX_BUTTON_2    (6)
#define PSCON_IDX_MAX         (7)

#define PSCON_MASK_BTN1_X     (0x20)	// Cross button
#define PSCON_MASK_BTN1_CIR   (0x40)    // Circle button
#define PSCON_MASK_BTN1_TRI   (0x80)    // Triangle button
#define PSCON_MASK_BTN1_SQR   (0x10)    // Square button

#define PSCON_MASK_BTN2_L1    (0x01)    // L1 button
#define PSCON_MASK_BTN2_R1    (0x02)    // R1 button
#define PSCON_MASK_BTN2_L2    (0x04)    // L2 button
#define PSCON_MASK_BTN2_R2    (0x08)    // R2 button
#define PSCON_MASK_BTN2_SHARE (0x10)    // Share button
#define PSCON_MASK_BTN2_OPT   (0x20)    // Options button
#define PSCON_MASK_BTN2_L3    (0x40)    // L3 button
#define PSCON_MASK_BTN2_R3    (0x80)    // R3 button

// DS4 D-Pad is a 4bit hat value (lower nibble of BUTTON_1), not a bitmask.
#define PSCON_MASK_BTN1_HAT    (0x0F)  // Bitmask to extract the hat nibble
#define PSCON_HAT_N            (0)     // Up
#define PSCON_HAT_NE           (1)     // Up-Right
#define PSCON_HAT_E            (2)     // Right
#define PSCON_HAT_SE           (3)     // Down-Right
#define PSCON_HAT_S            (4)     // Down
#define PSCON_HAT_SW           (5)     // Down-Left
#define PSCON_HAT_W            (6)     // Left
#define PSCON_HAT_NW           (7)     // Up-Left
#define PSCON_HAT_NONE         (8)     // Released


typedef struct {
	uint8_t report[PSCON_IDX_MAX];
	uint8_t set_rumble : 1;
	uint8_t motor_right;
	uint8_t motor_left;
	bool mounted;
} pscon_local_t;

extern pscon_local_t pscon_local;

typedef struct {
	float turn_axis;
	float turn_pitch;
	float move_axis;
	bool jump_pressed;
	bool jump_held;
	uint8_t btn1_current;
	uint8_t btn2_current;
	uint8_t btn1_prev;
	uint8_t btn2_prev;
} joystick_state_t;

void input_init(void);
void input_read(joystick_state_t *joystick_state);

static inline uint16_t read_adc_channel(unsigned int adc_input)
{
	adc_select_input(adc_input);
	sleep_us(5);
	return adc_read();
}

static inline bool is_button_pressed(const joystick_state_t* joystick_state, int index, int bitmask)
{
	if (index == PSCON_IDX_BUTTON_1) {
		return ((joystick_state->btn1_current & bitmask) == bitmask) && !((joystick_state->btn1_prev & bitmask) == bitmask);
	}
	else if (index == PSCON_IDX_BUTTON_2) {
		return ((joystick_state->btn2_current & bitmask) == bitmask) && !((joystick_state->btn2_prev & bitmask) == bitmask);
	}
	return false;
}

static inline bool is_button_held(const joystick_state_t* joystick_state, int index, int bitmask)
{
	if (index == PSCON_IDX_BUTTON_1) {
		return (joystick_state->btn1_current & bitmask) == bitmask;
	}
	else if (index == PSCON_IDX_BUTTON_2) {
		return (joystick_state->btn2_current & bitmask) == bitmask;
	}
	return false;
}

// D-Pad is reported as a 4bit hat value, so it must be compared by value, not by bitmask.
static inline bool is_dpad_pressed(const joystick_state_t* joystick_state, int hat_value)
{
	uint8_t current = joystick_state->btn1_current & PSCON_MASK_BTN1_HAT;
	uint8_t prev = joystick_state->btn1_prev & PSCON_MASK_BTN1_HAT;

	return (current == hat_value) && (prev != hat_value);
}

static inline bool is_dpad_held(const joystick_state_t* joystick_state, int hat_value)
{
	return (joystick_state->btn1_current & PSCON_MASK_BTN1_HAT) == hat_value;
}

#endif // INPUT_H
