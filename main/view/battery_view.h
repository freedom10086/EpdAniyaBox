#ifndef BATTERY_VIEW_H
#define BATTERY_VIEW_H

#include <stdio.h>
#include <stdlib.h>

#include "lcd/epdpaint.h"

typedef struct {
    int width;
    int height;

    int x, y; // 坐标x, y
    int battery_level;
} battery_view_t;

battery_view_t *battery_view_create(int x, int y, int width, int height);

void battery_view_draw(battery_view_t *battery_view, epd_paint_t *epd_paint, int8_t level, uint32_t loop_cnt);

void battery_view_deinit(battery_view_t *battery_view);

#endif