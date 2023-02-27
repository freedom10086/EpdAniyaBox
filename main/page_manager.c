#include "stdio.h"
#include "esp_log.h"
#include "string.h"

#include "page_manager.h"
#include "bike_common.h"

#include "page/test_page.h"
#include "page/info_page.h"
#include "page/image_page.h"
#include "page/temperature_page.h"
#include "page/upgrade_page.h"
#include "page/menu_page.h"
#include "page/manual_page.h"
#include "page/image_manage_page.h"
#include "page/setting_list_page.h"
#include "page/ble_device_page.h"
#include "battery.h"

#define TAG "page-manager"
#define TOTAL_PAGE 9

static int8_t pre_page_index = -1;
static int8_t menu_index = -1;

RTC_DATA_ATTR static int8_t current_page_index = -1;

static page_inst_t pages[] = {
        [TEMP_PAGE_INDEX] = {
                .page_name = "temperature",
                .on_draw_page = temperature_page_draw,
                .key_click_handler = temperature_page_key_click_handle,
                .on_create_page = temperature_page_on_create,
                .on_destroy_page = temperature_page_on_destroy
        },
        [1] = {
                .page_name = "info",
                .on_draw_page = info_page_draw,
                .on_create_page = info_page_on_create,
                .key_click_handler = info_page_key_click,
                .enter_sleep_handler = info_page_on_enter_sleep,
        },
        [2] = {
                .page_name = "test",
                .on_draw_page = test_page_draw,
        },
        [IMAGE_PAGE_INDEX] = {
                .page_name = "image",
                .on_draw_page = image_page_draw,
                .key_click_handler = image_page_key_click_handle,
                .on_create_page = image_page_on_create,
                .on_destroy_page = image_page_on_destroy,
                .enter_sleep_handler = image_page_on_enter_sleep,
        },
        [4] = {
                .page_name = "upgrade",
                .on_draw_page = upgrade_page_draw,
                .key_click_handler = upgrade_page_key_click_handle,
                .on_create_page = upgrade_page_on_create,
                .on_destroy_page = upgrade_page_on_destroy,
                .enter_sleep_handler = upgrade_page_on_enter_sleep,
        },
        [5] = {
                .page_name = "manual",
                .on_draw_page = manual_page_draw,
                .key_click_handler = manual_page_key_click,
                .on_create_page = manual_page_on_create,
                .on_destroy_page = manual_page_on_destroy,
        },
        [6] = {
                .page_name = "image-manage",
                .on_draw_page = image_manage_page_draw,
                .key_click_handler = image_manage_page_key_click,
                .on_create_page = image_manage_page_on_create,
                .on_destroy_page = image_manage_page_on_destroy,
                .enter_sleep_handler = image_manage_page_on_enter_sleep,
        },
        [7] = {
                .page_name = "setting-list",
                .on_draw_page = setting_list_page_draw,
                .key_click_handler = setting_list_page_key_click,
                .on_create_page = setting_list_page_on_create,
                .on_destroy_page = setting_list_page_on_destroy,
                .enter_sleep_handler = setting_list_page_on_enter_sleep,
                .after_draw_page = setting_list_page_after_draw,
        },
        [8] = {
            .page_name = "ble-device",
#ifdef CONFIG_ENABLE_BLE_DEVICES
            .on_draw_page = ble_device_page_draw,
            .key_click_handler = ble_device_page_key_click,
            .on_create_page = ble_device_page_on_create,
            .on_destroy_page = ble_device_page_on_destroy,
            .enter_sleep_handler = ble_device_page_on_enter_sleep,
            .after_draw_page = ble_device_page_after_draw,
#endif
        }
};

static page_inst_t menus[] = {
        [0] = {
                .page_name = "menu",
                .on_draw_page = menu_page_draw,
                .key_click_handler = menu_page_key_click,
                .on_create_page = menu_page_on_create,
                .on_destroy_page = menu_page_on_destroy,
                .after_draw_page = menu_page_after_draw,
        }
};

void page_manager_init(char *default_page) {
    // reset to -1 when awake from deep sleep
    current_page_index = -1;
    page_manager_switch_page(default_page);
}

int8_t page_manager_get_current_index() {
    return current_page_index;
}

void page_manager_switch_page_by_index(int8_t dest_page_index, bool open_by_parent) {
    if (current_page_index == dest_page_index) {
        ESP_LOGW(TAG, "dest page is current %d", dest_page_index);
        return;
    }
    if (dest_page_index < 0) {
        ESP_LOGE(TAG, "dest page is invalid %d", dest_page_index);
        return;
    }

    ESP_LOGI(TAG, "page switch from %s to %s ",
             current_page_index >= 0 ? pages[current_page_index].page_name : "empty",
             pages[dest_page_index].page_name);

    // new page on create
    if (open_by_parent) {
        pages[dest_page_index].parent_page_index = current_page_index;
    }
    if (pages[dest_page_index].on_create_page != NULL) {
        pages[dest_page_index].on_create_page(&current_page_index);
        ESP_LOGI(TAG, "page %s on create", pages[dest_page_index].page_name);
    }
    pre_page_index = current_page_index;
    current_page_index = dest_page_index;

    // old page destroy
    if (pre_page_index >= 0 && pages[pre_page_index].on_destroy_page != NULL) {
        pages[pre_page_index].on_destroy_page(NULL);
        ESP_LOGI(TAG, "page %s on destroy", pages[pre_page_index].page_name);
    }
}

void page_manager_switch_page(char *page_name) {
    int8_t idx = -1;
    for (int8_t i = 0; i < TOTAL_PAGE; ++i) {
        if (strcmp(pages[i].page_name, page_name) == 0) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        ESP_LOGE(TAG, "can not find page %s", page_name);
        page_manager_switch_page_by_index(0, true);
        return;
    }
    page_manager_switch_page_by_index(idx, true);
}

void page_manager_close_page() {
    if (pre_page_index == -1) {
        ESP_LOGE(TAG, "can not close curent page no pre page");
        return;
    }
    page_inst_t curr_page = page_manager_get_current_page();
    if (curr_page.parent_page_index >= 0) {
        page_manager_switch_page_by_index(curr_page.parent_page_index, false);
    } else {
        page_manager_switch_page_by_index(pre_page_index, false);
    }
}

page_inst_t page_manager_get_current_page() {
    page_inst_t current_page = pages[current_page_index];
    return current_page;
}

bool page_manager_has_menu() {
    return menu_index != -1;
}

page_inst_t page_manager_get_current_menu() {
    page_inst_t current_menu = menus[menu_index];
    return current_menu;
}

void page_manager_show_menu(char *name) {
    if (menu_index == -1) {
        // new page on create
        if (menus[0].on_create_page != NULL) {
            menus[0].on_create_page(NULL);
            ESP_LOGI(TAG, "menu %s on create", menus[0].page_name);
        }
        menu_index = 0;
    } else {
        ESP_LOGW(TAG, "menu %s exist cant show new", menus[menu_index].page_name);
    }
}

void page_manager_close_menu() {
    if (menu_index != -1) {
        if (menus[menu_index].on_destroy_page != NULL) {
            menus[menu_index].on_destroy_page(NULL);
            ESP_LOGI(TAG, "menu %s on destroy", menus[menu_index].page_name);
        }
        menu_index = -1;
    }
}

int page_manager_enter_sleep(uint32_t loop_cnt) {
    if (page_manager_has_menu()) {
        page_manager_close_menu();
        page_manager_request_update(false);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    page_inst_t current_page = page_manager_get_current_page();
    if (current_page.enter_sleep_handler != NULL) {
        return current_page.enter_sleep_handler((void *) loop_cnt);
    }

    int8_t battery_level = battery_get_level();
    if (battery_level > 0) {
        if (battery_is_charge()) {
            ESP_LOGI(TAG, "battery is charge use default sleep time");
            return DEFAULT_SLEEP_TS;
        }
        if (battery_level < 5) {
            // battery low never wake up
            return 0;
        }
        if (battery_level < 20) {
            return DEFAULT_SLEEP_TS * 5;
        }
        if (battery_level < 50) {
            return DEFAULT_SLEEP_TS * 2;
        }
    }
    // battery is invalid use default sleep time
    return DEFAULT_SLEEP_TS;
}

void page_manager_request_update(uint32_t full_refresh) {
    common_post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, &full_refresh, sizeof(full_refresh));
}