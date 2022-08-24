#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <string.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include <math.h>

#include "bike_common.h"
#include "test_page.h"
#include "tools/encode.h"
#include "epdpaint.h"
#include "key.h"

#include "bitmap_page.h"
#include "display.h"
#include "wifi/wifi_ap.h"
#include "epd_lcd_ssd1680.h"
#include "static/static.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "bitmap-page"

static uint8_t current_bitmap_page_index = 0;

void testBmp() {
    //bmp_header *bmpHeader = (bmp_header *) (aniya_200_1_bmp_start);
    bmp_header bmpHeader1;
    bmp_header_read(&bmpHeader1, (uint8_t *) aniya_200_1_bmp_start, aniya_200_1_bmp_end - aniya_200_1_bmp_start);
    bmp_header *bmpHeader = &bmpHeader1;
    if (bmpHeader->bfType != BMP_MAGIC) {
        ESP_LOGE(TAG, "not a bmp file %X %X", ((uint16_t *) aniya_200_1_bmp_start)[0], bmpHeader->bfType);
        return;
    } else {
        ESP_LOGI(TAG,
                 "bfType = 0x%X, bfSize = %ld, bfReserved1 = 0x%X, bfReserved2 = 0x%X, bfOffBits = %ld,"
                 "biSize = %ld, biWidth = %ld, biHeight = %ld,"
                 "biBitCount = %d, biCompression = %ld, biSizeImage = %ld,"
                 "biClrUsed = %ld, biClrImportant = %ld",
                 bmpHeader->bfType, bmpHeader->bfSize,
                 bmpHeader->bfReserved1, bmpHeader->bfReserved2, bmpHeader->bfOffBits,
                 bmpHeader->biSize, bmpHeader->biWidth, bmpHeader->biHeight,
                 bmpHeader->biBitCount, bmpHeader->biCompression, bmpHeader->biSizeImage,
                 bmpHeader->biClrUsed, bmpHeader->biClrImportant);

        if (bmpHeader->biBitCount != 1 && bmpHeader->biBitCount != 4
            && bmpHeader->biBitCount != 8 && bmpHeader->biBitCount != 16
            && bmpHeader->biBitCount != 24 && bmpHeader->biBitCount != 32) {
            ESP_LOGW(TAG, "not a supported bmp file biBitCount : %d", bmpHeader->biBitCount);
            return;
        }

        // stride = ((((biWidth * biBitCount) + 31) & ~31) >> 3)

        if ((bmpHeader->biBitCount == 1 || bmpHeader->biBitCount == 4 || bmpHeader->biBitCount == 8)
            && bmpHeader->biCompression == BI_RGB) {
            int bmiColorsCount = (bmpHeader->bfOffBits - 14 - bmpHeader->biSize) / sizeof(RGBQUAD);
            assert(bmiColorsCount == 1 << bmpHeader->biBitCount);
        } else if ((bmpHeader->biBitCount == 16 || bmpHeader->biBitCount == 32)
                   && bmpHeader->biCompression == BI_BITFIELDS) {
            assert(bmpHeader->bfOffBits - 14 - bmpHeader->biSize == sizeof(RGBQUAD_COLOR_MASK));
        } else {
            ESP_LOGW(TAG, "not supported bmp file biBitCount : %d biCompression:%ld",
                     bmpHeader->biBitCount,
                     bmpHeader->biCompression);
            return;
        }

        // check data size
        uint16_t stride = ((((bmpHeader->biWidth * bmpHeader->biBitCount) + 31) & ~31) >> 3);
        uint16_t image_byte_count = stride * abs(bmpHeader->biHeight);
        if (bmpHeader->biSizeImage > 0 && bmpHeader->biCompression == BI_RGB) {
            assert(bmpHeader->biSizeImage == image_byte_count);
        }

        uint8_t lut_count = 1 << bmpHeader->biBitCount;
        if (bmpHeader->biBitCount == 1) {
            RGBQUAD *color = (RGBQUAD *) (aniya_200_1_bmp_start + sizeof(bmp_header));
            for (int i = 0; i < lut_count; ++i) {
                ESP_LOGI(TAG, "lut : %d, blue:%d green:%d red:%d", i, color[i].blue, color[i].green, color[i].red);
            }
        }

        bmp_pixel_color color;
        bmp_img_common *bmp_img = (bmp_img_common *) aniya_200_1_bmp_start;
        bmp_get_pixel(&color, bmp_img, 0, 0);
        ESP_LOGI(TAG, "pixel : x:%d, y:%d blue:%d green:%d red:%d", 0, 0, color.blue, color.green, color.red);
    }
}

void bitmap_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    if (current_bitmap_page_index % 2 == 0) {
        epd_paint_draw_bitmap(epd_paint, 0, 0, LCD_H_RES, LCD_V_RES, (uint8_t *) aniya_200_1_bmp_start,
                              aniya_200_1_bmp_end - aniya_200_1_bmp_start, 1);
    } else {
        epd_paint_draw_bitmap(epd_paint, 0, 0, LCD_H_RES, LCD_V_RES, (uint8_t *) aniya_200_bmp_start,
                              aniya_200_bmp_end - aniya_200_bmp_start, 1);
    }
}

bool bitmap_page_key_click_handle(key_event_id_t key_event_type) {
    int full_update = 0;
    switch (key_event_type) {
        case KEY_1_SHORT_CLICK:
            current_bitmap_page_index += 1;
            esp_event_post_to(event_loop_handle, BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0,
                              &full_update, sizeof(full_update), 100 / portTICK_PERIOD_MS);
            return true;
        case KEY_1_LONG_CLICK:
            current_bitmap_page_index -= 1;
            esp_event_post_to(event_loop_handle, BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0,
                              &full_update, sizeof(full_update), 100 / portTICK_PERIOD_MS);
            return true;
        default:
            return false;
    }

    return false;
}