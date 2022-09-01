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
#include "esp_mac.h"
#include "esp_wifi.h"
#include <math.h>

#include "bike_common.h"
#include "digi_view.h"

#define TAG "digi-view"

//#define DIGI_LINE_WIDTH 7
//#define DIGI_GAP 2
//#define DIGI_LINE_LENGTH 44

// 7段点阵编码 .是7
//  ——      // 0
// |  |     // 1  2
//  ——      // 3
// |  |     // 4  5
//  ——      // 6
static uint8_t draw_map[] = {
        0b01110111, // 0
        0b00100100,  // 1
        0b01011101,  // 2
        0b01101101,
        0b00101110,
        0b01101011,
        0b01111011,
        0b00100101,
        0b01111111,
        0b01101111,
        0b01011011,  // e
};

uint8_t draw_digi_minus(digi_view_t *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {
    uint8_t DIGI_LINE_WIDTH = view->digi_thick;
    uint8_t DIGI_GAP = view->digi_gap;
    uint8_t DIGI_LINE_LENGTH = view->digi_width;

    epd_paint_draw_horizontal_line(epd_paint, x,
                                   y + DIGI_LINE_LENGTH - 1,
                                   DIGI_LINE_LENGTH / 2 - DIGI_LINE_WIDTH / 2 * 2, 1);
    for (int j = 1; j <= DIGI_LINE_WIDTH / 2; ++j) {
        epd_paint_draw_horizontal_line(epd_paint, x + j,
                                       y + DIGI_LINE_LENGTH - 1 - j,
                                       DIGI_LINE_LENGTH / 2 - DIGI_LINE_WIDTH / 2 * 2 - j * 2, 1);
        epd_paint_draw_horizontal_line(epd_paint, x + j,
                                       y + DIGI_LINE_LENGTH - 1 + j,
                                       DIGI_LINE_LENGTH / 2 - DIGI_LINE_WIDTH / 2 * 2 - j * 2, 1);
    }

    return DIGI_LINE_LENGTH / 2 - DIGI_LINE_WIDTH / 2 * 2 + DIGI_GAP;
}

int draw_digi_number(digi_view_t *view, epd_paint_t *epd_paint, uint8_t number, uint8_t x, uint8_t y, bool has_point) {
    uint8_t DIGI_LINE_WIDTH = view->digi_thick;
    uint8_t DIGI_GAP = view->digi_gap;
    uint8_t DIGI_LINE_LENGTH = view->digi_width;

    uint8_t mask = draw_map[number];
    if (has_point) {
        mask = mask | 0b10000000;
    }

    for (int i = 0; i < 8; ++i) {
        uint8_t bit = (mask >> i) & 0x01;
        if (!bit) {
            continue;
        }

        if (i == 0) {
            for (int j = 0; j < DIGI_LINE_WIDTH; ++j) {
                epd_paint_draw_horizontal_line(epd_paint, x + j + DIGI_GAP, y + j,
                                               DIGI_LINE_LENGTH - j * 2 - DIGI_GAP * 2, 1);
            }
        } else if (i == 3) {
            epd_paint_draw_horizontal_line(epd_paint, x + DIGI_LINE_WIDTH / 2 + DIGI_GAP,
                                           y + DIGI_LINE_LENGTH - 1,
                                           DIGI_LINE_LENGTH - DIGI_GAP * 2 - DIGI_LINE_WIDTH / 2 * 2, 1);
            for (int j = 1; j <= DIGI_LINE_WIDTH / 2; ++j) {
                epd_paint_draw_horizontal_line(epd_paint, x + DIGI_LINE_WIDTH / 2 + DIGI_GAP + j,
                                               y + DIGI_LINE_LENGTH - 1 - j,
                                               DIGI_LINE_LENGTH - DIGI_GAP * 2 - DIGI_LINE_WIDTH / 2 * 2 - j * 2, 1);
                epd_paint_draw_horizontal_line(epd_paint, x + DIGI_LINE_WIDTH / 2 + DIGI_GAP + j,
                                               y + DIGI_LINE_LENGTH - 1 + j,
                                               DIGI_LINE_LENGTH - DIGI_GAP * 2 - DIGI_LINE_WIDTH / 2 * 2 - j * 2, 1);
            }
        } else if (i == 6) {
            for (int j = 0; j < DIGI_LINE_WIDTH; ++j) {
                epd_paint_draw_horizontal_line(epd_paint, x + j + DIGI_GAP,
                                               y + DIGI_LINE_LENGTH * 2 - 2 - j,
                                               DIGI_LINE_LENGTH - j * 2 - DIGI_GAP * 2, 1);
            }
        } else if (i == 1) {
            for (int j = 0; j < DIGI_LINE_WIDTH; ++j) {
                if (j <= DIGI_LINE_WIDTH / 2) {
                    epd_paint_draw_vertical_line(epd_paint, x + j,
                                                 y + j + DIGI_GAP,
                                                 DIGI_LINE_LENGTH - DIGI_GAP * 2 - (DIGI_LINE_WIDTH / 2),
                                                 1);
                } else {
                    epd_paint_draw_vertical_line(epd_paint, x + j,
                                                 y + j + DIGI_GAP,
                                                 DIGI_LINE_LENGTH - (j - DIGI_LINE_WIDTH / 2) * 2 - DIGI_GAP * 2 -
                                                 (DIGI_LINE_WIDTH / 2), 1);
                }
            }
        } else if (i == 2) {
            for (int j = 0; j < DIGI_LINE_WIDTH; ++j) {
                if (j <= DIGI_LINE_WIDTH / 2) {
                    epd_paint_draw_vertical_line(epd_paint, x + DIGI_LINE_LENGTH - 1 - j,
                                                 y + j + DIGI_GAP,
                                                 DIGI_LINE_LENGTH - DIGI_GAP * 2 - (DIGI_LINE_WIDTH / 2),
                                                 1);
                } else {
                    epd_paint_draw_vertical_line(epd_paint, x + DIGI_LINE_LENGTH - 1 - j,
                                                 y + j + DIGI_GAP,
                                                 DIGI_LINE_LENGTH - (j - DIGI_LINE_WIDTH / 2) * 2 - DIGI_GAP * 2 -
                                                 (DIGI_LINE_WIDTH / 2), 1);
                }
            }
        } else if (i == 4) {
            for (int j = 0; j < DIGI_LINE_WIDTH; ++j) {
                if (j > DIGI_LINE_WIDTH / 2) {
                    epd_paint_draw_vertical_line(epd_paint, x + j,
                                                 y + j + DIGI_LINE_LENGTH - 1 - DIGI_LINE_WIDTH / 2 + DIGI_GAP,
                                                 DIGI_LINE_LENGTH - (j - DIGI_LINE_WIDTH / 2) * 2 - DIGI_GAP * 2 -
                                                 (DIGI_LINE_WIDTH / 2),
                                                 1);
                } else {
                    epd_paint_draw_vertical_line(epd_paint, x + j,
                                                 y + j + DIGI_LINE_LENGTH - 1 - DIGI_LINE_WIDTH / 2 + DIGI_GAP -
                                                 (j - DIGI_LINE_WIDTH / 2) * 2,
                                                 DIGI_LINE_LENGTH - DIGI_GAP * 2 - (DIGI_LINE_WIDTH / 2), 1);
                }
            }
        } else if (i == 5) {
            for (int j = 0; j < DIGI_LINE_WIDTH; ++j) {
                if (j > DIGI_LINE_WIDTH / 2) {
                    epd_paint_draw_vertical_line(epd_paint, x - j + DIGI_LINE_LENGTH - 1,
                                                 y + j + DIGI_LINE_LENGTH - 1 - DIGI_LINE_WIDTH / 2 + DIGI_GAP,
                                                 DIGI_LINE_LENGTH - (j - DIGI_LINE_WIDTH / 2) * 2 - DIGI_GAP * 2 -
                                                 (DIGI_LINE_WIDTH / 2),
                                                 1);
                } else {
                    epd_paint_draw_vertical_line(epd_paint, x - j + DIGI_LINE_LENGTH - 1,
                                                 y + j + DIGI_LINE_LENGTH - 1 - DIGI_LINE_WIDTH / 2 + DIGI_GAP -
                                                 (j - DIGI_LINE_WIDTH / 2) * 2,
                                                 DIGI_LINE_LENGTH - DIGI_GAP * 2 - (DIGI_LINE_WIDTH / 2), 1);
                }
            }
        } else {
            // is 7 draw point
            epd_paint_draw_filled_rectangle(epd_paint, x + DIGI_LINE_LENGTH + DIGI_LINE_WIDTH / 2 + DIGI_GAP,
                                            y + DIGI_LINE_LENGTH * 2 - DIGI_LINE_WIDTH - DIGI_GAP,
                                            x + DIGI_LINE_LENGTH + DIGI_LINE_WIDTH + DIGI_GAP,
                                            y + DIGI_LINE_LENGTH * 2 - DIGI_LINE_WIDTH / 2 - DIGI_GAP,
                                            1);
        }
    }

    uint8_t xdd = DIGI_LINE_LENGTH + DIGI_LINE_WIDTH + (has_point ? (DIGI_LINE_WIDTH + DIGI_GAP) : 0);
    return xdd;
}


digi_view_t *digi_view_create(int x, int y, int digi_width, int digi_thick, int digi_gap) {
    ESP_LOGI(TAG, "init digi view");
    digi_view_t *view = malloc(sizeof(digi_view_t));
    if (!view) {
        ESP_LOGE(TAG, "no memory for init digi view");
        return NULL;
    }

    view->x = x;
    view->y = y;
    view->digi_width = digi_width;
    view->digi_thick = digi_thick;
    view->digi_gap = digi_gap;

    ESP_LOGI(TAG, "digi view created");
    return view;
}

void digi_view_set_text(digi_view_t *digi_view, int number, int decimal, uint8_t decimal_len) {
    digi_view->number = number;
    digi_view->decimal = decimal;
    digi_view->decimal_len = decimal_len;
}

void digi_view_draw(digi_view_t *digi_view, epd_paint_t *epd_paint, uint32_t loop_cnt) {
    bool is_minus = digi_view->number < 0 || (digi_view->decimal_len > 0 && digi_view->decimal < 0); // TODO
    int number = abs(digi_view->number);

    uint8_t number_cnt = 0;

    while (number > 0) {
        number = number / 10;
        number_cnt++;
    }

    uint8_t x = digi_view->x;
    uint8_t y = digi_view->y;

    if (is_minus) {
        x += draw_digi_minus(digi_view, epd_paint, x, y);
    }

    // if number count is 0 add 0 in front
    uint8_t draw_num = 0;
    if (number_cnt == 0) {
        bool has_point = digi_view->decimal_len > 0;
        x += draw_digi_number(digi_view, epd_paint, 0, x, y, has_point);
    } else {
        for (int i = 0; i < number_cnt; ++i) {
            int a = 1;
            for (int j = 0; j < number_cnt - i - 1; j++) {
                a *= 10;
            }

            bool has_point = (i == number_cnt - 1) && digi_view->decimal_len > 0;
            draw_num = abs(digi_view->number) / a % 10;
            // ESP_LOGI(TAG, "draw num %d %d %d %d", draw_num, a, digi_view->number, number_cnt);
            x += draw_digi_number(digi_view, epd_paint, draw_num, x, y, has_point);
        }
    }

    if (digi_view->decimal_len > 0) {
        for (int i = 0; i < digi_view->decimal_len; ++i) {
            int a = 1;
            for (int j = 0; j < digi_view->decimal_len - i - 1; j++) {
                a *= 10;
            }
            draw_num = abs(digi_view->decimal) / a % 10;
            x += draw_digi_number(digi_view, epd_paint, draw_num, x, y, false);
        }
    }
}

void digi_view_draw_ee(digi_view_t *digi_view, epd_paint_t *epd_paint, uint8_t ee_cnt, uint32_t loop_cnt) {
    uint8_t x = digi_view->x;
    uint8_t y = digi_view->y;
    for (int i = 0; i < ee_cnt; ++i) {
        x += draw_digi_number(digi_view, epd_paint, 10, x, y, false);
    }
}

void digi_view_deinit(digi_view_t *digi_view) {

    if (digi_view != NULL) {
        free(digi_view);
        digi_view = NULL;
    }
}