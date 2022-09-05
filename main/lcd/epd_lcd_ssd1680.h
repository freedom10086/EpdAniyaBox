/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_io.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_H_RES 200
#define LCD_V_RES 200

typedef struct {
    spi_device_handle_t spi_dev;
    int busy_gpio_num;
    int reset_gpio_num; /*!< GPIO used to reset the LCD panel, set to -1 if it's not used */
    bool reset_level;
    bool _using_partial_mode;

    int _current_mem_area_start_x;
    int _current_mem_area_end_x;
    int _current_mem_area_start_y;
    int _current_mem_area_end_y;
    int _current_mem_pointer_x;
    int _current_mem_pointer_y;
} lcd_ssd1680_panel_t;

/**
 * @brief Create LCD panel for model GC9A01
 *
 * @param[in] io LCD panel IO handle
 * @param[in] panel_dev_config general panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid
 *          - ESP_ERR_NO_MEM        if out of memory
 *          - ESP_OK                on success
 */
esp_err_t new_panel_ssd1680(lcd_ssd1680_panel_t *panel,
                            spi_host_device_t bus,
                            const esp_lcd_panel_io_spi_config_t *io_config);

esp_err_t panel_ssd1680_init_full(lcd_ssd1680_panel_t *panel);

esp_err_t panel_ssd1680_init_partial(lcd_ssd1680_panel_t *panel);

esp_err_t panel_ssd1680_reset(lcd_ssd1680_panel_t *panel);

esp_err_t panel_ssd1680_clear_display(lcd_ssd1680_panel_t *panel, uint8_t color);

esp_err_t panel_ssd1680_draw_bitmap(lcd_ssd1680_panel_t *panel, int16_t x_start, int16_t y_start, int16_t x_end, int16_t y_end,
                                           const void *color_data) ;

esp_err_t panel_ssd1680_refresh(lcd_ssd1680_panel_t *panel, bool partial_update_mode);

esp_err_t panel_ssd1680_refresh_area(lcd_ssd1680_panel_t *panel, int16_t x, int16_t y, int16_t end_x, int16_t end_y);

esp_err_t panel_ssd1680_sleep(lcd_ssd1680_panel_t *panel);

esp_err_t panel_ssd1680_del(lcd_ssd1680_panel_t *panel);


#ifdef __cplusplus
}
#endif