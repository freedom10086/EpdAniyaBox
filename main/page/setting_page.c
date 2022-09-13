//
// Created by yang on 2022/9/13.
//
#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include "wifi/wifi_ap.h"

#include "setting_page.h"
#include "view/list_view.h"

#include "bike_common.h"
#include "page_manager.h"
#include "battery.h"

#define TAG "setting-page"

list_view_t *list_view = NULL;
static bool wifi_on = false;

void init_list_view(int y) {
    list_view = list_vew_create(20, y, 120, 80, &Font16);
    list_view_add_element(list_view, wifi_on ? "clsoe wifi" : "open wifi");
    list_view_add_element(list_view, "bbbbb");
    list_view_add_element(list_view, "ccccc");
    list_view_add_element(list_view, "ddddd");
}

void setting_page_on_create(void *arg) {
    init_list_view(20);

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

    //    if (!wifi_on) {
//        wifi_init_softap();
//        wifi_on = true;
//    }
}

void setting_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);

    list_vew_draw(list_view, epd_paint, loop_cnt);
}

bool setting_page_key_click(key_event_id_t key_event_type) {
    if (KEY_2_SHORT_CLICK == key_event_type) {
        if (list_view) {
            list_view->current_index = (list_view->current_index + 1) % list_view->element_count;
            page_manager_request_update(false);
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
            page_manager_request_update(false);
            return true;
        }
    }
    return false;
}

void setting_page_on_destroy(void *arg) {
    if (wifi_on) {
        wifi_deinit_softap();
        wifi_on = false;
    }

    list_view_deinit(list_view);
}

bool setting_page_on_enter_sleep(void *args) {
    return !battery_is_curving();
}