#include "bmp.h"

#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "bike_common.h"

#define TAG "bmp"

enum bmp_error bmp_header_read(bmp_header *header, uint8_t *data, uint16_t data_len) {
    if (data == NULL || data_len < sizeof(bmp_header)) {
        return BMP_INVALID_FILE;
    }
    bmp_header *bmpHeader = (bmp_header *) (data);
    *header = *bmpHeader;

    if (bmpHeader->bfType != BMP_MAGIC) {
        return BMP_INVALID_FILE;
    } else {
        if (bmpHeader->biBitCount != 1 && bmpHeader->biBitCount != 4
            && bmpHeader->biBitCount != 8 && bmpHeader->biBitCount != 16
            && bmpHeader->biBitCount != 24 && bmpHeader->biBitCount != 32) {
            return BMP_NOT_SUPPORTED_FORMAT;
        }

        if (bmpHeader->biCompression != BI_RGB) {
            if ((bmpHeader->biBitCount != 16 && bmpHeader->biBitCount != 32) &&
                bmpHeader->biCompression == BI_BITFIELDS) {
                return BMP_NOT_SUPPORTED_FORMAT;
            }
            if (bmpHeader->biCompression != BI_BITFIELDS) {
                return BMP_NOT_SUPPORTED_FORMAT;
            }
        }

        if (bmpHeader->biBitCount == 1 || bmpHeader->biBitCount == 4 || bmpHeader->biBitCount == 8) {
            int bmiColorsCount = (bmpHeader->bfOffBits - 14 - bmpHeader->biSize) / sizeof(RGBQUAD);
            assert(bmiColorsCount == 1 << bmpHeader->biBitCount);
        } else if (bmpHeader->biCompression == BI_BITFIELDS) {
            assert(bmpHeader->bfOffBits - 14 - bmpHeader->biSize == sizeof(RGBQUAD_COLOR_MASK));
        }

        // check data size
        uint16_t head_config_size = bmpHeader->bfSize - bmpHeader->bfOffBits;
        uint16_t min_size =
                ((((bmpHeader->biWidth * bmpHeader->biBitCount) + 31) & ~31) >> 3) * abs(bmpHeader->biHeight);
        uint16_t config_size = bmpHeader->biSizeImage; // 字节

        if (head_config_size != config_size || config_size < min_size) {
            return BMP_NOT_SUPPORTED_FORMAT;
        }
    }
    return BMP_OK;
}

enum bmp_error
bmp_get_pixel_from_line(pixel_color *out_color, bmp_header *img_header, RGBQUAD *colors, uint8_t *data, uint16_t x,
                        uint16_t y) {
    if (img_header->biBitCount == 1 || img_header->biBitCount == 4
        || img_header->biBitCount == 8) {
        uint32_t byte_index = ((x * img_header->biBitCount) >> 3);

        uint8_t *pixel_data = (uint8_t *) (data);
        uint8_t d = pixel_data[byte_index];
        if (img_header->biBitCount == 8) {
            out_color->blue = colors[d].blue;
            out_color->green = colors[d].green;
            out_color->red = colors[d].red;
        } else if (img_header->biBitCount == 4) {
            // 0  >> 4   1 not need
            uint8_t color_index = (d >> ((1 - x % 2) * img_header->biBitCount)) & 0x0f;
            out_color->blue = colors[color_index].blue;
            out_color->green = colors[color_index].green;
            out_color->red = colors[color_index].red;
        } else if (img_header->biBitCount == 1) {
            uint8_t color_index = (d >> (7 - x % 8)) & 0x01;

            out_color->blue = colors[color_index].blue;
            out_color->green = colors[color_index].green;
            out_color->red = colors[color_index].red;
        }
    } else if (img_header->biBitCount == 16 && img_header->biCompression == BI_RGB) {
        uint16_t *pixel_data = (uint16_t *) (data);
        // bgr 555
        out_color->blue = pixel_data[x] & 0b11111;
        out_color->green = (pixel_data[x] >> 5) & 0b11111;
        out_color->red = (pixel_data[x] >> 10) & 0b11111;
    } else if (img_header->biBitCount == 24) {
        bmp_pixel_24 *pixel_data = (bmp_pixel_24 *) (data);
        out_color->blue = pixel_data[x].blue;
        out_color->green = pixel_data[x].green;
        out_color->red = pixel_data[x].red;
    } else if (img_header->biBitCount == 32) {
        bmp_pixel_32 *pixel_data = (bmp_pixel_32 *) (data);
        out_color->blue = pixel_data[x].blue;
        out_color->green = pixel_data[x].green;
        out_color->red = pixel_data[x].red;
    } else {
        return BMP_ERROR;
    }

    return BMP_OK;
}

void bmp_get_pixel(pixel_color *out_color, bmp_img_common *bmp_img, uint16_t x, uint16_t y) {
    // get pixel color
    const size_t offset = (bmp_img->img_header.biHeight > 0 ? bmp_img->img_header.biHeight - 1 : 0);
    const uint16_t y_index = abs(offset - y);
    uint32_t line_start_index =
            ((((bmp_img->img_header.biWidth * bmp_img->img_header.biBitCount) + 31) & ~31) >> 3) * y_index;

    bmp_get_pixel_from_line(out_color, &bmp_img->img_header,
                            (RGBQUAD *) (bmp_img->data),
                            bmp_img->data + (bmp_img->img_header.bfOffBits - sizeof(bmp_header) + line_start_index),
                            x, y);
}

enum bmp_error bmp_header_read_file(bmp_img_file_common *bmp_img, FILE *img_file) {
    if (img_file == NULL) {
        return BMP_FILE_NOT_OPENED;
    }

    // Since an adress must be passed to fread, create a variable!
    unsigned short magic;

    // Check if its an bmp file by comparing the magic nbr:
    if (fread(&magic, sizeof(magic), 1, img_file) != 1 ||
        magic != BMP_MAGIC) {
        return BMP_INVALID_FILE;
    }

    fseek(img_file, 0, SEEK_SET);
    uint8_t buff[sizeof(bmp_header)];

    if (fread(buff, sizeof(bmp_header), 1, img_file) != 1) {
        return BMP_ERROR;
    }

    enum bmp_error err = bmp_header_read(&bmp_img->img_header, buff, sizeof(bmp_header));
    if (err != BMP_OK) {
        return err;
    }

    // init
    bmp_img->color_data = NULL;
    bmp_img->data_buff_size = 0;
    bmp_img->data_buff = NULL;
    bmp_img->data_start_y = 0;
    bmp_img->data_end_y = 0;

    // freed next lut or mask
    if (bmp_img->img_header.bfOffBits - sizeof(bmp_header)) {
        bmp_img->color_data = malloc(bmp_img->img_header.bfOffBits - sizeof(bmp_header));
        if (fread(bmp_img->color_data, bmp_img->img_header.bfOffBits - sizeof(bmp_header), 1, img_file) != 1) {
            return BMP_ERROR;
        }
    }

    return BMP_OK;
}

int bmp_read_file_lines(bmp_img_file_common *bmp_img, uint16_t start_y, uint16_t lines, FILE *img_file) {
    uint16_t pad_line_byte = ((bmp_img->img_header.biWidth * bmp_img->img_header.biBitCount + 31) & ~31) >> 3;
    bmp_img->data_start_y = start_y;
    bmp_img->data_end_y = start_y;

    uint32_t start_byte_index =
            (bmp_img->img_header.biHeight < 0 ? start_y : bmp_img->img_header.biHeight - lines - start_y) *
            pad_line_byte;
    fseek(img_file, start_byte_index + bmp_img->img_header.bfOffBits, SEEK_SET);
    size_t read_count = fread(bmp_img->data_buff, sizeof(uint8_t), pad_line_byte * lines, img_file);
    if (read_count > 0) {
        bmp_img->data_end_y += (read_count + pad_line_byte - 1) / pad_line_byte;
        return read_count;
    } else if (feof(img_file)) {
        return read_count;
    } else {
        return BMP_ERROR;
    }
}

enum bmp_error
bmp_file_get_pixel(pixel_color *out_color, bmp_img_file_common *bmp_img, uint16_t x, uint16_t y, FILE *img_file) {
    // data not in buff
    uint16_t pad_line_byte = ((bmp_img->img_header.biWidth * bmp_img->img_header.biBitCount + 31) & ~31) >> 3;
    if (y < bmp_img->data_start_y || y >= bmp_img->data_end_y) {
        // read from buff
        if (bmp_img->data_buff == NULL) {
            uint16_t buff_size = max(10240 / pad_line_byte * pad_line_byte, pad_line_byte);
            bmp_img->data_buff = malloc(buff_size);
            if (bmp_img->data_buff == NULL) {
                // no memory
                return BMP_ERROR;
            }

            bmp_img->data_buff_size = buff_size;
        }

        uint32_t start_tck = xTaskGetTickCount();
        uint16_t lines = min(abs(bmp_img->img_header.biHeight) - y, bmp_img->data_buff_size / pad_line_byte);

        int read_bytes = bmp_read_file_lines(bmp_img, y, lines, img_file);
        ESP_LOGI(TAG, "read lines from %d to %d buff_size:%d, read byte:%d cost %ldms",
                 y, y + lines, bmp_img->data_buff_size, read_bytes, pdTICKS_TO_MS(xTaskGetTickCount() - start_tck));
    }

    uint32_t line_start_index =
            (bmp_img->img_header.biHeight < 0 ? (y - bmp_img->data_start_y) : (bmp_img->data_end_y - y - 1)) *
            pad_line_byte;
    bmp_get_pixel_from_line(out_color, &bmp_img->img_header, (RGBQUAD *) bmp_img->color_data,
                            bmp_img->data_buff + line_start_index, x, y);

    return BMP_OK;
}

void bmp_file_free(bmp_img_file_common *bmp_img) {
    if (bmp_img->data_buff != NULL) {
        free(bmp_img->data_buff);
        bmp_img->data_buff = NULL;
        bmp_img->data_buff_size = 0;
    }

    if (bmp_img->color_data != NULL) {
        free(bmp_img->color_data);
        bmp_img->color_data = NULL;
    }
}