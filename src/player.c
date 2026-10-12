// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] player.c - Player utility functions

#include <math.h>

#include "app_config.h"
#include "player.h"
#include "renderer.h"
#include "world.h"
#include "block.h"
#include "sound.h"
#include "audio_action.h"


uint8_t current_block = BLOCK_CONCRETE;

#define TWO_PI_RADIANS 6.28318531f

static float clamp_float(float value, float minimum_value, float maximum_value)
{
	if (value < minimum_value) {
		return minimum_value;
	}
	if (value > maximum_value) {
		return maximum_value;
	}
	return value;
}

static float wrap_angle(float angle_radians)
{
	while (angle_radians < 0.0f) {
		angle_radians += TWO_PI_RADIANS;
	}
	while (angle_radians >= TWO_PI_RADIANS) {
		angle_radians -= TWO_PI_RADIANS;
	}
	return angle_radians;
}

static float clamp_pitch(float pitch_radians)
{
	if (pitch_radians < -1.571f) {
		return -1.571f;
	}
	if (pitch_radians > 1.571f) {
		return 1.571f;
	}
	return pitch_radians;
}

static bool player_collides_at(float candidate_x, float candidate_z, float eye_y)
{
	if (candidate_x < PLAYER_RADIUS || candidate_x > (float)WORLD_SIZE_X - PLAYER_RADIUS ||
		candidate_z < PLAYER_RADIUS || candidate_z > (float)WORLD_SIZE_Z - PLAYER_RADIUS) {
		return true;
	}

	float feet_y = eye_y - PLAYER_EYE_HEIGHT;
	float head_y = feet_y + PLAYER_BODY_HEIGHT;
	int minimum_block_x = (int)floorf(candidate_x - PLAYER_RADIUS);
	int maximum_block_x = (int)floorf(candidate_x + PLAYER_RADIUS);
	int minimum_block_z = (int)floorf(candidate_z - PLAYER_RADIUS);
	int maximum_block_z = (int)floorf(candidate_z + PLAYER_RADIUS);
	int minimum_block_y = (int)floorf(feet_y + 0.05f);
	int maximum_block_y = (int)floorf(head_y - 0.05f);

	for (int block_y = minimum_block_y; block_y <= maximum_block_y; ++block_y) {
		for (int block_z = minimum_block_z; block_z <= maximum_block_z; ++block_z) {
			for (int block_x = minimum_block_x; block_x <= maximum_block_x; ++block_x) {
				if (world_block_is_solid_at(block_x, block_y, block_z)) {
					return true;
				}
			}
		}
	}

	return false;
}

static void move_with_collision(player_t* player, float delta_x, float delta_z)
{
	float candidate_x = player->position_x + delta_x;
	if (!player_collides_at(candidate_x, player->position_z, player->eye_y)) {
		player->position_x = candidate_x;
	}

	float candidate_z = player->position_z + delta_z;
	if (!player_collides_at(player->position_x, candidate_z, player->eye_y)) {
		player->position_z = candidate_z;
	}

	player->position_x = clamp_float(player->position_x, PLAYER_RADIUS, (float)WORLD_SIZE_X - PLAYER_RADIUS);
	player->position_z = clamp_float(player->position_z, PLAYER_RADIUS, (float)WORLD_SIZE_Z - PLAYER_RADIUS);
}

void player_init(player_t* player)
{
	player->position_x = PLAYER_START_X;
	player->position_z = PLAYER_START_Z;
	player->yaw_radians = PLAYER_START_YAW;
	player->pitch_radians = PLAYER_START_PITCH;
	player->roll_radians = PLAYER_START_ROLL;
	player->eye_y = world_find_ground_eye_y(player->position_x, player->position_z, (float)(WORLD_SIZE_Y - 1));
	player->vertical_velocity = 0.0f;
	player->grounded = true;
	player->prev_grounded = true;
}

void player_update(player_t* player, const joystick_state_t* joystick_state, float delta_seconds)
{
	player->yaw_radians = wrap_angle(player->yaw_radians + joystick_state->turn_axis * PLAYER_TURN_SPEED * delta_seconds);
	player->pitch_radians = clamp_pitch(player->pitch_radians + joystick_state->turn_pitch * PLAYER_TURN_SPEED * delta_seconds);

	float forward_x = cosf(player->yaw_radians);
	float forward_z = sinf(player->yaw_radians);
	float travel_distance = joystick_state->move_axis * PLAYER_MOVE_SPEED * delta_seconds;
	move_with_collision(player, forward_x * travel_distance, forward_z * travel_distance);

	if (!get_target_display()) {

		float ground_eye_y = world_find_ground_eye_y(player->position_x, player->position_z, player->eye_y);
		if (player->eye_y > ground_eye_y + 0.03f) {
			player->grounded = false;
		}

		if (joystick_state->jump_pressed && player->grounded) {
			player->vertical_velocity = PLAYER_JUMP_SPEED;
			player->grounded = false;
			sound_play(3, jump_raw, sizeof(jump_raw), 250, false);
		}

		if (!player->grounded) {
			player->vertical_velocity -= PLAYER_GRAVITY * delta_seconds;
			float move_height = player->vertical_velocity * delta_seconds;
			if (player_collides_at(player->position_x, player->position_z, player->eye_y + move_height)) {
				player->vertical_velocity = 0.0f;
			}
			player->eye_y += player->vertical_velocity * delta_seconds;
		}
		else {
			player->eye_y = ground_eye_y;
		}

		if (player->eye_y <= ground_eye_y) {
			player->eye_y = ground_eye_y;
			player->vertical_velocity = 0.0f;
			player->grounded = true;
		}
		if (!player->prev_grounded && player->grounded) {
			sound_play(3, land_raw, sizeof(land_raw), 250, false);
		}
		player->prev_grounded = player->grounded;
	}
	else {
		if (is_button_pressed(joystick_state, PSCON_IDX_BUTTON_1, PSCON_MASK_BTN1_SQR)) {
			if (place_block(player, current_block)) {
				sound_play(3, placeblock_raw, sizeof(placeblock_raw), 220, false);
			};
		}

		if (is_button_pressed(joystick_state, PSCON_IDX_BUTTON_1, PSCON_MASK_BTN1_TRI)) {
			if (remove_block(player)) {
				sound_play(3, removeblock_raw, sizeof(removeblock_raw), 220, false);
			};
		}

		if (is_button_held(joystick_state, PSCON_IDX_BUTTON_2, PSCON_MASK_BTN2_L2)) {
			player->eye_y += 8.0f * delta_seconds;
		}
		if (is_button_held(joystick_state, PSCON_IDX_BUTTON_2, PSCON_MASK_BTN2_R2)) {
			player->eye_y -= 8.0f * delta_seconds;
		}

		if (is_button_pressed(joystick_state, PSCON_IDX_BUTTON_2, PSCON_MASK_BTN2_L1)) {
			sound_play(3, cursor_raw, sizeof(cursor_raw), 250, false);
			if (current_block > (BLOCK_NONE + 1)) {
				current_block--;
			}
			else {
				current_block = BLOCK_MAX - 1;
			}
		}

		if (is_button_pressed(joystick_state, PSCON_IDX_BUTTON_2, PSCON_MASK_BTN2_R1)) {
			sound_play(3, cursor_raw, sizeof(cursor_raw), 250, false);
			if (current_block < (BLOCK_MAX - 1)) {
				current_block++;
			}
			else {
				current_block = BLOCK_NONE + 1;
			}
		}
	}

	if (is_button_pressed(joystick_state, PSCON_IDX_BUTTON_1, PSCON_MASK_BTN1_CIR)) {
		sound_play(3, edit_raw, sizeof(edit_raw), 250, false);
		set_target_display(!get_target_display());
	}
}
