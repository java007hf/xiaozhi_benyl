#include "ssd1306_spi.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SSD1306_PAGES (SSD1306_SPI_HEIGHT / 8)

struct ssd1306_spi_t {
    spi_device_handle_t spi;
    int pin_dc;
    int pin_rst;
    uint8_t fb[SSD1306_SPI_WIDTH * SSD1306_PAGES];
};

static const char *TAG = "ssd1306_spi";

static const uint8_t font5x7[][5] = {
    [' ' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00},
    ['!' - 32] = {0x00, 0x00, 0x5f, 0x00, 0x00},
    ['-' - 32] = {0x08, 0x08, 0x08, 0x08, 0x08},
    ['0' - 32] = {0x3e, 0x51, 0x49, 0x45, 0x3e},
    ['1' - 32] = {0x00, 0x42, 0x7f, 0x40, 0x00},
    ['2' - 32] = {0x42, 0x61, 0x51, 0x49, 0x46},
    ['3' - 32] = {0x21, 0x41, 0x45, 0x4b, 0x31},
    ['4' - 32] = {0x18, 0x14, 0x12, 0x7f, 0x10},
    ['5' - 32] = {0x27, 0x45, 0x45, 0x45, 0x39},
    ['6' - 32] = {0x3c, 0x4a, 0x49, 0x49, 0x30},
    ['7' - 32] = {0x01, 0x71, 0x09, 0x05, 0x03},
    ['8' - 32] = {0x36, 0x49, 0x49, 0x49, 0x36},
    ['9' - 32] = {0x06, 0x49, 0x49, 0x29, 0x1e},
    [':' - 32] = {0x00, 0x36, 0x36, 0x00, 0x00},
    ['A' - 32] = {0x7e, 0x11, 0x11, 0x11, 0x7e},
    ['B' - 32] = {0x7f, 0x49, 0x49, 0x49, 0x36},
    ['C' - 32] = {0x3e, 0x41, 0x41, 0x41, 0x22},
    ['D' - 32] = {0x7f, 0x41, 0x41, 0x22, 0x1c},
    ['E' - 32] = {0x7f, 0x49, 0x49, 0x49, 0x41},
    ['F' - 32] = {0x7f, 0x09, 0x09, 0x09, 0x01},
    ['G' - 32] = {0x3e, 0x41, 0x49, 0x49, 0x7a},
    ['H' - 32] = {0x7f, 0x08, 0x08, 0x08, 0x7f},
    ['I' - 32] = {0x00, 0x41, 0x7f, 0x41, 0x00},
    ['J' - 32] = {0x20, 0x40, 0x41, 0x3f, 0x01},
    ['K' - 32] = {0x7f, 0x08, 0x14, 0x22, 0x41},
    ['L' - 32] = {0x7f, 0x40, 0x40, 0x40, 0x40},
    ['M' - 32] = {0x7f, 0x02, 0x0c, 0x02, 0x7f},
    ['N' - 32] = {0x7f, 0x04, 0x08, 0x10, 0x7f},
    ['O' - 32] = {0x3e, 0x41, 0x41, 0x41, 0x3e},
    ['P' - 32] = {0x7f, 0x09, 0x09, 0x09, 0x06},
    ['Q' - 32] = {0x3e, 0x41, 0x51, 0x21, 0x5e},
    ['R' - 32] = {0x7f, 0x09, 0x19, 0x29, 0x46},
    ['S' - 32] = {0x46, 0x49, 0x49, 0x49, 0x31},
    ['T' - 32] = {0x01, 0x01, 0x7f, 0x01, 0x01},
    ['U' - 32] = {0x3f, 0x40, 0x40, 0x40, 0x3f},
    ['V' - 32] = {0x1f, 0x20, 0x40, 0x20, 0x1f},
    ['W' - 32] = {0x3f, 0x40, 0x38, 0x40, 0x3f},
    ['X' - 32] = {0x63, 0x14, 0x08, 0x14, 0x63},
    ['Y' - 32] = {0x07, 0x08, 0x70, 0x08, 0x07},
    ['Z' - 32] = {0x61, 0x51, 0x49, 0x45, 0x43},
};

static esp_err_t ssd1306_write(ssd1306_spi_handle_t handle, bool data, const uint8_t *buf, size_t len)
{
    gpio_set_level(handle->pin_dc, data);

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = buf,
    };
    return spi_device_polling_transmit(handle->spi, &t);
}

static esp_err_t ssd1306_cmd(ssd1306_spi_handle_t handle, uint8_t cmd)
{
    return ssd1306_write(handle, false, &cmd, 1);
}

esp_err_t ssd1306_spi_new(const ssd1306_spi_config_t *config, ssd1306_spi_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    esp_err_t ret = ESP_OK;
    ssd1306_spi_handle_t handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_NO_MEM, TAG, "no memory for oled handle");

    handle->pin_dc = config->pin_dc;
    handle->pin_rst = config->pin_rst;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << handle->pin_dc) | (1ULL << handle->pin_rst),
        .mode = GPIO_MODE_OUTPUT,
    };
    ret = gpio_config(&io_conf);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ret, fail, TAG, "gpio_config failed");

    spi_bus_config_t buscfg = {
        .mosi_io_num = config->pin_mosi,
        .miso_io_num = -1,
        .sclk_io_num = config->pin_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof(handle->fb),
    };
    ret = spi_bus_initialize(config->host, &buscfg, SPI_DMA_CH_AUTO);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ret, fail, TAG, "spi_bus_initialize failed");

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = config->clock_speed_hz,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };
    ret = spi_bus_add_device(config->host, &devcfg, &handle->spi);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ret, fail, TAG, "spi_bus_add_device failed");

    gpio_set_level(handle->pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(handle->pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    const uint8_t init_cmds[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6,
        0xAF,
    };

    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        ret = ssd1306_cmd(handle, init_cmds[i]);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ret, fail, TAG, "init command failed");
    }

    *out_handle = handle;
    return ESP_OK;

fail:
    free(handle);
    return ret;
}

void ssd1306_spi_clear(ssd1306_spi_handle_t handle)
{
    if (handle) {
        memset(handle->fb, 0, sizeof(handle->fb));
    }
}

void ssd1306_spi_draw_pixel(ssd1306_spi_handle_t handle, int x, int y, bool on)
{
    if (!handle || x < 0 || x >= SSD1306_SPI_WIDTH || y < 0 || y >= SSD1306_SPI_HEIGHT) {
        return;
    }

    uint16_t index = x + (y / 8) * SSD1306_SPI_WIDTH;
    uint8_t mask = 1U << (y % 8);
    if (on) {
        handle->fb[index] |= mask;
    } else {
        handle->fb[index] &= ~mask;
    }
}

void ssd1306_spi_draw_rect(ssd1306_spi_handle_t handle, int x, int y, int w, int h)
{
    for (int i = 0; i < w; i++) {
        ssd1306_spi_draw_pixel(handle, x + i, y, true);
        ssd1306_spi_draw_pixel(handle, x + i, y + h - 1, true);
    }
    for (int i = 0; i < h; i++) {
        ssd1306_spi_draw_pixel(handle, x, y + i, true);
        ssd1306_spi_draw_pixel(handle, x + w - 1, y + i, true);
    }
}

static void draw_char(ssd1306_spi_handle_t handle, int x, int y, char c, int scale)
{
    if (c < 32 || c > 'Z') {
        c = ' ';
    }

    const uint8_t *glyph = font5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (glyph[col] & (1U << row)) {
                for (int sx = 0; sx < scale; sx++) {
                    for (int sy = 0; sy < scale; sy++) {
                        ssd1306_spi_draw_pixel(handle, x + col * scale + sx, y + row * scale + sy, true);
                    }
                }
            }
        }
    }
}

void ssd1306_spi_draw_text(ssd1306_spi_handle_t handle, int x, int y, const char *text, int scale)
{
    if (!handle || !text || scale < 1) {
        return;
    }

    while (*text) {
        draw_char(handle, x, y, *text++, scale);
        x += 6 * scale;
    }
}

esp_err_t ssd1306_spi_flush(ssd1306_spi_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    ESP_RETURN_ON_ERROR(ssd1306_cmd(handle, 0x21), TAG, "set column address failed");
    ESP_RETURN_ON_ERROR(ssd1306_cmd(handle, 0), TAG, "set column start failed");
    ESP_RETURN_ON_ERROR(ssd1306_cmd(handle, SSD1306_SPI_WIDTH - 1), TAG, "set column end failed");
    ESP_RETURN_ON_ERROR(ssd1306_cmd(handle, 0x22), TAG, "set page address failed");
    ESP_RETURN_ON_ERROR(ssd1306_cmd(handle, 0), TAG, "set page start failed");
    ESP_RETURN_ON_ERROR(ssd1306_cmd(handle, SSD1306_PAGES - 1), TAG, "set page end failed");

    return ssd1306_write(handle, true, handle->fb, sizeof(handle->fb));
}
