#include <stdio.h>
#include <stdlib.h>

#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include "wifi/wifi_ap.h"
#include "string.h"

#include <esp_log.h>
#include "static/static.h"
#include "upgrade_page.h"
#include "bike_common.h"
#include "wifi/my_http_server.h"
#include "page_manager.h"

#define TAG "upgrade-page"
#define HTTP_PREFIX "http://"

static bool wifi_on = false;
static char draw_buff[24] = {};

float last_ota_update_progress = 0;
float ota_progress = 0;

enum upgrade_state_t state = INIT;

static void ota_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data) {
    switch (event_id) {
        case OTA_START:
            state = UPGRADING;
            ota_progress = 0;
            last_ota_update_progress = 0;
            break;
        case OTA_PROGRESS:
            state = UPGRADING;
            ota_progress = *(float *) event_data;
            break;
        case OTA_SUCCESS:
            state = UPGRADE_SUCCESS;
            ota_progress = 100.0f;
            wifi_deinit_softap();
            break;
        case OTA_FAILED:
            state = UPGRADE_FAILED;
            wifi_deinit_softap();
            break;
        default:
            break;
    }

    if (event_id == OTA_PROGRESS) {
        if (ota_progress - last_ota_update_progress >= 0.5) {
            last_ota_update_progress = ota_progress;
            page_manager_request_update(false);
        }
    } else {
        page_manager_request_update(false);
    }
}

void upgrade_page_on_create(void *args) {
    state = INIT;
    // reg ota event
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_OTA_EVENT, ESP_EVENT_ANY_ID,
                                    ota_event_handler, NULL);

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

void upgrade_page_on_destroy(void *args) {
    if (wifi_on) {
        wifi_deinit_softap();
        wifi_on = false;
    }
}

void upgrade_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    if (state == INIT) {
        epd_paint_clear(epd_paint, 0);
        int y = 8;
        epd_paint_draw_string_at(epd_paint, 2, y,
                                 (char[]) {0xC1, 0xAC, 0xBD, 0xD3,
                                           0x57, 0x49, 0x46, 0x49,
                                           0xC9, 0xFD, 0xBC, 0xB6, 0xB9, 0xCC, 0xBC, 0xFE,
                                           0x3A, 0x00}, &Font_HZK16, 1);
        y += 20;

        sprintf(draw_buff, "SSID:%s", CONFIG_WIFI_SSID);
        epd_paint_draw_string_at(epd_paint, 4, y, draw_buff, &Font16, 1);
        y += 20;

        // 密码:
        epd_paint_draw_string_at(epd_paint, 4, y, (char[]) {0xC3, 0xDC, 0xC2, 0xEB, 0x3A, 0x00}, &Font_HZK16, 1);
        epd_paint_draw_string_at(epd_paint, 52, y, CONFIG_WIFI_PASSWORD, &Font_HZK16, 1);
        y += 20;

        esp_netif_ip_info_t ip_info;
        esp_err_t err = wifi_ap_get_ip(&ip_info);
        if (ESP_OK == err) {
            memcpy(draw_buff, HTTP_PREFIX, strlen(HTTP_PREFIX));
            esp_ip4addr_ntoa((const esp_ip4_addr_t *) &ip_info.ip.addr,
                             draw_buff + strlen(HTTP_PREFIX),
                             sizeof(draw_buff) - strlen(HTTP_PREFIX));
            epd_paint_draw_string_at(epd_paint, 4, y, draw_buff, &Font16, 1);
            y += 20;

            epd_paint_draw_string_at(epd_paint, 4, y, "/ota", &Font16, 1);
            y += 20;
        } else {
            epd_paint_draw_string_at(epd_paint, 4, y,
                                     (char[]) {0xBB, 0xF1, 0xC8, 0xA1, 0x49, 0x50,
                                               0xB5, 0xD8, 0xD6, 0xB7, 0xD6, 0xD0, 0x2E,
                                               0x00}, &Font_HZK16, 1);
            y += 20;
        }

        epd_paint_draw_string_at(epd_paint, 52, 174,
                                 (char[]) {0xB0, 0xB4, 0xC8, 0xCE, 0xD2, 0xE2, 0xBC,
                                           0xFC, 0xCD, 0xCB, 0xB3, 0xF6, 0x00},
                                 &Font_HZK16, 1);
    } else if (state == UPGRADING) {
        // upgrade icon
        epd_paint_draw_bitmap(epd_paint, 75, 70, 48, 38,
                              (uint8_t *) icon_upgrade_bmp_start,
                              icon_upgrade_bmp_end - icon_upgrade_bmp_start, 1);

        // 固件升级中.
        uint16_t data[] = {0xCCB9, 0xFEBC, 0xFDC9, 0xB6BC, 0xD0D6, 0x2E, 0x00};
        epd_paint_draw_string_at(epd_paint, 60, 114, (char *) data, &Font_HZK16, 1);

        char buff[8];
        sprintf(buff, "%.1f%%", ota_progress);
        epd_paint_draw_string_at(epd_paint, 80, 134, (char *) buff, &Font16, 1);
    } else if (state == UPGRADE_FAILED) {
        // upgrade failed icon
        epd_paint_draw_bitmap(epd_paint, 83, 62, 32, 32,
                              (uint8_t *) ic_close_bmp_start,
                              ic_close_bmp_end - ic_close_bmp_start, 1);

        // 固件升级失败.
        uint16_t data[] = {0xCCB9, 0xFEBC, 0xFDC9, 0xB6BC, 0xA7CA, 0xDCB0, 0x2E, 0x00};
        epd_paint_draw_string_at(epd_paint, 52, 114, (char *) data, &Font_HZK16, 1);

        // 按任意键重启
        epd_paint_draw_string_at(epd_paint, 52, 174,
                                 (char[]) {0xB0, 0xB4, 0xC8, 0xCE, 0xD2, 0xE2, 0xBC,
                                           0xFC, 0xD6, 0xD8, 0xC6, 0xF4, 0x00}, &Font_HZK16, 1);
    } else if (state == UPGRADE_SUCCESS) {
        // upgrade icon
        epd_paint_draw_bitmap(epd_paint, 75, 70, 48, 38,
                              (uint8_t *) icon_upgrade_bmp_start,
                              icon_upgrade_bmp_end - icon_upgrade_bmp_start, 1);

        // 固件升级成功.
        uint16_t data[] = {0xCCB9, 0xFEBC, 0xFDC9, 0xB6BC, 0xC9B3, 0xA6B9, 0x2E, 0x00};
        epd_paint_draw_string_at(epd_paint, 52, 114, (char *) data, &Font_HZK16, 1);

        char buff[8];
        sprintf(buff, "%.1f%%", ota_progress);
        epd_paint_draw_string_at(epd_paint, 80, 134, (char *) buff, &Font16, 1);

        // 按任意键重启
        epd_paint_draw_string_at(epd_paint, 52, 174,
                                 (char[]) {0xB0, 0xB4, 0xC8, 0xCE, 0xD2, 0xE2, 0xBC,
                                           0xFC, 0xD6, 0xD8, 0xC6, 0xF4, 0x00}, &Font_HZK16, 1);
    }
}

bool upgrade_page_key_click_handle(key_event_id_t key_event_type) {
    if (state == INIT) {
        page_manager_close_page();
    } else if (state == UPGRADE_FAILED) {
        esp_restart();
    } else if (state == UPGRADE_SUCCESS) {
        esp_restart();
    }

    return true;
}

int upgrade_page_on_enter_sleep(void *args) {
    // stop enter sleep
    return -1;
}