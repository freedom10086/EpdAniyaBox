/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <stdlib.h>
#include <sys/cdefs.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_check.h"

#include "esp_lcd_panel_ssd1680.h"

static const char *TAG = "lcd_panel.ssd1680";

static const uint8_t lut_full_update[] = {
        0x50, 0xAA, 0x55, 0xAA, 0x11, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xFF, 0xFF, 0x1F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//static const uint8_t lut_partial_update[] = {
//        0x10, 0x18, 0x18, 0x08, 0x18, 0x18,
//        0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
//        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//        0x00, 0x00, 0x13, 0x14, 0x44, 0x12,
//        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
//};

// ssd1680 commands
#define SSD1680_CMD_DRIVER_OUTPUT_CONTROL 0x01
#define SSD1680_CMD_BOOSTER_SOFT_START_CONTROL  0x0C
#define SSD1680_CMD_DEEP_SLEEP_MODE       0x10
#define SSD1680_CMD_DATA_ENTRY_MODE_SETTING 0x11
#define SSD1680_CMD_SOFT_RESET            0x12
#define SSD1680_CMD_TEMPERATURE_SENSOR_CONTROL 0x18
#define SSD1680_CMD_MASTER_ACTIVATION  0x20
#define SSD1680_CMD_DISPLAY_UPDATE_CONTROL_1      0x21
#define SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2      0x22
#define SSD1680_CMD_WRITE_RAM              0x24
#define SSD1680_CMD_WRITE_VCOM_REGISTER              0x2C
#define SSD1680_CMD_BORDER_WAVEFORM_CONTROL      0x3C
#define SSD1680_CMD_SET_RAM_X_START_END      0x44
#define SSD1680_CMD_SET_RAM_Y_START_END      0x45
#define SSD1680_CMD_SET_RAM_X_ADDRESS_COUNTER      0x4E
#define SSD1680_CMD_SET_RAM_Y_ADDRESS_COUNTER      0x4F

static esp_err_t panel_ssd1680_del(esp_lcd_panel_t *panel);

static esp_err_t panel_ssd1680_reset(esp_lcd_panel_t *panel);

static esp_err_t panel_ssd1680_init(esp_lcd_panel_t *panel);

static esp_err_t panel_ssd1680_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                           const void *color_data);

static esp_err_t panel_ssd1680_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);

static esp_err_t panel_ssd1680_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);

static esp_err_t panel_ssd1680_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);

static esp_err_t panel_ssd1680_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);

static esp_err_t panel_ssd1680_disp_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    int busy_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    unsigned int bits_per_pixel;
} ssd1680_panel_t;

esp_err_t
esp_lcd_new_panel_ssd1680(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                          esp_lcd_panel_handle_t *ret_panel) {
    esp_err_t ret = ESP_OK;
    ssd1680_panel_t *ssd1680 = NULL;
    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    ESP_GOTO_ON_FALSE(panel_dev_config->color_space == ESP_LCD_COLOR_SPACE_MONOCHROME, ESP_ERR_INVALID_ARG, err, TAG,
                      "support monochrome only");
    ESP_GOTO_ON_FALSE(panel_dev_config->bits_per_pixel == 1, ESP_ERR_INVALID_ARG, err, TAG, "bpp must be 1");
    ssd1680 = calloc(1, sizeof(ssd1680_panel_t));
    ESP_GOTO_ON_FALSE(ssd1680, ESP_ERR_NO_MEM, err, TAG, "no mem for ssd1680 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
                .mode = GPIO_MODE_OUTPUT,
                .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    int busy_gpio_num = -1;
    if (panel_dev_config->vendor_config) {
        ssd1680_vendor_config *vendor_config = (ssd1680_vendor_config *) panel_dev_config->vendor_config;
        busy_gpio_num = vendor_config->busy_gpio_num;
    }

    if (busy_gpio_num >= 0) {
        // setup gpio
        ESP_LOGI(TAG, "busy pin is %d", busy_gpio_num);
        gpio_config_t io_config = {
                .pin_bit_mask = (1ull << busy_gpio_num),
                .mode = GPIO_MODE_INPUT,
        };
        ESP_ERROR_CHECK(gpio_config(&io_config));
    }

    ssd1680->io = io;
    ssd1680->bits_per_pixel = panel_dev_config->bits_per_pixel;
    ssd1680->reset_gpio_num = panel_dev_config->reset_gpio_num;
    ssd1680->busy_gpio_num = busy_gpio_num;
    ssd1680->reset_level = panel_dev_config->flags.reset_active_high;
    ssd1680->base.del = panel_ssd1680_del;
    ssd1680->base.reset = panel_ssd1680_reset;
    ssd1680->base.init = panel_ssd1680_init;
    ssd1680->base.draw_bitmap = panel_ssd1680_draw_bitmap;
    ssd1680->base.invert_color = panel_ssd1680_invert_color;
    ssd1680->base.set_gap = panel_ssd1680_set_gap;
    ssd1680->base.mirror = panel_ssd1680_mirror;
    ssd1680->base.swap_xy = panel_ssd1680_swap_xy;
    ssd1680->base.disp_off = panel_ssd1680_disp_off;
    *ret_panel = &(ssd1680->base);
    ESP_LOGI(TAG, "new ssd1680 panel @%p", ssd1680);

    return ESP_OK;

    err:
    if (ssd1680) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(ssd1680);
    }
    return ret;
}

static esp_err_t panel_ssd1680_del(esp_lcd_panel_t *panel) {
    ssd1680_panel_t *ssd1680 = __containerof(panel, ssd1680_panel_t, base);
    if (ssd1680->reset_gpio_num >= 0) {
        gpio_reset_pin(ssd1680->reset_gpio_num);
    }
    if (ssd1680->busy_gpio_num >= 0) {
        gpio_reset_pin(ssd1680->busy_gpio_num);
    }

    // enter deep sleep mode
    esp_lcd_panel_io_tx_param(ssd1680->io, SSD1680_CMD_DEEP_SLEEP_MODE, (uint8_t[]) {0x011}, 1);

    ESP_LOGD(TAG, "del ssd1680 panel @%p", ssd1680);
    free(ssd1680);
    return ESP_OK;
}

static void wait_for_busy(ssd1680_panel_t *ssd1680) {
    //• Wait BUSY Low
    ESP_LOGI(TAG, "ssd1680 wait for busy ...");
    if (ssd1680->busy_gpio_num >= 0) {
        while (1) {
            if (gpio_get_level(ssd1680->busy_gpio_num)) {
                vTaskDelay(pdMS_TO_TICKS(5));
            } else {
                break;
            }
        }
    } else {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static esp_err_t panel_ssd1680_reset(esp_lcd_panel_t *panel) {
    ESP_LOGI(TAG, "lcd panel start reset.");
    ssd1680_panel_t *ssd1680 = __containerof(panel, ssd1680_panel_t, base);

    // perform hardware reset
    if (ssd1680->reset_gpio_num >= 0) {
        gpio_set_level(ssd1680->reset_gpio_num, !ssd1680->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(ssd1680->reset_gpio_num, ssd1680->reset_level);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_set_level(ssd1680->reset_gpio_num, !ssd1680->reset_level);
        vTaskDelay(pdMS_TO_TICKS(5));
        wait_for_busy(ssd1680);
    }

    ESP_LOGI(TAG, "lcd panel hw reset.");

    return ESP_OK;
}

static esp_err_t panel_ssd1680_sw_reset(esp_lcd_panel_t *panel) {
    ESP_LOGI(TAG, "lcd panel start reset.");
    ssd1680_panel_t *ssd1680 = __containerof(panel, ssd1680_panel_t, base);

    // sw reset
    esp_lcd_panel_io_tx_param(ssd1680->io, SSD1680_CMD_SOFT_RESET, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    //wait_for_busy(ssd1680);

    ESP_LOGI(TAG, "lcd panel sw reset OK.");

    return ESP_OK;
}

static esp_err_t
set_mem_area(ssd1680_panel_t *ssd1680, uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y) {
    //0x0F-->(15+1)*8=128
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    esp_lcd_panel_io_tx_param(ssd1680->io, SSD1680_CMD_SET_RAM_X_START_END,
                              (uint8_t[]) {(start_x >> 3) & 0xff, (end_x >> 3) & 0xff}, 2);

    esp_lcd_panel_io_tx_param(ssd1680->io, SSD1680_CMD_SET_RAM_Y_START_END,
                              (uint8_t[]) {start_y & 0xff, (start_y >> 8) & 0xff, end_y & 0xff, (end_y >> 8) & 0xff},
                              4);

    return ESP_OK;
}

static esp_err_t set_mem_pointer(ssd1680_panel_t *ssd1680, uint16_t x, uint16_t y) {
    esp_lcd_panel_io_tx_param(ssd1680->io, SSD1680_CMD_SET_RAM_X_ADDRESS_COUNTER, (uint8_t[]) {(x >> 3) & 0xff}, 1);
    esp_lcd_panel_io_tx_param(ssd1680->io, SSD1680_CMD_SET_RAM_Y_ADDRESS_COUNTER,
                              (uint8_t[]) {y & 0xff, (y >> 8) & 0xff}, 2);
    wait_for_busy(ssd1680);
    return ESP_OK;
}

static esp_err_t display_frame(ssd1680_panel_t *ssd1680) {
    esp_lcd_panel_io_tx_param(ssd1680->io, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2, (uint8_t[]) {0xF7}, 1);
    // update
    esp_lcd_panel_io_tx_param(ssd1680->io, SSD1680_CMD_MASTER_ACTIVATION, NULL, 0);
    wait_for_busy(ssd1680);
    return ESP_OK;
}

static esp_err_t display_frame_partial(ssd1680_panel_t *ssd1680) {
    esp_lcd_panel_io_tx_param(ssd1680->io, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2, (uint8_t[]) {0x0F}, 1);
    // update
    esp_lcd_panel_io_tx_param(ssd1680->io, SSD1680_CMD_MASTER_ACTIVATION, NULL, 0);
    wait_for_busy(ssd1680);
    return ESP_OK;
}

static esp_err_t clear_display(ssd1680_panel_t *ssd1680) {
    set_mem_area(ssd1680, 0, 0, LCD_H_RES - 1, LCD_V_RES - 1);
    set_mem_pointer(ssd1680, 0, 0);

    esp_lcd_panel_io_tx_param(ssd1680->io, SSD1680_CMD_WRITE_RAM, NULL, 0);

    size_t len = LCD_H_RES * LCD_V_RES * ssd1680->bits_per_pixel / 8;
    // TODO

    return ESP_OK;
}

static void set_lut_by_host(ssd1680_panel_t *ssd1680, uint8_t *lut, uint8_t len) {
    esp_lcd_panel_io_tx_param(ssd1680->io, 0x32, lut, len);
    wait_for_busy(ssd1680);
}

static esp_err_t panel_ssd1680_init(esp_lcd_panel_t *panel) {
    ssd1680_panel_t *ssd1680 = __containerof(panel, ssd1680_panel_t, base);
    esp_lcd_panel_io_handle_t io = ssd1680->io;

    ESP_LOGI(TAG, "ssd1680 start for init ...");
    // HW Reset
    // panel_ssd1680_reset(panel);

    // SW Reset by Command 0x12
    panel_ssd1680_sw_reset(panel);

    // Wait 10ms busy is high when sw reset
    vTaskDelay(pdMS_TO_TICKS(5));
    wait_for_busy(ssd1680);

    // 3. Send Initialization Code
    // Set gate driver output by Command 0x01
    esp_lcd_panel_io_tx_param(io, SSD1680_CMD_DRIVER_OUTPUT_CONTROL, (uint8_t[]) {(LCD_V_RES -1) & 0xff, ((LCD_V_RES - 1) >> 8) & 0xff, 0x00}, 3);
    wait_for_busy(ssd1680);

    esp_lcd_panel_io_tx_param(io, SSD1680_CMD_BOOSTER_SOFT_START_CONTROL, (uint8_t[]) {0xD7, 0xD6, 0x9D}, 3);
    esp_lcd_panel_io_tx_param(io, SSD1680_CMD_WRITE_VCOM_REGISTER, (uint8_t[]) {0xA8}, 1);
    esp_lcd_panel_io_tx_param(io, 0x3A, (uint8_t[]) {0x01}, 1);
    esp_lcd_panel_io_tx_param(io, 0x3B, (uint8_t[]) {0x08}, 1);


    // Set display RAM size by Command 0x11, 0x44, 0x45
    // 00 –Y decrement, X decrement, //01 –Y decrement, X increment, //10 –Y increment, X decrement, //11 –Y increment, X increment [POR]
    esp_lcd_panel_io_tx_param(io, SSD1680_CMD_DATA_ENTRY_MODE_SETTING, (uint8_t[]) {0x03}, 1);


    // set lut
    set_lut_by_host(ssd1680, lut_full_update, 30);


    set_mem_area(ssd1680, 0, 0, LCD_H_RES - 1, LCD_V_RES - 1);

    // Set panel border by Command 0x3C
    // esp_lcd_panel_io_tx_param(io, SSD1680_CMD_BORDER_WAVEFORM_CONTROL, (uint8_t[]) {0x80}, 1);

    //4. Load Waveform LUT
    //• Sense temperature by int/ext TS by Command 0x18
    //• Load waveform LUT from OTP by Command 0x22, 0x20 or by MCU
    esp_lcd_panel_io_tx_param(io, SSD1680_CMD_TEMPERATURE_SENSOR_CONTROL, (uint8_t[]) {0x80}, 1);

    esp_lcd_panel_io_tx_param(io, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_1, (uint8_t[]) {0x00, 0x80}, 2);

    //• Wait BUSY Low
    vTaskDelay(pdMS_TO_TICKS(10));

    // 5. Write Image and Drive Display Panel
    //• Write image data in RAM by Command 0x4E, 0x4F, 0x24, 0x26
    //• Set softstart setting by Command 0x0C
    //• Drive display panel by Command 0x22, 0x20
    set_mem_pointer(ssd1680, 0, 0);

    //• Wait BUSY Low
    wait_for_busy(ssd1680);

    //set_lut_by_host(ssd1680, WS_20_30);
    ESP_LOGI(TAG, "ssd1680 panel init ok !");
    return ESP_OK;
}

/**
 * x_start, y_start include, x_end, y_end not include
 */
static esp_err_t panel_ssd1680_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                           const void *color_data) {

//    if (1) {
//        return ESP_OK;
//    }

    ssd1680_panel_t *ssd1680 = __containerof(panel, ssd1680_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = ssd1680->io;
    // adding extra gap
    x_start += ssd1680->x_gap;
    x_end += ssd1680->x_gap;
    y_start += ssd1680->y_gap;
    y_end += ssd1680->y_gap;

    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    x_start &= 0xF8;
    uint16_t image_width = (x_end - x_start) & 0xF8;
    if (x_start + image_width > LCD_H_RES) {
        x_end = LCD_H_RES;
    }

    uint16_t image_height = y_end - y_start;
    if (y_start + image_height > LCD_V_RES) {
        y_end = LCD_V_RES;
    }

    set_mem_area(ssd1680, x_start, y_start, x_end - 1, y_end - 1);
    set_mem_pointer(ssd1680, x_start, y_start);

//    /* send the image data */
//    for (int j = 0; j < y_end - y_start; j++) {
//        for (int i = 0; i < (x_end - x_start) / 8; i++) {
//            //
//            //esp_lcd_panel_io_tx_color(io, 0, color_data, len);
//            //SendData(image_buffer[i + j * (image_width / 8)]);
//            esp_lcd_panel_io_tx_param(io, SSD1680_CMD_WRITE_RAM, color_data, 1);
//            color_data++;
//        }
//    }

    size_t len = (y_end - y_start) * (x_end - x_start) * ssd1680->bits_per_pixel / 8;
    // transfer frame buffer
    esp_lcd_panel_io_tx_color(io, SSD1680_CMD_WRITE_RAM, color_data, len);

    display_frame(ssd1680);

    return ESP_OK;
}

static esp_err_t panel_ssd1680_invert_color(esp_lcd_panel_t *panel, bool invert_color_data) {
    ssd1680_panel_t *ssd1680 = __containerof(panel, ssd1680_panel_t, base);
    esp_lcd_panel_io_handle_t io = ssd1680->io;
    //int command = 0;
    //if (invert_color_data) {
    //    command = SSD1680_CMD_INVERT_ON;
    //} else {
    //    command = SSD1680_CMD_INVERT_OFF;
    //}
    //esp_lcd_panel_io_tx_param(io, command, NULL, 0);
    return ESP_OK;
}

static esp_err_t panel_ssd1680_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y) {
    ssd1680_panel_t *ssd1680 = __containerof(panel, ssd1680_panel_t, base);
    esp_lcd_panel_io_handle_t io = ssd1680->io;
    return ESP_OK;
}

static esp_err_t panel_ssd1680_swap_xy(esp_lcd_panel_t *panel, bool swap_axes) {
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t panel_ssd1680_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap) {
    ssd1680_panel_t *ssd1680 = __containerof(panel, ssd1680_panel_t, base);
    ssd1680->x_gap = x_gap;
    ssd1680->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_ssd1680_disp_off(esp_lcd_panel_t *panel, bool off) {
    ssd1680_panel_t *ssd1680 = __containerof(panel, ssd1680_panel_t, base);
    esp_lcd_panel_io_handle_t io = ssd1680->io;
    // no need on off
    return ESP_OK;
}
