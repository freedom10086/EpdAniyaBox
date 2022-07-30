#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include "epd_lcd_ssd1680.h"
#include "epdpaint.h"

typedef struct {
    float temperature;
    bool temperature_valid;

    float altitude;
    bool altitude_valid;

    float wheel_speed;
    bool wheel_speed_valid;

    float crank_cadence;
    bool crank_cadence_valid;

    uint16_t heart_rate;
    bool heart_rate_valid;
} main_page_data_t;

void main_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

void main_page_update_temperature(float temp);

void main_page_update_altitude(float altitude);

void main_page_update_speed(float speed);

void main_page_update_crank_cadence(float crank_cadence);

void main_page_update_heart_rate(uint16_t heart_rate);

#endif