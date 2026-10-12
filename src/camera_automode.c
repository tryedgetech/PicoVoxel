// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] camera_automode.c - Camera automatic mode implementation

#include "camera_automode.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include "app_config.h"

#define CAMERA_AUTOMODE_PI 3.14159265f
#define CAMERA_AUTOMODE_TWO_PI 6.28318531f
#define CAMERA_AUTOMODE_MIN_SEGMENT_LENGTH 0.0001f


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
        angle_radians += CAMERA_AUTOMODE_TWO_PI;
    }
    while (angle_radians >= CAMERA_AUTOMODE_TWO_PI) {
        angle_radians -= CAMERA_AUTOMODE_TWO_PI;
    }
    return angle_radians;
}

static float clamp_pitch(float pitch_radians)
{
    if (pitch_radians < -(CAMERA_AUTOMODE_TWO_PI / 2.0f)) {
        return -(CAMERA_AUTOMODE_TWO_PI / 2.0f);
    }
    if (pitch_radians > (CAMERA_AUTOMODE_TWO_PI / 2.0f)) {
        return (CAMERA_AUTOMODE_TWO_PI / 2.0f);
    }
    return pitch_radians;
}

static int waypoint_index_for_mode(const camera_automode_t* automode, int index)
{
    if (automode->behavior == CAMERA_AUTOMODE_BEHAVIOR_LOOP) {
        while (index < 0) {
            index += automode->waypoint_count;
        }
        while (index >= automode->waypoint_count) {
            index -= automode->waypoint_count;
        }
        return index;
    }

    if (index < 0) {
        return 0;
    }
    if (index >= automode->waypoint_count) {
        return automode->waypoint_count - 1;
    }
    return index;
}

static float segment_length(const camera_waypoint_t* a, const camera_waypoint_t* b)
{
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dz = b->z - a->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static float catmull_rom(float p0, float p1, float p2, float p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

static float lerp_angle_shortest(float from_radians, float to_radians, float t)
{
    float delta = to_radians - from_radians;
    while (delta > CAMERA_AUTOMODE_PI) {
        delta -= CAMERA_AUTOMODE_TWO_PI;
    }
    while (delta < -CAMERA_AUTOMODE_PI) {
        delta += CAMERA_AUTOMODE_TWO_PI;
    }
    return wrap_angle(from_radians + delta * t);
}

static float lerp_float(float from_value, float to_value, float t)
{
    return from_value + (to_value - from_value) * t;
}

static void apply_waypoint_pose(const camera_waypoint_t* waypoint, player_t* player)
{
    player->position_x = clamp_float(waypoint->x, PLAYER_RADIUS, (float)WORLD_SIZE_X - PLAYER_RADIUS);
    player->eye_y = waypoint->y;
    player->position_z = clamp_float(waypoint->z, PLAYER_RADIUS, (float)WORLD_SIZE_Z - PLAYER_RADIUS);
    player->yaw_radians = wrap_angle(waypoint->yaw_radians);
    player->pitch_radians = clamp_pitch(waypoint->pitch_radians);
    player->roll_radians = wrap_angle(waypoint->roll_radians);
    player->vertical_velocity = 0.0f;
    player->grounded = true;
}

static void apply_segment_pose(const camera_automode_t* automode, int segment_index, float segment_progress_m, player_t* player)
{
    const camera_waypoint_t* w0;
    const camera_waypoint_t* w1;
    const camera_waypoint_t* w2;
    const camera_waypoint_t* w3;
    float t;
    float length;

    int i0 = waypoint_index_for_mode(automode, segment_index - 1);
    int i1 = waypoint_index_for_mode(automode, segment_index);
    int i2 = waypoint_index_for_mode(automode, segment_index + 1);
    int i3 = waypoint_index_for_mode(automode, segment_index + 2);

    w0 = &automode->waypoints[i0];
    w1 = &automode->waypoints[i1];
    w2 = &automode->waypoints[i2];
    w3 = &automode->waypoints[i3];

    length = segment_length(w1, w2);
    if (length <= CAMERA_AUTOMODE_MIN_SEGMENT_LENGTH) {
        t = 0.0f;
    }
    else {
        t = clamp_float(segment_progress_m / length, 0.0f, 1.0f);
    }

    player->position_x = clamp_float(catmull_rom(w0->x, w1->x, w2->x, w3->x, t), PLAYER_RADIUS, (float)WORLD_SIZE_X - PLAYER_RADIUS);
    player->eye_y = catmull_rom(w0->y, w1->y, w2->y, w3->y, t);
    player->position_z = clamp_float(catmull_rom(w0->z, w1->z, w2->z, w3->z, t), PLAYER_RADIUS, (float)WORLD_SIZE_Z - PLAYER_RADIUS);
    player->yaw_radians = lerp_angle_shortest(w1->yaw_radians, w2->yaw_radians, t);
    player->pitch_radians = clamp_pitch(lerp_float(w1->pitch_radians, w2->pitch_radians, t));
    player->roll_radians = lerp_angle_shortest(w1->roll_radians, w2->roll_radians, t);
    player->vertical_velocity = 0.0f;
    player->grounded = true;
}

void camera_automode_init(camera_automode_t* automode, const camera_waypoint_t* waypoints, int count)
{
    automode->waypoints = waypoints;
    automode->waypoint_count = count;
    automode->segment_index = 0;
    automode->segment_progress_m = 0.0f;
    automode->enabled = false;
    automode->behavior = CAMERA_AUTOMODE_BEHAVIOR_LOOP;
}

void camera_automode_set_enabled(camera_automode_t* automode, bool enabled, player_t* player)
{
    automode->enabled = enabled;
    automode->segment_index = 0;
    automode->segment_progress_m = 0.0f;

    if (enabled && player != NULL && automode->waypoint_count > 0) {
       apply_waypoint_pose(&automode->waypoints[0], player);
    }
}


bool camera_automode_is_enabled(const camera_automode_t* automode)
{
    return automode->enabled;
}

void camera_automode_update(camera_automode_t* automode, player_t* player, float delta_seconds)
{
    float distance_to_travel;
    if (!automode->enabled) {
        return;
    }

    if (automode->waypoint_count < 2) {
        automode->enabled = false;
        return;
    }

    distance_to_travel = 0.0f;
    if (delta_seconds > 0.0f) {
        int speed_index = waypoint_index_for_mode(automode, automode->segment_index);
        distance_to_travel = automode->waypoints[speed_index].speed * delta_seconds;
        if (distance_to_travel < 0.0f ) {
            distance_to_travel = 0.0f;
        }
    }

    while (distance_to_travel > 0.0f && automode->enabled) {
        int i1 = waypoint_index_for_mode(automode, automode->segment_index);
        int i2 = waypoint_index_for_mode(automode, automode->segment_index + 1);
        float length = segment_length(&automode->waypoints[i1], &automode->waypoints[i2]);

        if (length <= CAMERA_AUTOMODE_MIN_SEGMENT_LENGTH) {
            automode->segment_progress_m = 0.0f;
            if (automode->segment_index + 1 >= automode->waypoint_count - 1) {
                if (automode->behavior == CAMERA_AUTOMODE_BEHAVIOR_LOOP) {
                    automode->segment_index = 0;
                }
                else {
                    automode->segment_index = automode->waypoint_count - 1;
                    automode->enabled = false;
                    apply_waypoint_pose(&automode->waypoints[automode->waypoint_count - 1], player);
                    return;
                }
            }
            else {
                automode->segment_index++;
            }
            continue;
        }

        {
            float remaining = length - automode->segment_progress_m;
            if (distance_to_travel < remaining) {
                automode->segment_progress_m += distance_to_travel;
                distance_to_travel = 0.0f;
            }
            else {
                distance_to_travel -= remaining;
                automode->segment_progress_m = 0.0f;
                if (automode->segment_index + 1 >= automode->waypoint_count - 1) {
                    if (automode->behavior == CAMERA_AUTOMODE_BEHAVIOR_LOOP) {
                        automode->segment_index = 0;
                    }
                    else {
                        automode->segment_index = automode->waypoint_count - 1;
                        automode->enabled = false;
                        apply_waypoint_pose(&automode->waypoints[automode->waypoint_count - 1], player);
                        return;
                    }
                }
                else {
                    automode->segment_index++;
                }
            }
        }
    }
    apply_segment_pose(automode, automode->segment_index, automode->segment_progress_m, player);
}