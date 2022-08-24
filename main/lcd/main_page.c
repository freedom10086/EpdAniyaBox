#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <string.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "bike_common.h"
#include "main_page.h"
#include "epdpaint.h"


/*********************
 *      DEFINES
 *********************/
#define TAG "main-page"

static main_page_data_t main_page_data = {
        .temperature_valid = false,
        .altitude_valid = false,
};

static char draw_text_buf[20] = {0};

static int draw_speed_area(epd_paint_t *epd_paint, int y) {
    // speed
    float wheel_speed = main_page_data.wheel_speed;
    if (main_page_data.wheel_speed_valid) {
        if (wheel_speed > 999.9) {
            wheel_speed = 999.9f;
        }
        sprintf(draw_text_buf, "%02d", (int) wheel_speed);
    } else {
        sprintf(draw_text_buf, "00");
    }
    uint8_t speed_start_x_1 = LCD_H_RES / 2 - 6 - Font36.Width * 2;
    epd_paint_draw_string_at(epd_paint, speed_start_x_1, y, draw_text_buf, &Font36, 1);

    // speed point
    uint8_t start_speed_point_x = speed_start_x_1 + Font36.Width * strlen(draw_text_buf) + 3;
    epd_paint_draw_filled_rectangle(epd_paint, start_speed_point_x + 4, y + Font36.Height - 5,
                                    start_speed_point_x,
                                    y + Font36.Height - 1, 1);

    if (main_page_data.wheel_speed_valid) {
        if (strlen(draw_text_buf) == 2) {
            sprintf(draw_text_buf, "%02d", (int)(wheel_speed * 100) % 100);
        } else {
            sprintf(draw_text_buf, "%d", (int)(wheel_speed * 10) % 10);
        }
    } else {
        sprintf(draw_text_buf, "00");
    }
    epd_paint_draw_string_at(epd_paint, start_speed_point_x + 6, y, draw_text_buf, &Font36, 1);

    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + 12, "k", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + Font8.Height + 12, "m", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + 2 * Font8.Height + 12, "/", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + 3 * Font8.Height + 12, "h", &Font8, 1);

    // avg and max speed
    y += 40;
    epd_paint_draw_string_at(epd_paint, 16, y, "avg:28.1", &Font16_2, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES / 2 + 16, y, "max:28.1", &Font16_2, 1);

    return y;
}

void main_page_update_altitude(float altitude) {
    main_page_data.altitude_valid = true;
    main_page_data.altitude = altitude;
}

void main_page_update_speed(float speed) {
    main_page_data.wheel_speed = speed;
    main_page_data.wheel_speed_valid = true;
}

void main_page_update_crank_cadence(float crank_cadence) {
    main_page_data.crank_cadence = crank_cadence;
    main_page_data.crank_cadence_valid = true;
}

void main_page_update_heart_rate(uint16_t heart_rate) {
    main_page_data.heart_rate = heart_rate;
    main_page_data.heart_rate_valid = true;
}

void main_page_update_temp_hum(float temp, float hum) {
    main_page_data.temperature = temp;
    main_page_data.temperature_valid = true;

    main_page_data.humility = hum;
    main_page_data.humility_valid = true;
}

void main_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);

    int y = 0;
    // status bar
    // time

    if (loop_cnt % 2 == 0) {
        epd_paint_draw_string_at(epd_paint, (LCD_H_RES - Font16.Width * 5) / 2, y, "22:54", &Font16, 1);
    } else {
        epd_paint_draw_string_at(epd_paint, (LCD_H_RES - Font16.Width * 5) / 2, y, "22 54", &Font16, 1);
    }

    // gps
    epd_paint_draw_string_at(epd_paint, 0, y - 2, "G:12", &Font16_2, 1);

    // temperature
    if (main_page_data.temperature_valid) {
        sprintf(draw_text_buf, "T:%.1f", main_page_data.temperature);
    } else {
        sprintf(draw_text_buf, "T:--");
    }
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font16_2.Width * strlen(draw_text_buf), y - 2, draw_text_buf, &Font16_2, 1);
    epd_paint_draw_horizontal_line(epd_paint, 0, y + 16, LCD_H_RES, 1);

    y += 22;
    y = draw_speed_area(epd_paint, y);

    y += 18;
    epd_paint_draw_horizontal_line(epd_paint, 0, y, LCD_H_RES, 1);
    epd_paint_draw_vertical_line(epd_paint, LCD_H_RES / 2, y, LCD_V_RES - y - 1, 1);

    y += 4;
    // heart rate
    if (main_page_data.heart_rate_valid) {
        sprintf(draw_text_buf, "%d", main_page_data.heart_rate);
    } else {
        sprintf(draw_text_buf, "--");
    }
    epd_paint_draw_string_at(epd_paint, 4, y, draw_text_buf, &Font32, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES / 2 - Font8.Width - 4, y + 6, "b", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES / 2 - Font8.Width - 4, y + Font8.Height + 6, "p", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES / 2 - Font8.Width - 4, y + 2 * Font8.Height + 6, "m", &Font8, 1);

    // crank cadence valid
    if (main_page_data.crank_cadence_valid) {
        sprintf(draw_text_buf, "%.1f", main_page_data.crank_cadence);
    } else {
        sprintf(draw_text_buf, "0.0");
    }
    epd_paint_draw_string_at(epd_paint, LCD_H_RES / 2 + 4, y, draw_text_buf, &Font32, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + 6, "r", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + Font8.Height + 6, "p", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + 2 * Font8.Height + 6, "m", &Font8, 1);

    y += 36;
    epd_paint_draw_horizontal_line(epd_paint, 0, y, LCD_H_RES, 1);

    // time and distance
    y += 4;
    epd_paint_draw_string_at(epd_paint, 4, y, "1:27", &Font32, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES / 2 - Font8.Width - 4, y + 6, "m", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES / 2 - Font8.Width - 4, y + Font8.Height + 6, "i", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES / 2 - Font8.Width - 4, y + 2 * Font8.Height + 6, "n", &Font8, 1);

    epd_paint_draw_string_at(epd_paint, LCD_H_RES / 2 + 4, y, "18.6", &Font32, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + Font8.Height + 6, "k", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + 2 * Font8.Height + 6, "m", &Font8, 1);

    y += 36;
    epd_paint_draw_horizontal_line(epd_paint, 0, y, LCD_H_RES, 1);
    y += 4;

    // altitude
    if (main_page_data.altitude_valid) {
        sprintf(draw_text_buf, "%.1f", main_page_data.altitude);
    } else {
        sprintf(draw_text_buf, "--");
    }
    epd_paint_draw_string_at(epd_paint, 4, y, draw_text_buf, &Font32, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES / 2 - Font8.Width - 4, y + 2 * Font8.Height + 8, "m", &Font8, 1);

    // degree
    epd_paint_draw_string_at(epd_paint, LCD_H_RES / 2 + 4, y, "3.2", &Font32, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + 6, "d", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + Font8.Height + 6, "e", &Font8, 1);
    epd_paint_draw_string_at(epd_paint, LCD_H_RES - Font8.Width - 4, y + 2 * Font8.Height + 6, "g", &Font8, 1);
}