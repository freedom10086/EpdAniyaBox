#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "epdpaint.h"
#include "bmp.h"
#include "bike_common.h"

void epd_paint_init(epd_paint_t *epd_paint, unsigned char *image, int width, int height) {
    epd_paint->rotate = ROTATE_0;
    epd_paint->image = image;
    /* 1 byte = 8 pixels, so the width should be the multiple of 8 */
    epd_paint->width = width % 8 ? width + 8 - (width % 8) : width;
    epd_paint->height = height;
}

void epd_paint_deinit(epd_paint_t *epd_paint) {
    free(epd_paint->image);
    epd_paint->image = NULL;
}

/**
 *  @brief: clear the image
 */
void epd_paint_clear(epd_paint_t *epd_paint, int colored) {
    for (int x = 0; x < epd_paint->width; x++) {
        for (int y = 0; y < epd_paint->height; y++) {
            epd_paint_draw_absolute_pixel(epd_paint, x, y, colored);
        }
    }
}

void epd_paint_clear_range(epd_paint_t *epd_paint, int start_x, int start_y, int width, int height, int colored) {
    for (int x = start_x; x < min(start_x + width, epd_paint->width); x++) {
        for (int y = start_y; y < min(start_y + height, epd_paint->height); y++) {
            epd_paint_draw_absolute_pixel(epd_paint, x, y, colored);
        }
    }
}

void epd_paint_reverse_range(epd_paint_t *epd_paint, int start_x, int start_y, int width, int height) {
    for (int x = start_x; x < min(start_x + width, epd_paint->width); x++) {
        for (int y = start_y; y < min(start_y + height, epd_paint->height); y++) {
            if (epd_paint->image[(x + y * epd_paint->width) / 8] & (0x80 >> (x % 8))) { // bit is 1
                epd_paint_draw_absolute_pixel(epd_paint, x, y, 1);
            } else {
                epd_paint_draw_absolute_pixel(epd_paint, x, y, 0);
            }
        }
    }
}

/**
 *  @brief: draws a pixel by absolute coordinates.
 *          this function won't be affected by the rotate parameter.
 */
void epd_paint_draw_absolute_pixel(epd_paint_t *epd_paint, int x, int y, int colored) {
    if (x < 0 || x >= epd_paint->width || y < 0 || y >= epd_paint->height) {
        return;
    }
    if (IF_INVERT_COLOR) {
        if (colored) {
            epd_paint->image[(x + y * epd_paint->width) / 8] |= 0x80 >> (x % 8);
        } else {
            epd_paint->image[(x + y * epd_paint->width) / 8] &= ~(0x80 >> (x % 8));
        }
    } else {
        if (colored) {
            epd_paint->image[(x + y * epd_paint->width) / 8] &= ~(0x80 >> (x % 8));
        } else {
            epd_paint->image[(x + y * epd_paint->width) / 8] |= 0x80 >> (x % 8);
        }
    }
}

/**
 *  @brief: epd_paint draws a pixel by the coordinates
 */
void epd_paint_draw_pixel(epd_paint_t *epd_paint, int x, int y, int colored) {
    int point_temp;
    if (epd_paint->rotate == ROTATE_0) {
        if (x < 0 || x >= epd_paint->width || y < 0 || y >= epd_paint->height) {
            return;
        }
        epd_paint_draw_absolute_pixel(epd_paint, x, y, colored);
    } else if (epd_paint->rotate == ROTATE_90) {
        if (x < 0 || x >= epd_paint->height || y < 0 || y >= epd_paint->width) {
            return;
        }
        point_temp = x;
        x = epd_paint->width - y;
        y = point_temp;
        epd_paint_draw_absolute_pixel(epd_paint, x, y, colored);
    } else if (epd_paint->rotate == ROTATE_180) {
        if (x < 0 || x >= epd_paint->width || y < 0 || y >= epd_paint->height) {
            return;
        }
        x = epd_paint->width - x;
        y = epd_paint->height - y;
        epd_paint_draw_absolute_pixel(epd_paint, x, y, colored);
    } else if (epd_paint->rotate == ROTATE_270) {
        if (x < 0 || x >= epd_paint->height || y < 0 || y >= epd_paint->width) {
            return;
        }
        point_temp = x;
        x = y;
        y = epd_paint->height - point_temp;
        epd_paint_draw_absolute_pixel(epd_paint, x, y, colored);
    }
}

/**
 *  @brief: draws a charactor on the frame buffer but not refresh
 */
void epd_paint_draw_char_at(epd_paint_t *epd_paint, int x, int y, char ascii_char, sFONT *font, int colored) {
    int i, j;
    unsigned int char_offset =
            (ascii_char - font->start) * font->Height * (font->Width / 8 + (font->Width % 8 ? 1 : 0));
    const uint8_t *ptr = &font->table[char_offset];

    for (j = 0; j < font->Height; j++) {
        for (i = 0; i < font->Width; i++) {
            if (*ptr & (0x80 >> (i % 8))) {
                epd_paint_draw_pixel(epd_paint, x + i, y + j, colored);
            }
            if (i % 8 == 7) {
                ptr++;
            }
        }
        if (font->Width % 8 != 0) {
            ptr++;
        }
    }
}

// 0xA1A1~0xFEFE
void
epd_paint_draw_chinese_char_at(epd_paint_t *epd_paint, int x, int y, uint16_t font_char, sFONT *font, int colored) {
    int i, j;
    unsigned int char_offset =
            (94 * (unsigned int) ((font_char & 0xff) - 0xa0 - 1) + ((font_char >> 8) - 0xa0 - 1))
            * font->Height * (font->Width / 8 + (font->Width % 8 ? 1 : 0));
    const uint8_t *ptr = &font->table[char_offset];

    for (j = 0; j < font->Height; j++) {
        for (i = 0; i < font->Width; i++) {
            if (*ptr & (0x80 >> (i % 8))) {
                epd_paint_draw_pixel(epd_paint, x + i, y + j, colored);
            }
            if (i % 8 == 7) {
                ptr++;
            }
        }
        if (font->Width % 8 != 0) {
            ptr++;
        }
    }
}


/**
*  @brief: epd_paint displays a string on the frame buffer but not refresh
*/
void epd_paint_draw_string_at(epd_paint_t *epd_paint, int x, int y, const char *text, sFONT *font, int colored) {
    const uint8_t *p_text = (uint8_t *) text;
    uint16_t chinese_text;
    unsigned int counter = 0;
    int refcolumn = x;

    while (*p_text != 0) {
        if (font->is_chinese) {
            if (*p_text < 128) {
                epd_paint_draw_char_at(epd_paint, refcolumn, y, (char) *p_text, &Font16_2, colored);
                p_text++;
                refcolumn += Font16_2.Width;
            } else {
                chinese_text = ((*p_text) | *(p_text + 1) << 8);
                epd_paint_draw_chinese_char_at(epd_paint, refcolumn, y, chinese_text, font, colored);
                p_text += 2;
                refcolumn += font->Width;
            }
        } else {
            epd_paint_draw_char_at(epd_paint, refcolumn, y, (char) *p_text, font, colored);
            p_text++;
            refcolumn += font->Width;
        }
        counter++;
    }
}

/**
*  @brief: draws a line on the frame buffer
*/
void epd_paint_draw_line(epd_paint_t *epd_paint, int x0, int y0, int x1, int y1, int colored) {
    /* Bresenham algorithm */
    int dx = x1 - x0 >= 0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 - y0 <= 0 ? y1 - y0 : y0 - y1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while ((x0 != x1) && (y0 != y1)) {
        epd_paint_draw_pixel(epd_paint, x0, y0, colored);
        if (2 * err >= dy) {
            err += dy;
            x0 += sx;
        }
        if (2 * err <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/**
*  @brief: epd_paint draws a horizontal line on the frame buffer
*/
void epd_paint_draw_horizontal_line(epd_paint_t *epd_paint, int x, int y, int line_width, int colored) {
    int i;
    for (i = x; i < x + line_width; i++) {
        epd_paint_draw_pixel(epd_paint, i, y, colored);
    }
}

/**
*  @brief: draws a vertical line on the frame buffer
*/
void epd_paint_draw_vertical_line(epd_paint_t *epd_paint, int x, int y, int line_height, int colored) {
    int i;
    for (i = y; i < y + line_height; i++) {
        epd_paint_draw_pixel(epd_paint, x, i, colored);
    }
}


void epd_paint_draw_rectangle(epd_paint_t *epd_paint, int x0, int y0, int x1, int y1, int colored) {
    int min_x, min_y, max_x, max_y;
    min_x = x1 > x0 ? x0 : x1;
    max_x = x1 > x0 ? x1 : x0;
    min_y = y1 > y0 ? y0 : y1;
    max_y = y1 > y0 ? y1 : y0;

    epd_paint_draw_horizontal_line(epd_paint, min_x, min_y, max_x - min_x + 1, colored);
    epd_paint_draw_horizontal_line(epd_paint, min_x, max_y, max_x - min_x + 1, colored);
    epd_paint_draw_vertical_line(epd_paint, min_x, min_y, max_y - min_y + 1, colored);
    epd_paint_draw_vertical_line(epd_paint, max_x, min_y, max_y - min_y + 1, colored);
}

/**
*  @brief: epd_paint draws a filled rectangle
*/
void epd_paint_draw_filled_rectangle(epd_paint_t *epd_paint, int x0, int y0, int x1, int y1, int colored) {
    int min_x, min_y, max_x, max_y;
    int i;
    min_x = x1 > x0 ? x0 : x1;
    max_x = x1 > x0 ? x1 : x0;
    min_y = y1 > y0 ? y0 : y1;
    max_y = y1 > y0 ? y1 : y0;

    for (i = min_x; i <= max_x; i++) {
        epd_paint_draw_vertical_line(epd_paint, i, min_y, max_y - min_y + 1, colored);
    }
}

void epd_paint_draw_circle(epd_paint_t *epd_paint, int x, int y, int radius, int colored) {
    /* Bresenham algorithm */
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;

    do {
        epd_paint_draw_pixel(epd_paint, x - x_pos, y + y_pos, colored);
        epd_paint_draw_pixel(epd_paint, x + x_pos, y + y_pos, colored);
        epd_paint_draw_pixel(epd_paint, x + x_pos, y - y_pos, colored);
        epd_paint_draw_pixel(epd_paint, x - x_pos, y - y_pos, colored);
        e2 = err;
        if (e2 <= y_pos) {
            err += ++y_pos * 2 + 1;
            if (-x_pos == y_pos && e2 <= x_pos) {
                e2 = 0;
            }
        }
        if (e2 > x_pos) {
            err += ++x_pos * 2 + 1;
        }
    } while (x_pos <= 0);
}

/**
*  @brief: draws a filled circle
*/
void epd_paint_draw_filled_circle(epd_paint_t *epd_paint, int x, int y, int radius, int colored) {
    /* Bresenham algorithm */
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;

    do {
        epd_paint_draw_pixel(epd_paint, x - x_pos, y + y_pos, colored);
        epd_paint_draw_pixel(epd_paint, x + x_pos, y + y_pos, colored);
        epd_paint_draw_pixel(epd_paint, x + x_pos, y - y_pos, colored);
        epd_paint_draw_pixel(epd_paint, x - x_pos, y - y_pos, colored);
        epd_paint_draw_horizontal_line(epd_paint, x + x_pos, y + y_pos, 2 * (-x_pos) + 1, colored);
        epd_paint_draw_horizontal_line(epd_paint, x + x_pos, y - y_pos, 2 * (-x_pos) + 1, colored);
        e2 = err;
        if (e2 <= y_pos) {
            err += ++y_pos * 2 + 1;
            if (-x_pos == y_pos && e2 <= x_pos) {
                e2 = 0;
            }
        }
        if (e2 > x_pos) {
            err += ++x_pos * 2 + 1;
        }
    } while (x_pos <= 0);
}

void epd_paint_draw_bitmap(epd_paint_t *epd_paint, int x, int y, int width, int height, uint8_t *bmp_data,
                           uint16_t data_size, int colored) {
    bmp_header bmpHeader;
    enum bmp_error err = bmp_header_read(&bmpHeader, bmp_data, data_size);
    if (err != BMP_OK) {
        // not valid bmp pic just draw rec
        epd_paint_draw_rectangle(epd_paint, x, y, x + width - 1, y + height - 1, colored);
        epd_paint_draw_line(epd_paint, x, y, x + width, y + height, colored);
        epd_paint_draw_line(epd_paint, x, y + height, x + width, y, colored);
    } else {
        bmp_img_common *bmp_img = (bmp_img_common *) bmp_data;
        bmp_pixel_color out_color;
        uint8_t gray_color;

        uint16_t end_x = min(x + width, epd_paint->width);
        end_x = min(end_x, bmpHeader.biWidth);

        uint16_t end_y = min(y + height, epd_paint->height);
        end_y = min(end_y, abs(bmpHeader.biWidth));

        for (int j = y; j < end_y; ++j) {
            for (int i = x; i < end_x; ++i) {
                bmp_get_pixel(&out_color, bmp_img, (i - x), (j - y));
                // rgb 30% 50% 20%
                gray_color = (out_color.red * 3 + out_color.green * 5 + out_color.blue * 2) / 10;
                if (!colored) {
                    gray_color = 255 - gray_color;
                }
                epd_paint_draw_absolute_pixel(epd_paint, i, j, gray_color >= 132 ? 0 : 1);
            }
        }
    }
}

void epd_paint_draw_bitmap_file(epd_paint_t *epd_paint, int x, int y, int width, int height, FILE *file, int colored) {
    bmp_img_file_common bmpHeader;
    enum bmp_error err = bmp_header_read_file(&bmpHeader, file);
    if (err != BMP_OK) {
        // not valid bmp pic just draw rec
        epd_paint_draw_rectangle(epd_paint, x, y, x + width - 1, y + height - 1, colored);
        epd_paint_draw_line(epd_paint, x, y, x + width, y + height, colored);
        epd_paint_draw_line(epd_paint, x, y + height, x + width, y, colored);
    } else {
        bmp_pixel_color out_color;
        uint8_t gray_color;

        uint16_t end_x = min(x + width, epd_paint->width);
        end_x = min(end_x, bmpHeader.img_header.biWidth);

        uint16_t end_y = min(y + height, epd_paint->height);
        end_y = min(end_y, abs(bmpHeader.img_header.biWidth));

        for (int j = y; j < end_y; ++j) {
            for (int i = x; i < end_x; ++i) {
                bmp_file_get_pixel(&out_color, &bmpHeader, (i - x), (j - y), file);
                // rgb 30% 50% 20%
                gray_color = (out_color.red * 3 + out_color.green * 5 + out_color.blue * 2) / 10;
                if (!colored) {
                    gray_color = 255 - gray_color;
                }
                epd_paint_draw_absolute_pixel(epd_paint, i, j, gray_color >= 132 ? 0 : 1);
            }
        }
    }

    bmp_file_free(&bmpHeader);
}

/* END OF FILE */























