#include "stdio.h"
#include "esp_log.h"
#include "stdlib.h"
#include <dirent.h>
#include<sys/stat.h>

#include "jpeg_decoder.h"
#include "jpg.h"

#define TAG "jpg"

enum jpg_err jpg_header_read(jpg_t *header, uint8_t *data, uint16_t data_len) {
    uint16_t magic = ((uint16_t *) data)[0];
    if (magic != JPG_MAGIC) {
        return JPG_INVALID_FILE;
    } else {
        uint16_t tag = 0;
        uint16_t index = 2;

        tag = ((uint16_t *) (data + index))[0];
        while (tag != 0xc0ff && (tag >> 8) == 0xff) {
            index += 2;
            uint16_t size = ((uint16_t *) (data + index))[0];
            index += (size - 2);
            // read size to skip
            tag = ((uint16_t *) (data + index))[0];
        }
        // find c0ff
        if (tag == 0xc0ff) {
            index += 2; // 字段大小 每个数据的位数，一般为8位
            index += 1; // 精度
            uint16_t height = ((uint16_t *) (data + index))[0];
            index += 2;
            uint16_t width = ((uint16_t *) (data + index))[0];

            header->height = height;
            header->width = width;

            ESP_LOGI(TAG, "get jpg file height %d, width: %d", header->height, header->width);
        }
    }
    return JPG_OK;
}

enum jpg_err jpg_header_read_file(jpg_t *header, FILE *img_file) {
    if (img_file == NULL) {
        return JPG_INVALID_FILE;
    }

    // Since an adress must be passed to fread, create a variable!
    unsigned short magic;

    // Check if its an bmp file by comparing the magic nbr:
    if (fread(&magic, sizeof(magic), 1, img_file) != 1 ||
        magic != JPG_MAGIC) {
        ESP_LOGW(TAG, "not jpg file %x", magic);
        return JPG_INVALID_FILE;
    }

    uint16_t tag = 0;
    uint16_t section_size = 0;

    while (fread(&tag, sizeof(uint16_t), 1, img_file) == 1) {
        if (tag == 0xc0ff || (tag & 0xff) != 0xff) {
            break;
        }
        //ESP_LOGI(TAG, "current tag %x", tag);
        fread(&section_size, sizeof(uint16_t), 1, img_file);
        section_size = (section_size >> 8) | (section_size << 8);
        //ESP_LOGI(TAG, "section size %x", section_size);
        fseek(img_file, section_size - 2, SEEK_CUR);
    }

    // find c0ff
    if (tag == 0xc0ff) {
        fseek(img_file, 3, SEEK_CUR);

        fread(&header->height, sizeof(uint16_t), 1, img_file);
        fread(&header->width, sizeof(uint16_t), 1, img_file);

        header->height = header->height >> 8 | header->height << 8;
        header->width = header->width >> 8 | header->width << 8;

        ESP_LOGI(TAG, "get jpg file height %d, width: %d", header->height, header->width);

        return JPG_OK;
    }

    ESP_LOGW(TAG, "no size tag find %x", tag);

    return JPG_ERROR;
}

enum jpg_err
jpg_file_get_pixel(pixel_color *out_color, jpg_t *jpg_img, uint16_t x, uint16_t y, FILE *img_file, uint16_t file_size) {
    if (jpg_img->pixel == NULL) {
        //ESP_LOGI(TAG, "read jpg file to buff");
        uint8_t *indata = calloc(file_size, sizeof(uint8_t));
        if (indata == NULL) {
            ESP_LOGE(TAG, "Error allocating memory for file");
            return JPG_ERROR;
        }
        fseek(img_file, 0, SEEK_SET);
        fread(indata, sizeof(uint8_t), file_size, img_file);

        uint16_t *pixels = calloc(jpg_img->height * jpg_img->width, sizeof(uint16_t));
        if (pixels == NULL) {
            ESP_LOGE(TAG, "Error allocating memory for pixel");

            free(indata);
            return JPG_ERROR;
        }

        // JPEG decode config
        esp_jpeg_image_cfg_t jpeg_cfg = {
                .indata = indata,
                .indata_size = file_size,
                .outbuf = (uint8_t *) pixels,
                .outbuf_size = jpg_img->width * jpg_img->height * sizeof(uint16_t),
                .out_format = JPEG_IMAGE_FORMAT_RGB565,
                .out_scale = JPEG_IMAGE_SCALE_0,
                .flags = {
                        .swap_color_bytes = 1,
                }
        };

        //JPEG decode
        esp_jpeg_image_output_t outimg;
        esp_err_t error = esp_jpeg_decode(&jpeg_cfg, &outimg);

        free(indata);

        if (error != ESP_OK) {
            free(pixels);
            return JPG_ERROR;
        }

        jpg_img->pixel = (uint8_t *) pixels;
        ESP_LOGI(TAG, "JPEG image decoded! Size of the decoded image is: %dpx x %dpx", outimg.width, outimg.height);
    }

    // JPEG_IMAGE_FORMAT_RGB565
    uint16_t pixel = ((uint16_t *)jpg_img->pixel)[y * jpg_img->width + x];
    out_color->blue = (pixel & 0b11111) << 3;
    out_color->green = ((pixel >> 5) & 0b111111) << 2;
    out_color->red = ((pixel >> 11) & 0b11111) << 3;

    return JPG_OK;
}

void jpg_file_free(jpg_t *jpg_img) {
    if (jpg_img->pixel != NULL) {
        free(jpg_img->pixel);
        jpg_img->pixel = NULL;
    }
}