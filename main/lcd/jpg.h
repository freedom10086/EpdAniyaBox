#ifndef JPG_H
#define JPG_H

#include <stdio.h>
#include "epdpaint.h"

#define JPG_MAGIC 0xd8ff



enum jpg_err {
    JPG_NOT_SUPPORTED_FORMAT = -3,
    JPG_INVALID_FILE,
    JPG_ERROR,
    JPG_OK = 0
};

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t *pixel;
} jpg_t;

enum jpg_err jpg_header_read(jpg_t *header, uint8_t *data, uint16_t data_len);

enum jpg_err jpg_header_read_file(jpg_t *header, FILE *img_file);

enum jpg_err
jpg_file_get_pixel(pixel_color *out_color, jpg_t *jpg_img, uint16_t x, uint16_t y, FILE *img_file, uint16_t file_size);

void jpg_file_free(jpg_t *jpg_img);
#endif