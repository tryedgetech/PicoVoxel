// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] main.c - Main application file

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "app_config.h"
#include "input.h"
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/multicore.h"
#include "pico/rand.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "player.h"
#include "renderer.h"
#include "st7789.h"
#include "world.h"
#include "color.h"
#include "sound.h"
#include "audio_environment.h"
#include "camera_automode.h"


// for tinyusb
#include "bsp/board_api.h"
#include "tusb.h"
extern void hid_app_task(void);


// world model data
extern const uint8_t model_world_title_data[];
extern const uint8_t model_world_title_data_end[];
extern const uint8_t model_world_japanese_data[];
extern const uint8_t model_world_japanese_data_end[];


static environment_sound_t environment_japanese[] = {
//   pos_x,  pos_y,  pos_z,   fix,   att,  vol, ch, prev, interval, prob, pcm, len, flag
    { 7.12f,  6.00f, 23.14f, 10.0f, 20.0f, 180,  0,  0,          0,    0, waterfall_raw, sizeof(waterfall_raw), SOUND_ENV_FLAG_LOOP},
    {22.67f,  5.87f, 25.27f, 10.0f, 15.0f, 160,  1,  0,          0,    0, river_raw,     sizeof(river_raw), SOUND_ENV_FLAG_LOOP},
    { 6.53f, 21.00f,  9.04f,  5.0f, 12.0f, 200,  1,  0,   10000000,  230, higurashi_raw, sizeof(higurashi_raw), 0},
    {39.65f,  9.62f, 14.43f,  8.0f, 12.0f, 200,  1,  0,    1500000,  250, craw_raw,      sizeof(craw_raw), 0},
    {11.30f, 22.39f, 59.60f, 12.0f, 20.0f, 200,  1,  0,    1000000,  240, gull_raw,      sizeof(gull_raw), SOUND_ENV_FLAG_RANDOM},
    {56.11f,  5.59f, 57.10f, 12.0f, 20.0f, 200,  0,  0,    1000000,  250, gull_raw,      sizeof(gull_raw), SOUND_ENV_FLAG_RANDOM},
    {41.80f,  2.47f, 61.66f, 12.0f, 20.0f, 160,  1,  0,    2000000,  240, coast_raw,     sizeof(coast_raw), SOUND_ENV_FLAG_RANDOM},
    {20.23f, 22.44f, 34.30f, 10.0f, 20.0f, 180,  2,  0,          0,    0, cicada_raw,    sizeof(cicada_raw), SOUND_ENV_FLAG_LOOP},
    {58.25f, 19.44f, 36.49f, 10.0f, 20.0f, 160,  0,  0,          0,    0, cicada_raw,    sizeof(cicada_raw), SOUND_ENV_FLAG_LOOP},
};

static environment_sound_t environment_title[] = {
//   pos_x,  pos_y,  pos_z,   fix,   att,  vol, ch, prev, interval, prob, pcm, len, flag
    {31.50f,  5.50f, 17.50f,  8.0f, 10.0f, 200,  0,  0,          0,    0, title_raw,     sizeof(title_raw), SOUND_ENV_FLAG_LOOP},
};

static const camera_waypoint_t waypoints_japanese[] = {
//   pos_x, pos_y, pos_z, yaw,  pitch, roll, speed
    {32.00, 90.00, 32.00, 0.60, -1.70, -0.00, 12.0},
    {32.00, 70.00, 32.00, 0.60, -1.70, -3.13, 12.0},
    {32.00, 50.00, 32.00, 0.60, -1.70, -6.26, 12.0},
    {32.00, 40.00, 32.00, 2.60, -1.10, -6.26,  9.0},
    {42.15, 32.33, 63.73, 4.06, -0.58,  0.00,  3.0},
    {42.15, 28.60, 63.73, 5.22, -0.66,  0.00,  6.0},
    {42.15,  7.96, 63.73, 5.48, -0.42,  0.00,  6.0},
    {41.99,  5.96, 63.62, 4.68,  0.32,  0.00,  3.0},
    {41.99,  3.03, 63.62, 4.68,  0.31,  0.00,  6.0},
    {41.42,  3.37, 43.37, 4.68,  0.31,  0.15,  6.0},
    {37.32,  6.38, 32.38, 3.63,  0.15,  0.30,  6.0},
    {32.44,  5.15, 29.27, 3.33,  0.01,  0.00,  6.0},
    {25.20,  6.11, 26.74, 3.75,  0.01, -0.30,  6.0},
    {21.40,  8.46, 24.32, 3.57,  0.01,  0.00,  6.0},
    {15.65,  8.46, 20.82, 3.54, -0.26,  0.15,  3.0},
    {11.49,  8.46, 20.76, 2.62, -0.78,  0.00,  3.0},
    {11.49, 11.84, 20.76, 2.30, -0.91,  0.00,  6.0},
    {10.96, 26.59, 23.96, 2.27, -0.91,  0.00,  6.0},
    {10.96, 32.91, 23.96, 2.23, -1.05,  0.00,  6.0},
    {10.68, 32.91, 21.27, 3.05, -0.97,  0.00,  3.0},
    {10.73, 32.91, 18.18, 4.73, -0.91,  0.00,  6.0},
    {15.49, 14.48, 20.07, 0.73, -0.38,  0.00,  6.0},
    {26.41, 14.48, 24.04, 5.81, -0.38,  0.30,  3.0},
    {32.74, 14.48, 15.07, 4.41, -0.16,  0.30,  6.0},
    {33.83, 10.78, 11.43, 3.45,  0.02,  0.00,  3.0},
    {35.60, 10.78,  8.55, 3.19, -0.06,  0.00,  9.0},
    {25.79,  9.64,  8.66, 3.12,  0.07,  0.00,  9.0},
    {21.81, 10.63,  8.44, 3.14,  0.23,  0.00,  9.0},
    {17.70, 12.85,  8.44, 3.15,  0.23,  0.00,  3.0},
    {13.29, 14.00,  8.39, 3.15,  0.23,  0.00,  3.0},
    {11.00, 14.00,  8.52, 2.09, -0.14,  0.00,  6.0},
    { 9.93, 30.18, 10.38, 0.00, -0.25,  0.00,  6.0},
    {11.05, 34.54, 16.15, 0.00, -0.20, -0.30,  6.0},
    { 8.50, 34.54, 33.79, 1.67, -0.39,  0.00,  8.0},
    {12.18, 33.89, 48.29, 2.02, -0.44,  0.00,  3.0},
    {20.71, 33.89, 62.89, 3.26, -0.44,  0.00,  6.0},
    {32.88, 33.89, 63.74, 3.48, -0.73,  0.00,  6.0},
    {40.41, 33.89, 60.25, 4.68, -0.92,  0.00,  6.0},
    {59.89, 33.89, 47.52, 4.68, -1.05,  0.00,  6.0},
    {60.62, 33.89, 43.37, 4.64, -0.51,  0.00,  3.0},
    {59.69, 30.55, 43.37, 3.68, -0.40,  0.00,  3.0},
    {59.69, 26.45, 43.37, 3.36, -0.40,  0.00,  3.0},
    {61.74, 20.53, 43.37, 3.14, -0.08,  0.00,  6.0},
    {47.68, 20.53, 43.37, 3.14, -0.12,  0.00,  9.0},
    { 8.86, 20.53, 43.37, 3.14, -0.12,  0.00, 12.0},
    { 2.86, 20.53, 43.37, 3.14, -0.12,  0.00, 12.0},
};

static const camera_waypoint_t waypoints_opening[] = {
//   pos_x, pos_y, pos_z, yaw,  pitch, roll, speed
	{62.00, 50.50,  1.00, 3.14,  1.57,  0.00, 20.0},
	{62.00, 40.50,  1.00, 3.14,  1.57,  0.00, 20.0},
	{62.00, 10.50,  1.00, 3.14,  0.00,  0.00, 10.0},
	{62.00,  5.50,  5.00, 3.14,  0.00,  0.00, 10.0},
	{62.00,  5.50, 10.00, 3.14,  0.00,  0.00, 10.0},
	{62.00,  5.50, 20.00, 3.14,  0.00,  0.00, 10.0},
	{62.00,  5.50, 25.00, 3.14,  0.00,  0.00, 10.0},
	{46.00,  5.50, 41.00, 3.92,  0.00,  0.00, 10.0},
	{35.00,  5.50, 41.00, 3.92,  0.00,  0.00, 10.0},
	{ 6.00,  5.50, 41.00, 3.92,  0.00,  0.00, 10.0},
	{ 0.50,  5.50, 47.00, 4.71,  0.00,  0.00, 10.0},
	{ 0.50,  5.50, 56.00, 4.71,  0.00,  0.00, 10.0},
	{ 6.00,  5.58, 62.00, 4.71,  0.00,  0.00, 10.0},
	{16.00,  5.50, 62.00, 4.71,  0.00,  0.00, 10.0},
	{31.50,  5.50, 56.00, 4.71,  0.00,  0.00,  8.0},
	{31.50,  5.50, 45.00, 4.71,  0.00,  0.00, 15.0},
	{31.50, -8.00, 38.00, 4.71,  0.85,  0.00, 20.0},
	{31.50,-15.00, 30.00, 4.71,  0.85,  0.00, 30.0},
	{31.50,-20.00, 24.00, 4.71,  1.50,  0.00, 20.0},
	{31.50,  0.00, 17.50, 4.71,  1.57,  0.00,  8.0},
	{31.50,  5.50, 17.50, 4.71,  0.00,  0.00,  5.0},
	{31.50, 11.00, 17.50, 4.71, -1.57,  0.00, 10.0},
	{31.50, 20.00, 17.50, 4.71, -1.57,  0.00, 10.0},
	{21.00, 40.00, 17.50, 4.71, -1.57,  0.00, 15.0},
	{20.00, 44.00, 17.50, 4.71, -1.57,  0.00,  8.0},
	{18.00, 40.00, 17.50, 4.71, -1.57,  0.00,  5.0},
	{20.00, 36.00, 17.50, 4.71, -1.57,  0.00,  8.0},
	{22.00, 40.00, 17.50, 4.71, -1.57,  0.00, 30.0},
	{22.00, 70.00, 17.50, 4.71, -1.57,  0.00, 50.0},
	{22.00, 90.00, 17.50, 4.71, -1.57,  0.00, 50.0},
};

static const shader_params_t shader_params_opening = {
	.fog_start = FLOAT_TO_FIX(45.0f),
	.fog_end   = FLOAT_TO_FIX(58.0f),
	.fog_full  = FLOAT_TO_FIX(64.0f),
	.fog_color = RGB565(220, 220, 220),
	.face_scale = { 210, 255, 235, 190 }, // X, Z, TOP, BOTTOM
	.distance_scale = false,
#ifdef DO_BLOCK_SHADOW
    .shadow_end   = FLOAT_TO_FIX(0.0f),
    .shadow_color = RGB565(80, 80, 100),
	.light_dir_x = FLOAT_TO_FIX(-0.5f),
	.light_dir_z = FLOAT_TO_FIX(-0.866f),
	.shadow_length_scale = FLOAT_TO_FIX(1.4f),
#endif // DO_BLOCK_SHADOW
};

static const shader_params_t shader_params_main = {
	.fog_start = FLOAT_TO_FIX(14.0f),
	.fog_end   = FLOAT_TO_FIX(58.0f),
	.fog_full  = FLOAT_TO_FIX(64.0f),
	.fog_color = RGB565(255, 229, 204),
	.face_scale = { 210, 235, 255, 185 }, // X, Z, TOP, BOTTOM
	.distance_scale = true,
#ifdef DO_BLOCK_SHADOW
    .shadow_end   = FLOAT_TO_FIX(40.0f),
    .shadow_color = RGB565(40, 40, 60),
	.light_dir_x = FLOAT_TO_FIX(-0.5f),
	.light_dir_z = FLOAT_TO_FIX(-0.866f),
	.shadow_length_scale = FLOAT_TO_FIX(1.4f),
#endif // DO_BLOCK_SHADOW
};

static uint16_t frame_buffer[LCD_WIDTH * LCD_HEIGHT] __attribute__((aligned(4)));
static uint16_t draw_buffer[DRAW_WIDTH * DRAW_HEIGHT];


#ifdef DO_RACING_THE_BEAM
#define LCD_DMA_TRANSFER_US     17000
#define LCD_UPSCALE_US          5900
#define TEARING_BORDER_MARGIN_ROWS 2

static uint64_t dma_transfer_us = LCD_DMA_TRANSFER_US;
static uint64_t upscale_us = LCD_UPSCALE_US;
#endif // DO_RACING_THE_BEAM


#define SNAPSHOT_UART_CHUNK_BYTES 512
#define SNAPSHOT_FRAMEBUFFER_BYTES (LCD_WIDTH * LCD_HEIGHT * sizeof(frame_buffer[0]))

typedef enum {
    APP_RUNMODE_MAIN = 0,
    APP_RUNMODE_OPENING = 1,
} app_runmode_t;

typedef enum {
    RENDER_JOB_RENDER = 1,
    RENDER_JOB_INTERPOLATE = 2,
	RENDER_JOB_UPSCALE = 3,
} render_job_kind_t;

typedef struct {
    const player_t* player;
    uint16_t* framebuffer;
	const uint16_t* src_buffer;
    int y_start;
    int y_end;
    bool bspan;
	absolute_time_t proc_time;
} render_job_t;

static volatile render_job_t core1_job;


static void core1_render_worker(void)
{
#if defined(DO_TEXTURE_SAMPLING) && defined(DO_TEXSAMPLING_INTERP)
	// Core1 interpolator initialization
	renderer_init_texture_interp();
#endif // defined(DO_TEXTURE_SAMPLING) && defined(DO_TEXSAMPLING_INTERP)

    while (true) {
        uint32_t job_kind = multicore_fifo_pop_blocking();
		absolute_time_t start_time = get_absolute_time();
        if (job_kind == RENDER_JOB_RENDER) {
#ifdef DO_WATCHDOG
			watchdog_hw->scratch[1] = WATCHDOG_TRACE_RENDER | WATCHDOG_TRACE_START | (0x01);
#endif // DO_WATCHDOG
            renderer_render_partial(
                core1_job.player,
                core1_job.framebuffer,
                core1_job.y_start,
                core1_job.y_end,
                core1_job.bspan);
#ifdef DO_WATCHDOG
			watchdog_hw->scratch[1] = WATCHDOG_TRACE_RENDER | WATCHDOG_TRACE_END | (0x01);
#endif // DO_WATCHDOG
        }
        else if (job_kind == RENDER_JOB_INTERPOLATE) {
#ifdef DO_WATCHDOG
			watchdog_hw->scratch[1] = WATCHDOG_TRACE_INTERP | WATCHDOG_TRACE_START | (0x01);
#endif // DO_WATCHDOG
            renderer_interpolate_partial(
                core1_job.framebuffer,
                core1_job.y_start,
                core1_job.y_end,
                core1_job.bspan);
#ifdef DO_WATCHDOG
			watchdog_hw->scratch[1] = WATCHDOG_TRACE_INTERP | WATCHDOG_TRACE_END | (0x01);
#endif // DO_WATCHDOG
        }
        else if (job_kind == RENDER_JOB_UPSCALE) {
#ifdef DO_WATCHDOG
			watchdog_hw->scratch[1] = WATCHDOG_TRACE_UPSCALE | WATCHDOG_TRACE_START | (0x01);
#endif // DO_WATCHDOG
            upscale_2x_aa_partial(
                core1_job.src_buffer,
                core1_job.framebuffer,
                core1_job.y_start,
                core1_job.y_end);
#ifdef DO_WATCHDOG
			watchdog_hw->scratch[1] = WATCHDOG_TRACE_UPSCALE | WATCHDOG_TRACE_END | (0x01);
#endif // DO_WATCHDOG
        }
		core1_job.proc_time = absolute_time_diff_us(start_time, get_absolute_time());
        multicore_fifo_push_blocking(job_kind);
    }
}

static float clamp_delta_seconds(float delta_seconds)
{
	if (delta_seconds < 0.001f) {
		return 0.001f;
	}
	if (delta_seconds > 0.100f) {
		return 0.100f;
	}
	return delta_seconds;
}

static inline int clamp_split_line(int split_line)
{
	if (split_line < 4) {
		return 4;
	}
	if (split_line > DRAW_HEIGHT - 4) {
		return DRAW_HEIGHT - 4;
	}
	return split_line;
}

static inline int adjust_split_line(int split_line, absolute_time_t core0_time, absolute_time_t core1_time)
{
	if (core0_time <= 0 || core1_time <= 0) {
		return split_line;
	}

	float ratio = (float)core0_time / (float)core1_time;
	int adjustment = (int)((1.0f - ratio) * 0.10f * (float)DRAW_HEIGHT);
	if (adjustment > 8) {
		adjustment = 8;
	}
	else if (adjustment < -8) {
		adjustment = -8;
	}

	return clamp_split_line(split_line + adjustment);
}

#ifdef DO_RACING_THE_BEAM
static inline int estimate_safe_draw_rows(int64_t elapsed_us)
{
	if (elapsed_us <= 0) {
		return 0;
	}
	if (elapsed_us >= (int64_t)dma_transfer_us) {
		return 0;
	}

	int64_t denom_us = (int64_t)dma_transfer_us - (int64_t)upscale_us;
	if (denom_us <= 0) {
		return 0;
	}

	int64_t rows_safe = ((int64_t)elapsed_us * (int64_t)DRAW_HEIGHT) / denom_us;
	rows_safe -= TEARING_BORDER_MARGIN_ROWS;
	if (rows_safe < 0) {
		rows_safe = 0;
	}
	if (rows_safe > DRAW_HEIGHT) {
		rows_safe = DRAW_HEIGHT;
	}

	int64_t remain_us = (int64_t)dma_transfer_us - elapsed_us;
	int64_t rows_budget = (remain_us * (int64_t)DRAW_HEIGHT) / (int64_t)upscale_us;
	if (rows_budget < 0) {
		rows_budget = 0;
	}
	if (rows_budget > DRAW_HEIGHT) {
		rows_budget = DRAW_HEIGHT;
	}

	return (int)((rows_safe < rows_budget) ? rows_safe : rows_budget);
}
#endif // DO_RACING_THE_BEAM

// Send data over UART in chunks, flushing after each chunk
static void uart_snapshot_write_all(const void* data, size_t length)
{
	const uint8_t* bytes = (const uint8_t*)data;
	while (length > 0) {
		int chunk = (length > SNAPSHOT_UART_CHUNK_BYTES) ? SNAPSHOT_UART_CHUNK_BYTES : (int)length;
		stdio_put_string((const char*)bytes, chunk, false, false);
		stdio_flush();
		bytes += chunk;
		length -= (size_t)chunk;
#ifdef DO_WATCHDOG
		watchdog_update();
#endif // DO_WATCHDOG
	}
}

// Send framebuffer snapshot over UART in chunks
static void send_framebuffer_snapshot_uart(const uint16_t* framebuffer)
{
	char header[128];
	int header_len = snprintf(header, sizeof(header),
		"SNAP_BEGIN\nWIDTH %d\nHEIGHT %d\nFORMAT RGB565LE\nBYTES %u\nDATA\n",
		LCD_WIDTH, LCD_HEIGHT, (unsigned int)SNAPSHOT_FRAMEBUFFER_BYTES);

	if (header_len <= 0 || header_len >= (int)sizeof(header)) {
		return;
	}

	stdio_flush();
	uart_snapshot_write_all(header, (size_t)header_len);
	uart_snapshot_write_all(framebuffer, SNAPSHOT_FRAMEBUFFER_BYTES);
	uart_snapshot_write_all("\nSNAP_END\n", sizeof("\nSNAP_END\n") - 1);
	stdio_flush();
}

// reference to Raspberry Pi Pico SDK Examples(adc/onboard_temperature)
static inline float get_onboard_temp_c(void)
{
    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)read_adc_channel(4) * conversionFactor;
    return 27.0f - (adc - 0.706f) / 0.001721f;
}


int main(void)
{
	stdio_init_all();
	board_init();

	// Set voltage and clock frequency
	vreg_disable_voltage_limit();
	vreg_set_voltage(VREG_VOLTAGE_1_25);
	set_sys_clock_khz(300000, true);

    uint32_t freq = clock_get_hz(clk_sys);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, freq, freq);

	uart_init(uart0, 115200);	
	sleep_ms(100);

printf("start\n");
printf("clk_sys  = %ld Hz\n", clock_get_hz(clk_sys));
printf("clk_peri = %ld Hz\n", clock_get_hz(clk_peri));


	// init host stack on configured roothub port
	tuh_init(BOARD_TUH_RHPORT);
	if (board_init_after_tusb) {
		board_init_after_tusb();
	}

	// Initialize input system
	input_init();

	// Initialize LUT
	init_luts();

#ifdef DO_TEXTURE_SAMPLING
	// Initialize textures
	finalize_textures();
#ifdef DO_TEXSAMPLING_INTERP
	renderer_init_texture_interp();
#endif // DO_TEXSAMPLING_INTERP
#endif // DO_TEXTURE_SAMPLING

	// launch core1
	multicore_fifo_drain();
	multicore_launch_core1(core1_render_worker);

	// Initialize LCD
	lcd_init();
	lcd_draw_color_test_patterns();

	// Initialize sound
	sound_init();
	
	sleep_ms(2000);

	// initialize player
	player_t player;
	player_init(&player);


	int split_line = DRAW_HEIGHT / 2;
	absolute_time_t core0_time = 0;
	absolute_time_t core1_time = 0;
	absolute_time_t previous_time = get_absolute_time();
	absolute_time_t current_time = previous_time;
	absolute_time_t prerender_time = 0;
	absolute_time_t postrender_time = 0;

	// for display FPS
	bool display_fps = true;
	float fps_his[8] = { 0.0f };
	float min_fps = 1000000.0f;
	float max_fps = 0.0f;
	float avg_fps = 0.0f;
	char fps_str[64];
	draw_string_t fps_attr = {
		.posx = 5,
		.posy = 5,
		.fontdata = &skfont16,
		.color = RGB565(255, 127, 0),
		.opacity = 255,
	};
	draw_string_t fps_attr_sub = {
		.posx = 5,
		.posy = 22,
		.fontdata = &skfont12,
		.color = RGB565(255, 127, 0),
		.opacity = 255,
	};
	uint32_t fps_counter = 0;
	absolute_time_t startfps_time = 0;
	// for display CPU temp
    adc_set_temp_sensor_enabled(true);


#ifdef DO_PROFILING
	absolute_time_t preinterp_time = 0;
	absolute_time_t postinterp_time = 0;
#endif // DO_PROFILING

	bool bspan = false;
	player_t player_last = player;
	joystick_state_t joystick_state = {0};
	absolute_time_t send_start_time = 0;
	bool snapshot_requested = false;
	uint32_t frame_counter = 0;

	
	// Set initial run mode and shader parameters
	app_runmode_t app_run_mode = APP_RUNMODE_OPENING;
	renderer_set_shader_params(&shader_params_opening);

	// Initialize world model
	world_load_compressed_model(model_world_title_data, (int)(model_world_title_data_end - model_world_title_data));

	// Initialize environment sound
	environment_sound_info_t environment_info;
	sound_init_environment(&environment_info, environment_title,
		(int)(sizeof(environment_title) / sizeof(environment_title[0])));

	// initialize camera automode
	camera_automode_t camera_automode;
	camera_automode_init(&camera_automode, waypoints_opening,
		(int)(sizeof(waypoints_opening) / sizeof(waypoints_opening[0])));

	// Enable camera automode for the opening sequence
	camera_automode_set_enabled(&camera_automode, true, &player);


#ifdef DO_WATCHDOG
	if (watchdog_enable_caused_reboot()) {
        printf("*** WDT REBOOT DETECTED ***\n");
        printf("Previous run was frozen / hung.\n");
		printf("Watchdog scratch[0] core0: 0x%08X\n", (unsigned int)watchdog_hw->scratch[0]);
		printf("Watchdog scratch[1] core1: 0x%08X\n", (unsigned int)watchdog_hw->scratch[1]);
    }
	else if (watchdog_caused_reboot()) {
        printf("Reboot caused by watchdog (but not from watchdog_enable)\n");
    }
	else {
        printf("Clean boot (power-on or RUN pin reset)\n");
    }

	// enable watchdog timer
	watchdog_enable(WATCHDOG_TIMEOUT_MS, true);
#endif // DO_WATCHDOG


	printf("main loop\n");

	// main loop
	while (true) {
		// Calculate delta time
		current_time = get_absolute_time();
		float delta_seconds = (float)absolute_time_diff_us(previous_time, current_time) / 1000000.0f;
		previous_time = current_time;
		delta_seconds = clamp_delta_seconds(delta_seconds);

#if defined(DO_TEXTURE_SAMPLING) && defined(DO_TEXTURE_ANIMATION)
		// Update texture animation
		renderer_update_animation_texture(delta_seconds);
#endif // defined(DO_TEXTURE_SAMPLING) && defined(DO_TEXTURE_ANIMATION)

		// Read input
		input_read(&joystick_state);

		// snapshot
		if (is_button_pressed(&joystick_state, PSCON_IDX_BUTTON_2, PSCON_MASK_BTN2_L3)) {
			snapshot_requested = true;
		}

		if (app_run_mode == APP_RUNMODE_OPENING) {
			// Check if user skips the opening sequence
			if (is_button_pressed(&joystick_state, PSCON_IDX_BUTTON_1, PSCON_MASK_BTN1_X)) {
				app_run_mode = APP_RUNMODE_MAIN;
				player_init(&player);

				renderer_set_shader_params(&shader_params_main);
				world_load_compressed_model(model_world_japanese_data,
					(int)(model_world_japanese_data_end - model_world_japanese_data));

				sound_init_environment(&environment_info, environment_japanese,
					(int)(sizeof(environment_japanese) / sizeof(environment_japanese[0])));

				camera_automode_init(&camera_automode, waypoints_japanese,
					(int)(sizeof(waypoints_japanese) / sizeof(waypoints_japanese[0])));

				camera_automode_set_enabled(&camera_automode, false, &player);
			}
		}
		else {
			// Toggle camera automode
			if (is_button_pressed(&joystick_state, PSCON_IDX_BUTTON_2, PSCON_MASK_BTN2_OPT)) {
				bool enabled = !camera_automode_is_enabled(&camera_automode);
				camera_automode_set_enabled(&camera_automode, enabled, &player);
			}
		}

		// Update player state
		if (camera_automode_is_enabled(&camera_automode)) {
			camera_automode_update(&camera_automode, &player, delta_seconds);
		}
		else {
			player_update(&player, &joystick_state, delta_seconds);
		}

		// update taget
		update_target(&player);

		// multicore rendering
#ifdef DO_DDA_STATISTICS
		memset(&g_render_stats, 0, sizeof(g_render_stats));
#endif // DO_DDA_STATISTICS
		core1_job.player = &player;
		core1_job.framebuffer = draw_buffer;
		core1_job.y_start = split_line;
		core1_job.y_end = DRAW_HEIGHT;
		core1_job.bspan = bspan;
		multicore_fifo_push_blocking(RENDER_JOB_RENDER);
		{
			prerender_time = get_absolute_time();
#ifdef DO_WATCHDOG
			watchdog_hw->scratch[0] = WATCHDOG_TRACE_RENDER | WATCHDOG_TRACE_START | (0x01);
#endif // DO_WATCHDOG

			// core 0 renders the top part of the screen 
			renderer_render_partial(&player, draw_buffer, 0, split_line, bspan);

#ifdef DO_WATCHDOG
			watchdog_hw->scratch[0] = WATCHDOG_TRACE_RENDER | WATCHDOG_TRACE_END | (0x01);
#endif // DO_WATCHDOG
			postrender_time = get_absolute_time();
		}
		uint32_t render_done = multicore_fifo_pop_blocking();
		if (render_done != RENDER_JOB_RENDER) {
			printf("render worker error\n");
		}

		// adjust split line
		core0_time = absolute_time_diff_us(prerender_time, postrender_time);
		core1_time = core1_job.proc_time;
		split_line = adjust_split_line(split_line, core0_time, core1_time);

#ifdef DO_PROFILING
			absolute_time_t interpcore1_time = 0;
#endif // DO_PROFILING

		// multicore interpolates
		if (is_player_moving(&player, &player_last)) {
			core1_job.y_start = DRAW_HEIGHT / 2;
			core1_job.y_end = DRAW_HEIGHT;
			core1_job.bspan = bspan;
			multicore_fifo_push_blocking(RENDER_JOB_INTERPOLATE);
			{
#ifdef DO_PROFILING
				preinterp_time = get_absolute_time();
#endif // DO_PROFILING

#ifdef DO_WATCHDOG
			watchdog_hw->scratch[0] = WATCHDOG_TRACE_INTERP | WATCHDOG_TRACE_START | (0x01);
#endif // DO_WATCHDOG

				// core 0 interpolates the top part of the screen 
				renderer_interpolate_partial(draw_buffer, 0, DRAW_HEIGHT / 2, bspan);

#ifdef DO_WATCHDOG
			watchdog_hw->scratch[0] = WATCHDOG_TRACE_INTERP | WATCHDOG_TRACE_END | (0x01);
#endif // DO_WATCHDOG

#ifdef DO_PROFILING
				postinterp_time = get_absolute_time();
#endif // DO_PROFILING
			}
			uint32_t interp_done = multicore_fifo_pop_blocking();
			if (interp_done != RENDER_JOB_INTERPOLATE) {
				printf("interpolate worker error\n");
			}
#ifdef DO_PROFILING
			interpcore1_time = core1_job.proc_time;
#endif // DO_PROFILING
		}
#ifdef DO_PROFILING
		else {
			interpcore1_time = 0;
		}
#endif // DO_PROFILING


#ifdef DO_PROFILING
		absolute_time_t pretinyusb_time = get_absolute_time();
#endif // DO_PROFILING

		// tinyusb host task
		tuh_task();
		hid_app_task();

#ifdef DO_PROFILING
		absolute_time_t tinyusb_time = get_absolute_time();
#endif // DO_PROFILING

		// sound effect
		sound_play_environment(&environment_info,
			current_time, player.position_x, player.eye_y, player.position_z);

		int safe_rows = 0;
		absolute_time_t pre_safedraw_time = get_absolute_time();
#ifdef DO_RACING_THE_BEAM
		if ((frame_counter & 0x1F) == 0x1F) {
			// Wait for the DMA transfer to complete  every 32 frames
			// to update the SPI tansfer and upscale times.
		}
		else {
			// Write to the framebuffer in parallel with the DMA transfer,
			// for the area that is estimated to have been read by the DMA.
			safe_rows = estimate_safe_draw_rows(absolute_time_diff_us(send_start_time, get_absolute_time()));
			if (safe_rows > 0) {
				upscale_2x_aa_partial(draw_buffer, frame_buffer, 0, safe_rows);
			}
		}
#endif // DO_RACING_THE_BEAM
		absolute_time_t post_safedraw_time = get_absolute_time();

		// Wait for previous DMA transfer
		lcd_wait_dma();
#ifdef DO_RACING_THE_BEAM
		if ((frame_counter & 0x1F) == 0x1F) {
			// update the DMA transfer time for SPI
			dma_transfer_us = absolute_time_diff_us(send_start_time, get_absolute_time() -
				absolute_time_diff_us(pre_safedraw_time, post_safedraw_time));
		}
#endif // DO_RACING_THE_BEAM

#ifdef DO_PROFILING
		absolute_time_t dma_time = get_absolute_time();
		absolute_time_t upscalecore1_time = 0;
#endif // DO_PROFILING

		// The remaining upscale is executed in parallel.
		if (safe_rows < DRAW_HEIGHT) {
			int mid = safe_rows + (DRAW_HEIGHT - safe_rows) / 2;
			core1_job.src_buffer = draw_buffer;
			core1_job.framebuffer = frame_buffer;
			core1_job.y_start = mid;
			core1_job.y_end = DRAW_HEIGHT;
			multicore_fifo_push_blocking(RENDER_JOB_UPSCALE);
			{
#ifdef DO_WATCHDOG
			watchdog_hw->scratch[0] = WATCHDOG_TRACE_UPSCALE | WATCHDOG_TRACE_START | (0x01);
#endif // DO_WATCHDOG
				upscale_2x_aa_partial(draw_buffer, frame_buffer, safe_rows, mid);
#ifdef DO_WATCHDOG
			watchdog_hw->scratch[0] = WATCHDOG_TRACE_UPSCALE | WATCHDOG_TRACE_END | (0x01);
#endif // DO_WATCHDOG
			}
			uint32_t upscale_done = multicore_fifo_pop_blocking();
			if (upscale_done != RENDER_JOB_UPSCALE) {
				printf("upscale worker error\n");
			}
#ifdef DO_RACING_THE_BEAM
			if ((frame_counter & 0x1F) == 0x1F) {
				// Update the upscale time
				// Multiply by 2 to account for both halves of the upscale operation.
				upscale_us = core1_job.proc_time * 2;
			}
#endif // DO_RACING_THE_BEAM
#ifdef DO_PROFILING
			upscalecore1_time = core1_job.proc_time;
#endif // DO_PROFILING
		}
#ifdef DO_PROFILING
		else {
			upscalecore1_time = 0;
		}
#endif // DO_PROFILING


#ifdef DO_PROFILING
		absolute_time_t upscale_time = get_absolute_time();
#endif // DO_PROFILING

		if (get_target_display()) {
			// Draw block selection UI
			draw_block_select(frame_buffer);
		}

#ifdef DO_PROFILING
		absolute_time_t drawsel_time = get_absolute_time();
#endif // DO_PROFILING

		// toggle display fps
		if (is_button_pressed(&joystick_state, PSCON_IDX_BUTTON_2, PSCON_MASK_BTN2_R3)) {
			display_fps = !display_fps;
			if (display_fps) {
				startfps_time = current_time;
				fps_counter = 0;
				min_fps = 1000000.0f;
				max_fps = 0.0f;
			}
		}
		if (display_fps && (delta_seconds > 0.0f)) {
			// display FPS
			fps_his[fps_counter & 7] = 1.0f / delta_seconds;
			avg_fps = fps_counter > 0 ?
				(float)fps_counter / (absolute_time_diff_us(startfps_time, current_time) / 1000000.0f) : 0.0f;
			if ((fps_counter & 7) == 7) {
				float fps_sum = 0.0f;
				for (int i = 0; i <= 7; i++) {
					fps_sum += fps_his[i];
				}
				float fps_wk = fps_sum / 8.0f;
				if (fps_wk < min_fps) min_fps = fps_wk;
				if (fps_wk > max_fps) max_fps = fps_wk;
			}

			snprintf((char*)fps_str, sizeof(fps_str), "FPS:%.1f", fps_his[fps_counter & 7]);
			renderer_draw_string(fps_attr, (const char*)fps_str, frame_buffer, LCD_WIDTH, LCD_HEIGHT);
			snprintf((char*)fps_str, sizeof(fps_str), "avg %.1f", avg_fps);
			fps_attr_sub.posy = 22;
			renderer_draw_string(fps_attr_sub, (const char*)fps_str, frame_buffer, LCD_WIDTH, LCD_HEIGHT);
			snprintf((char*)fps_str, sizeof(fps_str), "min %.1f", min_fps);
			fps_attr_sub.posy = 34;
			renderer_draw_string(fps_attr_sub, (const char*)fps_str, frame_buffer, LCD_WIDTH, LCD_HEIGHT);
			snprintf((char*)fps_str, sizeof(fps_str), "max %.1f", max_fps);
			fps_attr_sub.posy = 46;
			renderer_draw_string(fps_attr_sub, (const char*)fps_str, frame_buffer, LCD_WIDTH, LCD_HEIGHT);
			snprintf((char*)fps_str, sizeof(fps_str), "tmp %.1f c", get_onboard_temp_c());
			fps_attr_sub.posy = 64;
			renderer_draw_string(fps_attr_sub, (const char*)fps_str, frame_buffer, LCD_WIDTH, LCD_HEIGHT);
			fps_counter++;
		}

		// snapshot framebuffer via UART
		if (snapshot_requested) {
			snapshot_requested = false;
			send_framebuffer_snapshot_uart(frame_buffer);
		}


#ifdef DO_PROFILING
		absolute_time_t dispfps_time = get_absolute_time();
#endif // DO_PROFILING

		// Send framebuffer to LCD via DMA
		send_start_time = get_absolute_time();
		lcd_send_framebuffer(frame_buffer);

#ifdef DO_PROFILING
		absolute_time_t send_time = get_absolute_time();
#endif // DO_PROFILING


#ifdef DO_PROFILING
		if ((frame_counter & 0x1F) == 0) {
			printf("render 0(%lld) 1(%lld) split line:%d\n", core0_time, core1_time, split_line);
			printf("interp 0(%lld) 1(%lld) upscale P-row:%d(%lld) R0(%lld) R1(%lld)\n",
				absolute_time_diff_us(preinterp_time, postinterp_time), interpcore1_time,
				safe_rows, absolute_time_diff_us(pre_safedraw_time, post_safedraw_time),
				absolute_time_diff_us(dma_time, upscale_time), upscalecore1_time);
			printf("etc = %lld, dma = %lld, send = %lld, tinyusb = %lld SPI = %lld\n",
				absolute_time_diff_us(current_time, prerender_time), absolute_time_diff_us(post_safedraw_time, dma_time),
				absolute_time_diff_us(dispfps_time, send_time),
				absolute_time_diff_us(pretinyusb_time, tinyusb_time), dma_transfer_us);
			printf("sound = %lld, dispfps = %lld, delta = %f, %.1f FPS\n",
				absolute_time_diff_us(tinyusb_time, pre_safedraw_time), absolute_time_diff_us(drawsel_time, dispfps_time),
				delta_seconds, 1.0f / delta_seconds);
			printf("{%.2f, %.2f, %.2f, %.2f, %.2f, %.2f,  },\n", player.position_x, player.eye_y, player.position_z,
				player.yaw_radians, player.pitch_radians, player.roll_radians);
#ifdef DO_DDA_STATISTICS
			printf("STAT: rays(%lld|%lld), steps(%lld|%lld), MChits(%lld|%lld), skipcells(%lld|%lld), hit(%lld|%lld), miss(%lld|%lld), fog(%lld|%lld)\n",
				g_render_stats[0].rays,             g_render_stats[1].rays,
				g_render_stats[0].dda_steps,        g_render_stats[1].dda_steps,
				g_render_stats[0].macro_hits,       g_render_stats[1].macro_hits,
				g_render_stats[0].macro_skip_cells, g_render_stats[1].macro_skip_cells,
				g_render_stats[0].hit_rays,         g_render_stats[1].hit_rays,
				g_render_stats[0].miss_rays,        g_render_stats[1].miss_rays,
				g_render_stats[0].fogfull_rays,     g_render_stats[1].fogfull_rays);
			printf("STAT: avg-steps = %.2f, max(%u|%u), avg-dist = %.2f, avg-skip = %.2f, macro-rate = %.2f%%, hit-rate = %.2f%%, miss-rate = %.2f%%, fog-rate = %.2f%%\n",
				(float)(g_render_stats[0].dda_steps + g_render_stats[1].dda_steps)               / (float)(g_render_stats[0].rays + g_render_stats[1].rays),
				(unsigned int)g_render_stats[0].max_dda_steps, (unsigned int)g_render_stats[1].max_dda_steps,
				(float)((g_render_stats[0].total_dist + g_render_stats[1].total_dist) >> FIX_SHIFT) / (float)(g_render_stats[0].rays + g_render_stats[1].rays),
				(float)(g_render_stats[0].macro_skip_cells + g_render_stats[1].macro_skip_cells) / (float)(g_render_stats[0].rays + g_render_stats[1].rays),
				(float)(g_render_stats[0].macro_hits + g_render_stats[1].macro_hits)*100         / (float)(g_render_stats[0].rays + g_render_stats[1].rays),
				(float)(g_render_stats[0].hit_rays + g_render_stats[1].hit_rays)*100             / (float)(g_render_stats[0].rays + g_render_stats[1].rays),
				(float)(g_render_stats[0].miss_rays + g_render_stats[1].miss_rays)*100           / (float)(g_render_stats[0].rays + g_render_stats[1].rays),
				(float)(g_render_stats[0].fogfull_rays + g_render_stats[1].fogfull_rays)*100     / (float)(g_render_stats[0].rays + g_render_stats[1].rays));				
#endif // DO_DDA_STATISTICS
		}
#endif // DO_PROFILING

#ifdef DO_WATCHDOG
		watchdog_update();
#endif // DO_WATCHDOG

		frame_counter++;

		tight_loop_contents();
		player_last = player;
		bspan = !bspan;
	}
}


// TinyUSB Callbacks
void tuh_mount_cb(uint8_t dev_addr) {
	// application set-up
	printf("A device with address %d is mounted\r\n", dev_addr);
}

void tuh_umount_cb(uint8_t dev_addr) {
	// application tear-down
	printf("A device with address %d is unmounted \r\n", dev_addr);
}