#include "esp_log.h"

#include "lcd/epd_lcd_ssd1680.h"
#include "bike_common.h"
#include "menu_page.h"
#include "static/static.h"
#include "page_manager.h"

#define TAG "menu_page"

static bool active = false;
static uint8_t current_index = 0;
static bool switching_index = false;
static bool handling_click_event = false;

#define MENU_START_Y 88
#define MENU_START_Y_2 144
#define MENU_ITEM_HEIGHT 56
#define MENU_ITEM_WIDTH 50

void menu_page_on_create(void *arg) {
    ESP_LOGI(TAG, "menu_page_on_create");
    active = true;
}

void menu_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    if (!active) {
        return;
    }

    ESP_LOGI(TAG, "menu_page_draw");
    epd_paint_clear_range(epd_paint, 0, MENU_START_Y, LCD_H_RES, LCD_V_RES, 0);

    // draw line
    epd_paint_draw_horizontal_line(epd_paint, 0, MENU_START_Y, LCD_H_RES, 1);
    epd_paint_draw_horizontal_line(epd_paint, 0, MENU_START_Y + MENU_ITEM_HEIGHT, LCD_H_RES, 1);

    epd_paint_draw_vertical_line(epd_paint, MENU_ITEM_WIDTH, MENU_START_Y, MENU_ITEM_HEIGHT * 2, 1);
    epd_paint_draw_vertical_line(epd_paint, MENU_ITEM_WIDTH * 2, MENU_START_Y, MENU_ITEM_HEIGHT * 2, 1);
    epd_paint_draw_vertical_line(epd_paint, MENU_ITEM_WIDTH * 3, MENU_START_Y, MENU_ITEM_HEIGHT * 2, 1);

    // draw icon
    // home
    epd_paint_draw_bitmap(epd_paint, 8, MENU_START_Y + 4, 35, 32,
                          (uint8_t *) ic_home_bmp_start,
                          ic_home_bmp_end - ic_home_bmp_start, 1);
    uint16_t home[] = {0xD7CA, 0xB3D2, 0x00};
    epd_paint_draw_string_at(epd_paint, 9, MENU_START_Y + 38, (char *) home, &Font_HZK16, 1);

    // image
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH + 7, MENU_START_Y + 4, 36, 32,
                          (uint8_t *) ic_image_bmp_start,
                          ic_image_bmp_end - ic_image_bmp_start, 1);
    uint16_t image[] = {0xBCCD, 0xACC6, 0x00};
    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH + 9, MENU_START_Y + 38, (char *) image, &Font_HZK16, 1);

    // manual
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH * 2 + 10, MENU_START_Y + 4, 30, 32,
                          (uint8_t *) ic_manual_bmp_start,
                          ic_manual_bmp_end - ic_manual_bmp_start, 1);
    uint16_t manual[] = {0xB5CB, 0xF7C3, 0x00};
    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH * 2 + 9, MENU_START_Y + 38, (char *) manual, &Font_HZK16, 1);

    // setting
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH * 3 + 8, MENU_START_Y + 4, 33, 32,
                          (uint8_t *) ic_setting_bmp_start,
                          ic_setting_bmp_end - ic_setting_bmp_start, 1);
    uint16_t setting[] = {0xE8C9, 0xC3D6, 0x00};
    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH * 3 + 9, MENU_START_Y + 38, (char *) setting, &Font_HZK16, 1);


    // second line
    // info
    epd_paint_draw_bitmap(epd_paint, 9, MENU_START_Y_2 + 4, 32, 32,
                          (uint8_t *) ic_info_bmp_start,
                          ic_info_bmp_end - ic_info_bmp_start, 1);
    uint16_t info[] = {0xC5D0, 0xA2CF, 0x00};
    epd_paint_draw_string_at(epd_paint, 9, MENU_START_Y_2 + 38, (char *) info, &Font_HZK16, 1);

    // upgrade
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH + 8, MENU_START_Y_2 + 4, 32, 32,
                          (uint8_t *) ic_upgrade_bmp_start,
                          ic_upgrade_bmp_end - ic_upgrade_bmp_start, 1);
    uint16_t upgrade[] = {0xFDC9, 0xB6BC, 0x00};
    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH + 9, MENU_START_Y_2 + 38, (char *) upgrade, &Font_HZK16, 1);

    // reboot
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH * 2 + 9, MENU_START_Y_2 + 4, 32, 32,
                          (uint8_t *) ic_reboot_bmp_start,
                          ic_reboot_bmp_end - ic_reboot_bmp_start, 1);
    uint16_t reboot[] = {0xD8D6, 0xF4C6, 0x00};
    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH * 2 + 9, MENU_START_Y_2 + 38, (char *) reboot, &Font_HZK16, 1);

    // close
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH * 3 + 8, MENU_START_Y_2 + 4, 32, 32,
                          (uint8_t *) ic_close_bmp_start,
                          ic_close_bmp_end - ic_close_bmp_start, 1);
    uint16_t close[] = {0xD8B9, 0xD5B1, 0x00};
    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH * 3 + 9, MENU_START_Y_2 + 38, (char *) close, &Font_HZK16, 1);

    // select item
    uint8_t startY = (current_index <= 3) ? MENU_START_Y : MENU_START_Y_2;
    uint8_t startX = (current_index % 4) * MENU_ITEM_WIDTH;
    epd_paint_reverse_range(epd_paint, startX + 1, startY + 2, MENU_ITEM_WIDTH - 2, MENU_ITEM_HEIGHT - 3);
}

void menu_page_after_draw(uint32_t loop_cnt) {
    switching_index = false;
}

void select_next() {
    switching_index = true;
    current_index = (current_index + 1) % 8;
    int full_update = 0;
    post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, &full_update, sizeof(full_update));
}

void handle_click_event() {
    if (handling_click_event) {
        return;
    }
    handling_click_event = true;
    if (current_index == 0) {
        // home page
        page_manager_switch_page("temperature");
    } else if (current_index == 1) {
        // image manager
    } else if (current_index == 2) {
        // manual
    } else if (current_index == 3) {
        // setting
    } else if (current_index == 4) {
        // info
        page_manager_switch_page("info");
    } else if (current_index == 5) {
        // upgrade
        page_manager_switch_page("upgrade");
    } else if (current_index == 6) {
        // reboot
        esp_restart();
        return;
    } else {
        // close
        page_manager_close_page();
    }

    int full_update = 0;
    post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, &full_update, sizeof(full_update));
    handling_click_event = false;
}

bool menu_page_key_click(key_event_id_t key_event_type) {
    if (!active) {
        return false;
    }
    if (switching_index) {
        ESP_LOGW(TAG, "handle pre click ignore %d", key_event_type);
        return true;
    } else {
        ESP_LOGI(TAG, "menu_page_key_click %d", key_event_type);
    }
    switch (key_event_type) {
        case KEY_1_SHORT_CLICK:
            handle_click_event();
            break;
        case KEY_2_SHORT_CLICK:
            select_next();
            break;
        case KEY_1_LONG_CLICK:
            page_manager_close_page();
            int full_update = 0;
            post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, &full_update, sizeof(full_update));
            break;
        default:
            return false;
    }
    return true;
}

bool menu_page_on_enter_sleep(void *args) {
    return !active;
}

void menu_page_on_destroy(void *arg) {
    active = false;
}