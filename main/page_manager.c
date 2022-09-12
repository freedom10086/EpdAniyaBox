#include "stdio.h"
#include "esp_log.h"
#include "string.h"

#include "page_manager.h"

#include "page/main_page.h"
#include "page/test_page.h"
#include "page/info_page.h"
#include "page/bitmap_page.h"
#include "page/temperature_page.h"
#include "page/upgrade_page.h"

#define TAG "page-manager"
#define TOTAL_PAGE 5

static int8_t pre_page_index = -1;
static int8_t current_page_index = -1;

static page_inst_t pages[] = {
        [0] = {
                .page_name = "main",
                .on_draw_page = main_page_draw,
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
        [3] = {
                .page_name = "bitmap",
                .on_draw_page = bitmap_page_draw,
                .key_click_handler = bitmap_page_key_click_handle,
        },
        [4] = {
                .page_name = "temperature",
                .on_draw_page = temperature_page_draw,
                .key_click_handler = temperature_page_key_click_handle,
                .on_create_page = temperature_page_on_create,
                .on_destroy_page = temperature_page_on_destroy
        },
        [5] = {
                .page_name = "upgrade",
                .on_draw_page = upgrade_page_draw,
                .key_click_handler = upgrade_page_key_click_handle,
                .on_create_page = upgrade_page_on_create,
                .on_destroy_page = upgrade_page_on_destroy
        }
};

void page_manager_init(char *default_page) {
    page_manager_switch_page(default_page);
}

void page_manager_reg_page(page_inst_t page) {

}

void page_manager_switch_page_by_index(int8_t dest_page_index) {
    if (current_page_index == dest_page_index) {
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
    if (pages[dest_page_index].on_create_page != NULL) {
        pages[dest_page_index].on_create_page(NULL);
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
    page_manager_switch_page_by_index(idx);
}

void page_manager_close_page() {
    if (pre_page_index == -1) {
        ESP_LOGE(TAG, "can not close curent page no pre page");
        return;
    }
    page_manager_switch_page_by_index(pre_page_index);
}

page_inst_t page_manager_get_current_page() {
    page_inst_t current_page = pages[current_page_index];
    return current_page;
}

bool page_manager_enter_sleep(uint32_t loop_cnt) {
    page_inst_t current_page = page_manager_get_current_page();
    if (current_page.enter_sleep_handler != NULL) {
        return current_page.enter_sleep_handler((void *)loop_cnt);
    }
    return true;
}