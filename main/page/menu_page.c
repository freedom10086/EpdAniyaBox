#include "esp_log.h"

#include "lcd/epd_lcd_ssd1680.h"
#include "bike_common.h"
#include "menu_page.h"
#include "static/static.h"
#include "page_manager.h"

#define TAG "menu_page"

#define MENU_ITEM_COUNT 4
#define MENU_START_Y 144
#define MENU_ITEM_HEIGHT 56
#define MENU_ITEM_WIDTH 50
#define MENU_AUTO_CLOSE_TIMEOUT_TS 30

static uint32_t last_key_event_tick;
static uint8_t current_index = 0;
static uint32_t switching_index = 0;

void menu_page_on_create(void *arg) {
    ESP_LOGI(TAG, "menu_page_on_create");
    last_key_event_tick = xTaskGetTickCount();
    switching_index = 0;
}

void menu_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    if (xTaskGetTickCount() - last_key_event_tick > configTICK_RATE_HZ * MENU_AUTO_CLOSE_TIMEOUT_TS) {
        page_manager_close_menu();
        return;
    }

    //ESP_LOGI(TAG, "menu_page_draw");
    epd_paint_clear_range(epd_paint, 0, MENU_START_Y, LCD_H_RES, LCD_V_RES, 0);

    // draw line
    epd_paint_draw_horizontal_line(epd_paint, 0, MENU_START_Y, LCD_H_RES, 1);
    epd_paint_draw_horizontal_line(epd_paint, 0, MENU_START_Y + MENU_ITEM_HEIGHT, LCD_H_RES, 1);

    epd_paint_draw_vertical_line(epd_paint, MENU_ITEM_WIDTH, MENU_START_Y, MENU_ITEM_HEIGHT * 2, 1);
    epd_paint_draw_vertical_line(epd_paint, MENU_ITEM_WIDTH * 2, MENU_START_Y, MENU_ITEM_HEIGHT * 2, 1);
    epd_paint_draw_vertical_line(epd_paint, MENU_ITEM_WIDTH * 3, MENU_START_Y, MENU_ITEM_HEIGHT * 2, 1);

    // draw icon
    // 0. home
    epd_paint_draw_bitmap(epd_paint, 8, MENU_START_Y + 4, 35, 32,
                          (uint8_t *) ic_home_bmp_start,
                          ic_home_bmp_end - ic_home_bmp_start, 1);
    uint16_t home[] = {0xD7CA, 0xB3D2, 0x00};
    epd_paint_draw_string_at(epd_paint, 9, MENU_START_Y + 38, (char *) home, &Font_HZK16, 1);

    // 1. image
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH + 7, MENU_START_Y + 4, 36, 32,
                          (uint8_t *) ic_image_bmp_start,
                          ic_image_bmp_end - ic_image_bmp_start, 1);
    uint16_t image[] = {0xBCCD, 0xACC6, 0x00};
    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH + 9, MENU_START_Y + 38, (char *) image, &Font_HZK16, 1);

    // 2. setting
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH * 2 + 8, MENU_START_Y + 4, 33, 32,
                          (uint8_t *) ic_setting_bmp_start,
                          ic_setting_bmp_end - ic_setting_bmp_start, 1);
    uint16_t setting[] = {0xE8C9, 0xC3D6, 0x00};
    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH * 2 + 9, MENU_START_Y + 38, (char *) setting, &Font_HZK16, 1);

    // 3. close
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH * 3 + 8, MENU_START_Y + 4, 32, 32,
                          (uint8_t *) ic_close_bmp_start,
                          ic_close_bmp_end - ic_close_bmp_start, 1);
    uint16_t close[] = {0xD8B9, 0xD5B1, 0x00};
    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH * 3 + 9, MENU_START_Y + 38, (char *) close, &Font_HZK16, 1);

    // select item
    uint8_t startX = (current_index % 4) * MENU_ITEM_WIDTH + 1;
    epd_paint_reverse_range(epd_paint, startX + 1, MENU_START_Y + 2, MENU_ITEM_WIDTH - 3, MENU_ITEM_HEIGHT - 2);
}

void menu_page_after_draw(uint32_t loop_cnt) {
    switching_index = 0;
}

void change_select(bool next) {
    switching_index = xTaskGetTickCount();
    current_index = (current_index + MENU_ITEM_COUNT + (next ? 1 : -1)) % MENU_ITEM_COUNT;
    int full_update = 0;
    post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, &full_update, sizeof(full_update));
}

void handle_click_event() {
    if (current_index == 0) {
        // home page
        page_manager_switch_page("temperature");
    } else if (current_index == 1) {
        // image manager
        page_manager_switch_page("image");
    } else if (current_index == 2) {
        // setting
        page_manager_switch_page("setting-list");
    }
    page_manager_close_menu();
    page_manager_request_update(false);
}

bool menu_page_key_click(key_event_id_t key_event_type) {
    if (switching_index > 0 && xTaskGetTickCount() - switching_index < configTICK_RATE_HZ * 2) {
        ESP_LOGW(TAG, "handle pre click ignore %d", key_event_type);
        return true;
    }
    switch (key_event_type) {
        case KEY_1_SHORT_CLICK:
            last_key_event_tick = xTaskGetTickCount();
            handle_click_event();
            break;
        case KEY_1_LONG_CLICK:
            page_manager_close_menu();
            page_manager_request_update(false);
            break;
        case KEY_2_SHORT_CLICK:
            last_key_event_tick = xTaskGetTickCount();
            change_select(true);
            break;
        case KEY_2_LONG_CLICK:
            last_key_event_tick = xTaskGetTickCount();
            change_select(false);
            break;
        default:
            return false;
    }
    return true;
}

void menu_page_on_destroy(void *arg) {

}