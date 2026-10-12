// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] st7789.c - ST7789 display driver implementation

#include "st7789.h"
#include "app_config.h"
#include "color.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include <string.h>
#include <stdio.h>

// DMA channel
static int dma_chan = -1;
static dma_channel_config dma_cfg;


#define ST_CMD_NOP        0x00
#define ST_CMD_SWRESET    0x01
#define ST_CMD_SLPOUT     0x11
#define ST_CMD_NORON      0x13
#define ST_CMD_INVOFF     0x20
#define ST_CMD_INVON      0x21
#define ST_CMD_DISPON     0x29
#define ST_CMD_CASET      0x2A
#define ST_CMD_RASET      0x2B
#define ST_CMD_RAMWR      0x2C
#define ST_CMD_COLMOD     0x3A
#define ST_CMD_MADCTL     0x36
#define ST_CMD_FRMCTR1    0xB1
#define ST_CMD_FRMCTR2    0xB2
#define ST_CMD_FRMCTR3    0xB3
#define ST_CMD_INVCTR     0xB4
#define ST_CMD_PWCTR1     0xC0
#define ST_CMD_PWCTR2     0xC1
#define ST_CMD_PWCTR3     0xC2
#define ST_CMD_PWCTR4     0xC3
#define ST_CMD_PWCTR5     0xC4
#define ST_CMD_VMCTR1     0xC5
#define ST_CMD_GMCTRP1    0xE0
#define ST_CMD_GMCTRN1    0xE1

// Column/row offset
#define COL_OFFSET 0
#define ROW_OFFSET 0


static void lcd_pin_init(void) {
	// SPI pins
	gpio_set_function(LCD_PIN_SCK,  GPIO_FUNC_SPI);
	gpio_set_function(LCD_PIN_MOSI, GPIO_FUNC_SPI);

	// CS, DC, RST, BL as GPIO outputs
	gpio_init(LCD_PIN_RST);
	gpio_set_dir(LCD_PIN_RST, GPIO_OUT);

	gpio_init(LCD_PIN_DC);
	gpio_set_dir(LCD_PIN_DC, GPIO_OUT);

	gpio_init(LCD_PIN_CS);
	gpio_set_dir(LCD_PIN_CS, GPIO_OUT);

	gpio_init(LCD_PIN_BL);
	gpio_set_dir(LCD_PIN_BL, GPIO_OUT);

	gpio_put(LCD_PIN_CS, 1);
	gpio_put(LCD_PIN_DC, 0);
	gpio_put(LCD_PIN_BL, 1); // backlight on
}

static inline void cs_select(void) {
	gpio_put(LCD_PIN_CS, 0);
}

static inline void cs_deselect(void) {
	gpio_put(LCD_PIN_CS, 1);
}

static void lcd_write_cmd(uint8_t cmd) {
	gpio_put(LCD_PIN_DC, 0); // command mode
	cs_select();
	spi_write_blocking(LCD_SPI, &cmd, 1);
	cs_deselect();
}

static void lcd_write_data(const uint8_t* data, size_t len) {
	gpio_put(LCD_PIN_DC, 1); // data mode
	cs_select();
	spi_write_blocking(LCD_SPI, data, len);
	cs_deselect();
}

static void lcd_write_data_byte(uint8_t val) {
	lcd_write_data(&val, 1);
}

static void lcd_write_rgb565_pixels(const uint16_t* pixels, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		uint16_t pixel = pixels[i];
		uint8_t hi = (uint8_t)(pixel >> 8);
		uint8_t lo = (uint8_t)(pixel & 0xFFu);
		spi_write_blocking(LCD_SPI, &hi, 1);
		spi_write_blocking(LCD_SPI, &lo, 1);
	}
}

static void lcd_reset(void) {
	gpio_put(LCD_PIN_RST, 1);
	sleep_ms(100);
	gpio_put(LCD_PIN_RST, 0);
	sleep_ms(100);
	gpio_put(LCD_PIN_RST, 1);
	sleep_ms(100);
}

static void lcd_init_sequence(void) {
	// Memory access control
	lcd_write_cmd(ST_CMD_MADCTL);
	lcd_write_data_byte (0xC0);

	// Color mode: 16-bit (RGB565)
	lcd_write_cmd(ST_CMD_COLMOD);
	lcd_write_data_byte(0x55);

	// Frame rate control normal mode
	lcd_write_cmd(ST_CMD_FRMCTR1);
	{
		uint8_t d[] = {0x01, 0x2C, 0x2D};
		lcd_write_data(d, sizeof(d));
	}

	// Frame rate control idle mode
	lcd_write_cmd(ST_CMD_FRMCTR2);
	{
		uint8_t d[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
		lcd_write_data(d, sizeof(d));
	}

	// Frame rate control partial mode
	lcd_write_cmd(ST_CMD_FRMCTR3);
	{
		uint8_t d[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
		lcd_write_data(d, sizeof(d));
	}

	// Power control 1
	lcd_write_cmd(ST_CMD_PWCTR1);
	{
		uint8_t d[] = {0xA2, 0x02, 0x84};
		lcd_write_data(d, sizeof(d));
	}

	// Power control 2
	lcd_write_cmd(ST_CMD_PWCTR2);
	lcd_write_data_byte(0xC5);

	/* Power control 3 (normal mode) */
	lcd_write_cmd(ST_CMD_PWCTR3);
	{
		uint8_t d[] = {0x0A, 0x00};
		lcd_write_data(d, sizeof(d));
	}

	/* Power control 4 (idle mode) */
	lcd_write_cmd(ST_CMD_PWCTR4);
	{
		uint8_t d[] = {0x8A, 0x2A};
		lcd_write_data(d, sizeof(d));
	}

	// Power control 5 (partial mode)
	lcd_write_cmd(ST_CMD_PWCTR5);
	{
		uint8_t d[] = {0x8A, 0xEE};
		lcd_write_data(d, sizeof(d));
	}

	// VCOM control
	lcd_write_cmd(ST_CMD_VMCTR1);
	lcd_write_data_byte(0x0E);

	// Gamma positive
	lcd_write_cmd(ST_CMD_GMCTRP1);
	{
		uint8_t d[] = {
			0xD0, 0x00, 0x02, 0x07, 0x0B, 0x1A, 0x31, 0x54,
        	0x40, 0x09, 0x12, 0x12, 0x12, 0x17
		};
		lcd_write_data(d, sizeof(d));
	}

	// Gamma negative
	lcd_write_cmd(ST_CMD_GMCTRN1);
	{
		uint8_t d[] = {
			0xD0, 0x00, 0x02, 0x07, 0x05, 0x25, 0x2D, 0x44,
        	0x45, 0x15, 0x17, 0x18, 0x1A, 0x1D
		};
		lcd_write_data(d, sizeof(d));
	}

	lcd_write_cmd(0xF0);
    lcd_write_data_byte(0x01);

    lcd_write_cmd(0xF6);
    lcd_write_data_byte(0x00);

	// Sleep out
	lcd_write_cmd(ST_CMD_SLPOUT);
	sleep_ms(120);

	lcd_write_cmd(ST_CMD_INVON);

	// Display on
	lcd_write_cmd(ST_CMD_DISPON);
	sleep_ms(120);
}


static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
	lcd_write_cmd(ST_CMD_CASET);
	{
		uint8_t d[] = {
			x0 >> 8, (uint8_t)(x0 + COL_OFFSET),
			x1 >> 8, (uint8_t)(x1 + COL_OFFSET)
		};
		lcd_write_data(d, 4);
	}

	lcd_write_cmd(ST_CMD_RASET);
	{
		uint8_t d[] = {
			y0 >> 8, (uint8_t)(y0 + ROW_OFFSET),
			y1 >> 8, (uint8_t)(y1 + ROW_OFFSET)
		};
		lcd_write_data(d, 4);
	}

	lcd_write_cmd(ST_CMD_RAMWR);
}


static void dma_init_channel(void) {
	dma_chan = dma_claim_unused_channel(true);
	dma_cfg = dma_channel_get_default_config(dma_chan);
	channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);
	channel_config_set_dreq(&dma_cfg, spi_get_dreq(LCD_SPI, true));
	channel_config_set_read_increment(&dma_cfg, true);
	channel_config_set_write_increment(&dma_cfg, false);
}


void lcd_init(void) {
	// Initialize SPI
	uint actual = spi_init(LCD_SPI, LCD_SPI_BAUDRATE);

printf("SPI set = %d, actual = %d\n", LCD_SPI_BAUDRATE, actual);

	spi_set_format(LCD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

	// Initialize GPIO pins
	lcd_pin_init();

	// Reset and initialize display
	lcd_reset();
	lcd_init_sequence();

	// Initialize DMA channel
	dma_init_channel();
}

void lcd_draw_color_test_patterns(void)
{
	uint16_t row_buf[LCD_WIDTH];
	const int gray_height = LCD_HEIGHT / 2;
	const int stripe_width = LCD_WIDTH / 16;
	const int block_width = LCD_WIDTH / 4;
	const int block_height = (LCD_HEIGHT - gray_height) / 4;

	lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
	gpio_put(LCD_PIN_DC, 1);
	cs_select();

	for (int y = 0; y < LCD_HEIGHT; y++) {
		for (int x = 0; x < LCD_WIDTH; x++) {
			uint16_t color = 0;
			if (y < gray_height) {
				int index = x / stripe_width;
				if (index > 15) {
					index = 15;
				}
				uint8_t level = (uint8_t)(((uint16_t)index * 255u) / 15u);
				color = color_rgb565(level, level, level);
			} else {
				int block_x = x / block_width;
				int block_y = (y - gray_height) / block_height;
				int index = block_y * 4 + block_x;
				switch (index) {
				case 0: color = color_rgb565(0, 0, 0); break;
				case 1: color = color_rgb565(255, 0, 0); break;
				case 2: color = color_rgb565(0, 255, 0); break;
				case 3: color = color_rgb565(0, 0, 255); break;
				case 4: color = color_rgb565(255, 255, 0); break;
				case 5: color = color_rgb565(0, 255, 255); break;
				case 6: color = color_rgb565(255, 0, 255); break;
				case 7: color = color_rgb565(255, 128, 0); break;
				case 8: color = color_rgb565(128, 0, 255); break;
				case 9: color = color_rgb565(255, 255, 255); break;
				case 10: color = color_rgb565(64, 64, 64); break;
				case 11: color = color_rgb565(0, 128, 128); break;
				case 12: color = color_rgb565(128, 0, 128); break;
				case 13: color = color_rgb565(128, 128, 0); break;
				case 14: color = color_rgb565(255, 128, 128); break;
				case 15: color = color_rgb565(128, 128, 255); break;
				default: color = color_rgb565(255, 255, 255); break;
				}
			}
			row_buf[x] = color;
		}
		lcd_write_rgb565_pixels(row_buf, LCD_WIDTH);
	}

	cs_deselect();
	lcd_write_cmd(0x29);
}

void lcd_send_framebuffer(const uint16_t* fb) {
	lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

	gpio_put(LCD_PIN_DC, 1); // data mode
	cs_select();

	// Configure SPI for 16-bit transfer during DMA
	spi_set_format(LCD_SPI, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

	dma_channel_configure(
		dma_chan,
		&dma_cfg,
		&spi_get_hw(LCD_SPI)->dr, // write to SPI data register
		fb,                       // read from framebuffer
		LCD_WIDTH * LCD_HEIGHT,   // number of 16-bit transfers
		true                      // start immediately
	);
}

void lcd_wait_dma(void)
{
	if (dma_chan >= 0) {
		dma_channel_wait_for_finish_blocking(dma_chan);
		// Wait for SPI to finish transmitting
		while (spi_is_busy(LCD_SPI)) {
			tight_loop_contents();
		}
	}
	cs_deselect();
	// Restore 8-bit SPI format
	spi_set_format(LCD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void lcd_fill(uint16_t color) {
	lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

	gpio_put(LCD_PIN_DC, 1);
	cs_select();

	// Send color data as two bytes (big-endian)
	uint8_t hi = color >> 8;
	uint8_t lo = color & 0xFF;
	for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
		spi_write_blocking(LCD_SPI, &hi, 1);
		spi_write_blocking(LCD_SPI, &lo, 1);
	}

	cs_deselect();
}

void lcd_display(uint16_t* buff)
{
    lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
	gpio_put(LCD_PIN_DC, 1);

	cs_select();
	for (int i = 0; i < LCD_HEIGHT; i++) {
		spi_write_blocking(LCD_SPI, (uint8_t *)&buff[i * LCD_WIDTH], LCD_WIDTH * 2);       
    }
	cs_deselect();

	lcd_write_cmd(0x29);
}
