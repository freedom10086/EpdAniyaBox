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
#include "lcd/epdpaint.h"
#include "battery.h"
#include "view/list_view.h"

#include "info_page.h"
#include "lcd/display.h"
#include "wifi/wifi_ap.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "info-page"

static char info_page_draw_text_buf[64] = {0};

list_view_t *list_view = NULL;

bool wifi_on = false;

void init_list_view(int y) {
    list_view = list_vew_create(20, y, 120, 80, &Font16);
    list_view_add_element(list_view, wifi_on ? "clsoe wifi" : "open wifi");
    list_view_add_element(list_view, "bbbbb");
    list_view_add_element(list_view, "ccccc");
    list_view_add_element(list_view, "ddddd");
}

bool info_page_key_click(key_event_id_t key_event_type) {
    int full_update = 0;
    if (KEY_2_SHORT_CLICK == key_event_type) {
        if (list_view) {
            list_view->current_index = (list_view->current_index + 1) % list_view->element_count;
            post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, &full_update, sizeof(full_update));
            return true;
        }
    } else if (KEY_1_SHORT_CLICK == key_event_type) {
        if (list_view && list_view->current_index == 0) {
            // on off wifi
            if (wifi_on) {
                wifi_deinit_softap();
                wifi_on = false;
            } else {
                wifi_init_softap();
                wifi_on = true;
            }
            list_view_update_item(list_view, 0, wifi_on ? "clsoe wifi" : "open wifi");
            post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, &full_update, sizeof(full_update));
            return true;
        }
    }

    return false;
}

void info_page_on_create(void *arg) {
    //ESP_ERR_WIFI_NOT_INIT
    wifi_mode_t wifi_mode;
    esp_err_t err = esp_wifi_get_mode(&wifi_mode);
    ESP_LOGI(TAG, "current wifi mdoe: %d, %d %s", wifi_mode, err, esp_err_to_name(err));
    if (ESP_ERR_WIFI_NOT_INIT == err) {
        wifi_on = false;
    } else {
        wifi_on = wifi_mode != WIFI_MODE_NULL;
    }
    ESP_LOGI(TAG, "current wifi status: %s", wifi_on ? "on" : "off");

    if (!wifi_on) {
        wifi_init_softap();
        wifi_on = true;
    }
}

void info_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    int y = 16;

    sprintf(info_page_draw_text_buf, "loop: %ld", loop_cnt);
    epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font20, 1);
    y += 20;

    wifi_mode_t wifi_mode;
    esp_err_t err = esp_wifi_get_mode(&wifi_mode);
    if (ESP_ERR_WIFI_NOT_INIT == err) {
        wifi_on = false;
        wifi_mode = WIFI_MODE_NULL;
        epd_paint_draw_string_at(epd_paint, 0, y, "wifi: off", &Font20, 1);
        y += 20;
    } else {
        wifi_on = true;
        epd_paint_draw_string_at(epd_paint, 0, y, "wifi: on", &Font20, 1);
        y += 20;

        // wifi mode
        sprintf(info_page_draw_text_buf, "mode: %s", wifi_mode == WIFI_MODE_STA ? "sta" : (
                wifi_mode == WIFI_MODE_AP ? "ap" : (
                        wifi_mode == WIFI_MODE_APSTA ? "ap sta" : "max"
                )
        ));
        epd_paint_draw_string_at(epd_paint, 16, y, info_page_draw_text_buf, &Font16, 1);
        y += 16;
    }

    // battery
    int battery_voltage = battery_get_voltage();
    int8_t battery_level = battery_get_level();

    epd_paint_draw_string_at(epd_paint, 0, y, "battery:", &Font20, 1);
    y += 20;

    sprintf(info_page_draw_text_buf, "%dmv %d%%", battery_voltage * 2, battery_level);
    epd_paint_draw_string_at(epd_paint, 16, y, info_page_draw_text_buf, &Font16, 1);
    y += 20;

    if (list_view == NULL) {
        init_list_view(y + 20);
    }
    list_vew_draw(list_view, epd_paint, loop_cnt);
}

bool info_page_on_enter_sleep(void *args) {
    return !battery_is_curving();
}