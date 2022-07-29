/**
 *  @filename   :   epdpaint.h
 *  @brief      :   Header file for epdpaint.cpp
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     July 28 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef EPDPAINT_H
#define EPDPAINT_H

// Display orientation
#define ROTATE_0            0
#define ROTATE_90           1
#define ROTATE_180          2
#define ROTATE_270          3

// Color inverse. 1 or 0 = set or reset a bit if set a colored pixel
#define IF_INVERT_COLOR     1

#include "fonts.h"

typedef struct {
    unsigned char *image;
    int width;
    int height;
    int rotate;
} epd_paint_t;

void epd_paint_init(epd_paint_t *epd, unsigned char *image, int width, int height);

void epd_paint_deinit(epd_paint_t *epd_paint);

void epd_paint_clear(epd_paint_t *epd_paint, int colored);

void epd_paint_draw_absolute_pixel(epd_paint_t *epd_paint, int x, int y, int colored);

void epd_paint_draw_pixel(epd_paint_t *epd_paint, int x, int y, int colored);

void epd_paint_draw_char_at(epd_paint_t *epd_paint, int x, int y, char ascii_char, sFONT *font, int colored);

void epd_paint_draw_string_at(epd_paint_t *epd_paint, int x, int y, const char *text, sFONT *font, int colored);

void epd_paint_draw_line(epd_paint_t *epd_paint, int x0, int y0, int x1, int y1, int colored);

void epd_paint_draw_horizontal_line(epd_paint_t *epd_paint, int x, int y, int width, int colored);

void epd_paint_draw_vertical_line(epd_paint_t *epd_paint, int x, int y, int height, int colored);

void epd_paint_draw_rectangle(epd_paint_t *epd_paint, int x0, int y0, int x1, int y1, int colored);

void epd_paint_draw_filled_rectangle(epd_paint_t *epd_paint, int x0, int y0, int x1, int y1, int colored);

void epd_paint_draw_circle(epd_paint_t *epd_paint, int x, int y, int radius, int colored);

void epd_paint_draw_filled_circle(epd_paint_t *epd_paint, int x, int y, int radius, int colored);

#endif