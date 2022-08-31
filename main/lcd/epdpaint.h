#ifndef EPDPAINT_H
#define EPDPAINT_H

// Display orientation
#define ROTATE_0            0
#define ROTATE_90           1
#define ROTATE_180          2
#define ROTATE_270          3

// Color inverse. 1 or 0 = set or reset a bit if set a colored pixel
#define IF_INVERT_COLOR     0

#include <stdio.h>
#include "fonts.h"

typedef struct {
    unsigned char *image;
    int width;
    int height;
    int rotate;
} epd_paint_t;

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} pixel_color;

void epd_paint_init(epd_paint_t *epd, unsigned char *image, int width, int height);

void epd_paint_deinit(epd_paint_t *epd_paint);

void epd_paint_clear(epd_paint_t *epd_paint, int colored);

void epd_paint_clear_range(epd_paint_t *epd_paint, int start_x, int start_y, int width, int height, int colored);

void epd_paint_reverse_range(epd_paint_t *epd_paint, int start_x, int start_y, int width, int height);

void epd_paint_draw_absolute_pixel(epd_paint_t *epd_paint, int x, int y, int colored);

void epd_paint_draw_pixel(epd_paint_t *epd_paint, int x, int y, int colored);

void epd_paint_draw_char_at(epd_paint_t *epd_paint, int x, int y, char ascii_char, sFONT *font, int colored);

void epd_paint_draw_chinese_char_at(epd_paint_t *epd_paint, int x, int y, uint16_t font_char, sFONT *font, int colored);

void epd_paint_draw_string_at(epd_paint_t *epd_paint, int x, int y, const char *text, sFONT *font, int colored);

void epd_paint_draw_line(epd_paint_t *epd_paint, int x0, int y0, int x1, int y1, int colored);

void epd_paint_draw_horizontal_line(epd_paint_t *epd_paint, int x, int y, int width, int colored);

void epd_paint_draw_vertical_line(epd_paint_t *epd_paint, int x, int y, int height, int colored);

void epd_paint_draw_rectangle(epd_paint_t *epd_paint, int x0, int y0, int x1, int y1, int colored);

void epd_paint_draw_filled_rectangle(epd_paint_t *epd_paint, int x0, int y0, int x1, int y1, int colored);

void epd_paint_draw_circle(epd_paint_t *epd_paint, int x, int y, int radius, int colored);

void epd_paint_draw_filled_circle(epd_paint_t *epd_paint, int x, int y, int radius, int colored);

void epd_paint_draw_bitmap(epd_paint_t *epd_paint, int x, int y, int width, int height, uint8_t *bmp_data,
                           uint16_t data_size, int colored);

void epd_paint_draw_bitmap_file(epd_paint_t *epd_paint, int x, int y, int width, int height, FILE *file, int colored);

void
epd_paint_draw_jpg_file(epd_paint_t *epd_paint, int x, int y, int width, int height, FILE *file, uint16_t file_size,
                        int colored);

#endif