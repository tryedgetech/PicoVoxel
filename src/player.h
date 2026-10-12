// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] player.h - Player utility functions header file

#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>

#include "input.h"

typedef struct {
	float position_x;
	float eye_y;
	float position_z;
	float yaw_radians;
	float pitch_radians;
	float roll_radians;
	float vertical_velocity;
	bool grounded;
	bool prev_grounded;
} player_t;

extern uint8_t current_block;

void player_init(player_t* player);
void player_update(player_t* player, const joystick_state_t* joystick_state, float delta_seconds);

static inline bool is_player_moving(const player_t* player, const player_t* previous_player)
{
	if ((player->position_x    != previous_player->position_x)     ||
		(player->position_z    != previous_player->position_z)     ||
		(player->eye_y         != previous_player->eye_y)          ||
		(player->yaw_radians   != previous_player->yaw_radians)    ||
		(player->pitch_radians != previous_player->pitch_radians)  ||
		(player->roll_radians  != previous_player->roll_radians)   ||
		(player->grounded      != previous_player->grounded)       ||
		(player->vertical_velocity != previous_player->vertical_velocity)) {
		return true;
	}
	return false;
}

#endif // PLAYER_H
