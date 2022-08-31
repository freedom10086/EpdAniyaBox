#ifndef DIGI_VIEW_H
#define DIGI_VIEW_H

#include <stdio.h>
#include <stdlib.h>

#include "lcd/epdpaint.h"

typedef struct {
    int number; // 整数部分  18
    int decimal; // 小数部分 9 = 18.9
    int decimal_len; // 小数长度 0 表示没有

    int x, y; // 坐标x, y

    int digi_width; // 每个文字宽度
    int digi_thick; // 厚度
    int digi_gap; // 每个数码管间隔
} digi_view_t;

digi_view_t *digi_view_create(int x, int y, int digi_width, int digi_thick, int digi_gap);

void digi_view_set_text(digi_view_t *digi_view, int number, int decimal, uint8_t decimal_len);

void digi_view_draw(digi_view_t *digi_view, epd_paint_t *epd_paint, uint32_t loop_cnt);

void digi_view_draw_ee(digi_view_t *digi_view, epd_paint_t *epd_paint, uint8_t ee_cnt, uint32_t loop_cnt);

void digi_view_deinit(digi_view_t *digi_view);


#endif