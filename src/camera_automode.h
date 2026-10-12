// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] camera_automode.h - Camera automatic mode header file

#ifndef CAMERA_AUTOMODE_H
#define CAMERA_AUTOMODE_H

#include <stdbool.h>
#include "player.h"

typedef struct {
    float x;
    float y;
    float z;
    float yaw_radians;
    float pitch_radians;
    float roll_radians;
    float speed;
} camera_waypoint_t;

typedef enum {
    CAMERA_AUTOMODE_BEHAVIOR_LOOP = 0,
    CAMERA_AUTOMODE_BEHAVIOR_STOP_AT_LAST = 1,
} camera_automode_behavior_t;

typedef struct {
    const camera_waypoint_t* waypoints;
    int waypoint_count;
    int segment_index;
    float segment_progress_m;
    bool enabled;
    camera_automode_behavior_t behavior;
} camera_automode_t;

void camera_automode_init(camera_automode_t* automode, const camera_waypoint_t* waypoints, int count);
void camera_automode_set_enabled(camera_automode_t* automode, bool enabled, player_t* player);
bool camera_automode_is_enabled(const camera_automode_t* automode);
void camera_automode_update(camera_automode_t* automode, player_t* player, float delta_seconds);

#endif // CAMERA_AUTOMODE_H