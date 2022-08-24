#include "bmp.h"

#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"

// BMP_HEADER

void bmp_header_init(bmp_header *header,
                     uint16_t width,
                     uint16_t height,
                     uint16_t biBitCount) {
    // (sizeof(bmp_pixel_24) * width + BMP_GET_PADDING (width)) * abs(height);
    header->bfType = BMP_MAGIC;
    header->bfSize = ((((width * biBitCount) + 31) & ~31) >> 3) * abs(height);
    header->bfReserved1 = 0;
    header->bfReserved2 = 0;
    header->biSize = 40;
    header->biWidth = width;
    header->biHeight = height;
    header->biPlanes = 1;
    header->biBitCount = biBitCount;
    header->biCompression = BI_RGB;
    header->biSizeImage = 0;
    header->biXPelsPerMeter = 0;
    header->biYPelsPerMeter = 0;
    header->biClrUsed = 0;
    header->biClrImportant = 0;

    header->bfOffBits = 54;

    if (biBitCount == 1 || biBitCount == 4 || biBitCount == 8) {
        int lut_cnt = 1 << biBitCount;
        // 灰阶lut
        header->bfOffBits += sizeof(RGBQUAD) * lut_cnt;
    }
}

enum bmp_error bmp_header_write(const bmp_header *header, RGBQUAD *bmiColors, FILE *img_file) {
    if (header == NULL) {
        return BMP_HEADER_NOT_INITIALIZED;
    } else if (img_file == NULL) {
        return BMP_FILE_NOT_OPENED;
    }

    // Use the type instead of the variable because its a pointer!
    fwrite(header, sizeof(bmp_header), 1, img_file);

    if (bmiColors != NULL) {
        // uint16_t lut_cnt = (header->bfOffBits - 14 - header->biSize) / sizeof(RGBQUAD);
        fwrite(bmiColors, header->bfOffBits - 14 - header->biSize, 1, img_file);
    }

    return BMP_OK;
}

enum bmp_error bmp_header_read(bmp_header *header, uint8_t *data, uint16_t data_len) {
    if (data == NULL || data_len < sizeof(bmp_header)) {
        return BMP_INVALID_FILE;
    }
    unsigned short magic;
    bmp_header *bmpHeader = (bmp_header *) (data);
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
            int bmiColorsMaskCount = 1;
            assert(bmpHeader->bfOffBits - 14 - bmpHeader->biSize == sizeof(RGBQUAD_COLOR_MASK));
        }

        // check data size
        uint16_t should_be_size = bmpHeader->bfSize - bmpHeader->bfOffBits;
        uint16_t actual_size =
                ((((bmpHeader->biWidth * bmpHeader->biBitCount) + 31) & ~31) >> 3) * abs(bmpHeader->biHeight);

        if (should_be_size != actual_size) {
            return BMP_NOT_SUPPORTED_FORMAT;
        }

        *header = *bmpHeader;
    }
    return BMP_OK;
}

enum bmp_error bmp_header_read_file(bmp_header *header, FILE *img_file) {
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

    fseek(img_file, -1, SEEK_CUR);
    if (fread(header, sizeof(bmp_header), 1, img_file) != 1) {
        return BMP_ERROR;
    }

    return BMP_OK;
}

void bmp_set_pixel(bmp_pixel_color *color, uint8_t red, uint8_t green, uint8_t blue) {
    color->red = red;
    color->green = green;
    color->blue = blue;
}

void bmp_get_pixel(bmp_pixel_color *out_color, bmp_img_common *bmp_img, uint16_t x, uint16_t y) {
    // get pixel color

    const size_t offset = (bmp_img->img_header.biHeight > 0 ? bmp_img->img_header.biHeight - 1 : 0);
    const uint16_t y_index = abs(offset - y);

    if (bmp_img->img_header.biBitCount == 1 || bmp_img->img_header.biBitCount == 4
        || bmp_img->img_header.biBitCount == 8) {
        RGBQUAD *colors = NULL;
        uint16_t bmiColorsCount = 0;

        colors = (RGBQUAD *) (bmp_img->data);
        bmiColorsCount = 1 << bmp_img->img_header.biBitCount;

        uint16_t byte_index = ((((bmp_img->img_header.biWidth * bmp_img->img_header.biBitCount) + 31) & ~31) >> 3) * y_index
                              + ((x * bmp_img->img_header.biBitCount) >> 3);
        uint8_t *pixel_data = (uint8_t *) (bmp_img->data + bmiColorsCount * sizeof(RGBQUAD));
        uint8_t d = pixel_data[byte_index];
        if (bmp_img->img_header.biBitCount == 8) {
            out_color->blue = colors[d].blue;
            out_color->green = colors[d].green;
            out_color->red = colors[d].red;
        } else if (bmp_img->img_header.biBitCount == 4) {
            // 0  >> 4   1 not need
            uint8_t color_index = (d >> ((1 - x % 2) * bmp_img->img_header.biBitCount)) & 0x0f;
            out_color->blue = colors[color_index].blue;
            out_color->green = colors[color_index].green;
            out_color->red = colors[color_index].red;
        } else if (bmp_img->img_header.biBitCount == 1) {
            uint8_t color_index = (d >> (7 - x % 8)) & 0x01;

            out_color->blue = colors[color_index].blue;
            out_color->green = colors[color_index].green;
            out_color->red = colors[color_index].red;
        }
    } else if (bmp_img->img_header.biBitCount == 16 && bmp_img->img_header.biCompression == BI_RGB) {
        uint16_t *pixel_data = (uint16_t *) (bmp_img->data);
        uint16_t h_index = ((((bmp_img->img_header.biWidth * bmp_img->img_header.biBitCount) + 31) & ~31) >> 3) * y_index + x;
        // bgr 555
        out_color->blue = pixel_data[h_index] & 0b11111;
        out_color->green = (pixel_data[h_index] >> 5) & 0b11111;
        out_color->red = (pixel_data[h_index] >> 10) & 0b11111;
    } else if (bmp_img->img_header.biBitCount == 24) {
        bmp_pixel_24 *pixel_data = (bmp_pixel_24 *) (bmp_img->data);
        uint16_t h_index = ((((bmp_img->img_header.biWidth * bmp_img->img_header.biBitCount) + 31) & ~31) >> 3) * y_index + x;

        out_color->blue = pixel_data[h_index].blue;
        out_color->green = pixel_data[h_index].green;
        out_color->red = pixel_data[h_index].red;
    } else if (bmp_img->img_header.biBitCount == 32) {
        bmp_pixel_32 *pixel_data = (bmp_pixel_32 *) (bmp_img->data);
        uint16_t h_index = ((((bmp_img->img_header.biWidth * bmp_img->img_header.biBitCount) + 31) & ~31) >> 3) * y_index + x;

        out_color->blue = pixel_data[h_index].blue;
        out_color->green = pixel_data[h_index].green;
        out_color->red = pixel_data[h_index].red;
    } else {
        // not supported
        out_color->blue = 0x00;
        out_color->green = 0x00;
        out_color->red = 0x00;
    }
}


// BMP_IMG

void bmp_img_alloc(bmp_img_24 *img) {
    const size_t h = abs(img->img_header.biHeight);

    // Allocate the required memory for the pixels:
    img->img_pixels = malloc(sizeof(bmp_pixel_24 *) * h);

    for (size_t y = 0; y < h; y++) {
        img->img_pixels[y] = malloc(sizeof(bmp_pixel_24) * img->img_header.biWidth);
    }
}

void bmp_img_free(bmp_img_24 *img) {
    const size_t h = abs(img->img_header.biHeight);

    for (size_t y = 0; y < h; y++) {
        free(img->img_pixels[y]);
    }
    free(img->img_pixels);
}

enum bmp_error bmp_img_write(const bmp_img_24 *img,
                             const char *filename) {
    FILE *img_file = fopen(filename, "wb");

    if (img_file == NULL) {
        return BMP_FILE_NOT_OPENED;
    }

    // NOTE: This way the correct error code could be returned.
    const enum bmp_error err = bmp_header_write(&img->img_header, NULL, img_file);

    if (err != BMP_OK) {
        // ERROR: Could'nt write the header!
        fclose(img_file);
        return err;
    }

    // Select the mode (bottom-up or top-down):
    const size_t h = abs(img->img_header.biHeight);
    const size_t offset = (img->img_header.biHeight > 0 ? h - 1 : 0);

    // Create the padding:
    const unsigned char padding[3] = {'\0', '\0', '\0'};

    // Write the content:
    for (size_t y = 0; y < h; y++) {
        // Write a whole row of pixels to the file:
        fwrite(img->img_pixels[abs(offset - y)], sizeof(bmp_pixel_24), img->img_header.biWidth, img_file);

        // Write the padding for the row!
        fwrite(padding, sizeof(unsigned char), BMP_GET_PADDING (img->img_header.biWidth), img_file);
    }

    // NOTE: All good!
    fclose(img_file);
    return BMP_OK;
}

enum bmp_error bmp_img_read(bmp_img_24 *img, const char *filename) {
    FILE *img_file = fopen(filename, "rb");

    if (img_file == NULL) {
        return BMP_FILE_NOT_OPENED;
    }

    // NOTE: This way the correct error code can be returned.
    const enum bmp_error err = bmp_header_read_file(&img->img_header, img_file);

    if (err != BMP_OK) {
        // ERROR: Could'nt read the image header!
        fclose(img_file);
        return err;
    }

    // load lut
    uint16_t lut_byte = (img->img_header.bfOffBits - 14 - img->img_header.biSize);
    // lut_byte = sizeof(RGBQUAD), lut_byte / sizeof(RGBQUAD)
    if (lut_byte > 0) {
        if (lut_byte % sizeof(RGBQUAD) != 0) {
            return BMP_INVALID_LUT_SIZE;
        }

        uint16_t lut_count = lut_byte / sizeof(RGBQUAD);
        RGBQUAD *lut = malloc(sizeof(RGBQUAD) * lut_count);
        fread(lut, sizeof(RGBQUAD), lut_count, img_file);
    }

    bmp_img_alloc(img);

    // Select the mode (bottom-up or top-down):
    const size_t h = abs(img->img_header.biHeight);
    const size_t offset = (img->img_header.biHeight > 0 ? h - 1 : 0);
    const size_t padding = BMP_GET_PADDING (img->img_header.biWidth);

    // Needed to compare the return value of fread
    const size_t items = img->img_header.biWidth;

    // Read the content:
    for (size_t y = 0; y < h; y++) {
        // Read a whole row of pixels from the file:
        if (fread(img->img_pixels[abs(offset - y)], sizeof(bmp_pixel_24), items, img_file) != items) {
            fclose(img_file);
            return BMP_ERROR;
        }

        // Skip the padding:
        fseek(img_file, padding, SEEK_CUR);
    }

    // NOTE: All good!
    fclose(img_file);
    return BMP_OK;
}