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

#include "bike_common.h"
#include "test_page.h"
#include "tools/encode.h"
#include "lcd/epdpaint.h"


/*********************
 *      DEFINES
 *********************/
#define TAG "test-page"

void test_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    //你好世界！ gbk encode
    // https://www.qqxiuzi.cn/bianma/zifuji.php
    uint8_t data[] = {0xC4, 0xE3, 0xBA, 0xC3, 0xCA, 0xC0, 0xBD, 0xE7, 0xA3, 0xA1};
    epd_paint_draw_string_at(epd_paint, 4, 4, (char *) data, &Font_HZK16, 1);
}