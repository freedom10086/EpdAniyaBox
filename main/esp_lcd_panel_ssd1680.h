/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_lcd_panel_vendor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_H_RES 152
#define LCD_V_RES 296

typedef struct {
    spi_device_handle_t spi_dev;
    int busy_gpio_num;
    int reset_gpio_num; /*!< GPIO used to reset the LCD panel, set to -1 if it's not used */
    bool reset_level;
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

esp_err_t panel_ssd1680_init(lcd_ssd1680_panel_t *panel);

esp_err_t panel_ssd1680_init_partial(lcd_ssd1680_panel_t *panel);

esp_err_t panel_ssd1680_reset(lcd_ssd1680_panel_t *panel);

esp_err_t panel_ssd1680_draw_bitmap(lcd_ssd1680_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                           const void *color_data) ;

esp_err_t panel_ssd1680_sleep(lcd_ssd1680_panel_t *panel);

esp_err_t panel_ssd1680_del(lcd_ssd1680_panel_t *panel);


#ifdef __cplusplus
}
#endif