// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] input.c - Input handling implementation

#include "input.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

pscon_local_t pscon_local;

static bool previous_button_down;

static float clamp_axis(float axis_value)
{
	if (axis_value > 1.0f) {
		return 1.0f;
	}
	if (axis_value < -1.0f) {
		return -1.0f;
	}
	return axis_value;
}

#ifdef DO_JOYSTICK_CONTROL
static float normalize_adc_axis(uint16_t raw_value, bool invert_axis)
{
	float axis_value = ((float)raw_value - JOYSTICK_ADC_CENTER) / JOYSTICK_ADC_CENTER;
	axis_value = clamp_axis(axis_value);
	if (invert_axis) {
		axis_value = -axis_value;
	}

	float axis_magnitude = fabsf(axis_value);
	if (axis_magnitude < JOYSTICK_DEAD_ZONE) {
		return 0.0f;
	}

	float scaled_magnitude = (axis_magnitude - JOYSTICK_DEAD_ZONE) / (1.0f - JOYSTICK_DEAD_ZONE);
	return axis_value < 0.0f ? -scaled_magnitude : scaled_magnitude;
}
#endif // DO_JOYSTICK_CONTROL

void input_init(void)
{
	adc_init();

#ifdef DO_JOYSTICK_CONTROL
	adc_gpio_init(JOYSTICK_ADC_X_GPIO);
	adc_gpio_init(JOYSTICK_ADC_Y_GPIO);

	gpio_init(JOYSTICK_BUTTON_GPIO);
	gpio_set_dir(JOYSTICK_BUTTON_GPIO, GPIO_IN);
	gpio_pull_up(JOYSTICK_BUTTON_GPIO);
#endif // DO_JOYSTICK_CONTROL

	memset(pscon_local.report, 0, sizeof (pscon_local.report));
	pscon_local.set_rumble = 0;
	pscon_local.motor_right = 0;
	pscon_local.motor_left = 0;
	pscon_local.mounted = false;

	previous_button_down = false;
}

static float normalize_pscon_axis(uint16_t raw_value, bool invert_axis)
{
	float axis_value = ((float)raw_value - PSCON_ANALOG_CENTER) / PSCON_ANALOG_CENTER;
	axis_value = clamp_axis(axis_value);
	if (invert_axis) {
		axis_value = -axis_value;
	}

	float axis_magnitude = fabsf(axis_value);
	if (axis_magnitude < PSCON_DEAD_ZONE) {
		return 0.0f;
	}

	float scaled_magnitude = (axis_magnitude - PSCON_DEAD_ZONE) / (1.0f - PSCON_DEAD_ZONE);
	return axis_value < 0.0f ? -scaled_magnitude : scaled_magnitude;
}

uint16_t read_pscon_analog(int index)
{
	if (!pscon_local.mounted) {
		return PSCON_ANALOG_CENTER;
	}

	uint16_t axis_value = (uint16_t)(pscon_local.report[index]) << 4;
	return axis_value;
}

bool read_pscon_button(int index, int bitmask)
{
	if (!pscon_local.mounted) {
		return false;
	}

	return (pscon_local.report[index] & bitmask) == bitmask;
}

uint8_t read_pscon_button_status(int index)
{
	if (!pscon_local.mounted) {
		return 0;
	}

	return (pscon_local.report[index]);
}

void input_read(joystick_state_t *joystick_state)
{
#ifdef DO_JOYSTICK_CONTROL
	bool button_down = !gpio_get(JOYSTICK_BUTTON_GPIO);
	uint16_t raw_x = read_adc_channel(JOYSTICK_ADC_X_INPUT);
	uint16_t raw_y = read_adc_channel(JOYSTICK_ADC_Y_INPUT);

	joystick_state->turn_axis    = normalize_adc_axis(raw_x, JOYSTICK_X_INVERT != 0);
	joystick_state->move_axis    = normalize_adc_axis(raw_y, JOYSTICK_Y_INVERT != 0);
#else
	bool button_down = read_pscon_button(PSCON_IDX_BUTTON_1, PSCON_MASK_BTN1_X);
	joystick_state->turn_axis    = 
			normalize_pscon_axis(read_pscon_analog(PSCON_IDX_ANALOG_LX), PSCON_X_INVERT != 0) +
			normalize_pscon_axis(read_pscon_analog(PSCON_IDX_ANALOG_RX), PSCON_X_INVERT != 0);
	joystick_state->turn_pitch   = normalize_pscon_axis(read_pscon_analog(PSCON_IDX_ANALOG_RY), PSCON_Y_INVERT != 0);
	joystick_state->move_axis    = normalize_pscon_axis(read_pscon_analog(PSCON_IDX_ANALOG_LY), PSCON_Y_INVERT != 0);
	joystick_state->btn1_prev    = joystick_state->btn1_current;
	joystick_state->btn2_prev    = joystick_state->btn2_current;
	joystick_state->btn1_current = read_pscon_button_status(PSCON_IDX_BUTTON_1);
	joystick_state->btn2_current = read_pscon_button_status(PSCON_IDX_BUTTON_2);
#endif // DO_JOYSTICK_CONTROL

	joystick_state->jump_pressed = button_down && ! previous_button_down;
	joystick_state->jump_held    = button_down;

	previous_button_down = button_down;
}
