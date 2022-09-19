#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include "esp_mac.h"
#include "esp_wifi.h"

#include "bike_common.h"
#include "lcd/epdpaint.h"
#include "battery.h"

#include "info_page.h"
#include "lcd/display.h"
#include "wifi/wifi_ap.h"
#include "lcd/epd_lcd_ssd1680.h"
#include "page_manager.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "info-page"

static char info_page_draw_text_buf[64] = {0};

bool wifi_on = false;
esp_app_desc_t running_app_info;
const esp_partition_t *running_partition;

bool info_page_key_click(key_event_id_t key_event_type) {
    if (key_event_type == KEY_1_SHORT_CLICK || key_event_type == KEY_2_SHORT_CLICK) {
        page_manager_close_page();
        page_manager_request_update(false);
        return true;
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

    running_partition = esp_ota_get_running_partition();
    esp_ota_get_partition_description(running_partition, &running_app_info);
}

void info_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    int y = 8;

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
    y += 18;

    // heap
    sprintf(info_page_draw_text_buf, "free heap:%ld", esp_get_free_heap_size());
    epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font16, 1);
    y += 18;

    // partition
    sprintf(info_page_draw_text_buf, "ota subtype:%d", running_partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
    epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font16, 1);
    y += 18;

    // version
    sprintf(info_page_draw_text_buf, "date:%s", running_app_info.date);
    epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font16, 1);
    y += 18;

    // time
    sprintf(info_page_draw_text_buf, "time:%s", running_app_info.time);
    epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font16, 1);
    y += 18;


    // version
    sprintf(info_page_draw_text_buf, "%s", running_app_info.version);
    epd_paint_draw_string_at(epd_paint, 0, LCD_V_RES - 1 - 18, info_page_draw_text_buf, &Font16, 1);
}

int info_page_on_enter_sleep(void *args) {
    return DEFAULT_SLEEP_TS;
}