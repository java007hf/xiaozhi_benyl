#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_SPI_WIDTH   128
#define SSD1306_SPI_HEIGHT  64

typedef struct ssd1306_spi_t *ssd1306_spi_handle_t;

typedef struct {
    spi_host_device_t host;
    int pin_sclk;
    int pin_mosi;
    int pin_rst;
    int pin_dc;
    int clock_speed_hz;
} ssd1306_spi_config_t;

esp_err_t ssd1306_spi_new(const ssd1306_spi_config_t *config, ssd1306_spi_handle_t *out_handle);
esp_err_t ssd1306_spi_flush(ssd1306_spi_handle_t handle);
void ssd1306_spi_clear(ssd1306_spi_handle_t handle);
void ssd1306_spi_draw_pixel(ssd1306_spi_handle_t handle, int x, int y, bool on);
void ssd1306_spi_draw_rect(ssd1306_spi_handle_t handle, int x, int y, int w, int h);
void ssd1306_spi_draw_text(ssd1306_spi_handle_t handle, int x, int y, const char *text, int scale);

#ifdef __cplusplus
}
#endif
