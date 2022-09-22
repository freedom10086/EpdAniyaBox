#include "setting_list_page.h"

#include "esp_log.h"

#include "lcd/epd_lcd_ssd1680.h"
#include "bike_common.h"
#include "static/static.h"
#include "page_manager.h"

#define TAG "setting_list_page"

static uint8_t current_index = 0;
static bool switching_index = false;

#define SETTING_ITEM_HEIGHT 40
#define PADDING_X 12
#define PADDING_Y 4
#define TEXT_PADDING_Y 11

void setting_list_page_on_create(void *arg) {
    ESP_LOGI(TAG, "on_create");
}

void setting_list_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);

    // draw line
    epd_paint_draw_horizontal_line(epd_paint, 0, SETTING_ITEM_HEIGHT * 0, LCD_H_RES, 1);
    epd_paint_draw_horizontal_line(epd_paint, 0, SETTING_ITEM_HEIGHT * 1, LCD_H_RES, 1);
    epd_paint_draw_horizontal_line(epd_paint, 0, SETTING_ITEM_HEIGHT * 2, LCD_H_RES, 1);
    epd_paint_draw_horizontal_line(epd_paint, 0, SETTING_ITEM_HEIGHT * 3, LCD_H_RES, 1);
    epd_paint_draw_horizontal_line(epd_paint, 0, SETTING_ITEM_HEIGHT * 4, LCD_H_RES, 1);

    // 0 close
    epd_paint_draw_bitmap(epd_paint, 15, SETTING_ITEM_HEIGHT * 0 + PADDING_Y, 17, 32,
                          (uint8_t *) ic_back_bmp_start,
                          ic_back_bmp_end - ic_back_bmp_start, 1);
    uint16_t close[] = {0xCBCD, 0xF6B3, 0x00};
    epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, SETTING_ITEM_HEIGHT * 0 + TEXT_PADDING_Y,
                             (char *) close, &Font_HZK16, 1);

    // 1. info
    epd_paint_draw_bitmap(epd_paint, 9, SETTING_ITEM_HEIGHT * 1 + PADDING_Y, 32, 32,
                          (uint8_t *) ic_info_bmp_start,
                          ic_info_bmp_end - ic_info_bmp_start, 1);
    uint16_t info[] = {0xD8B9, 0xDAD3, 0x00};
    epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, SETTING_ITEM_HEIGHT * 1 + TEXT_PADDING_Y,
                             (char *) info, &Font_HZK16, 1);

    // 2. manual
    epd_paint_draw_bitmap(epd_paint, 10, SETTING_ITEM_HEIGHT * 2 + PADDING_Y, 30, 32,
                          (uint8_t *) ic_manual_bmp_start,
                          ic_manual_bmp_end - ic_manual_bmp_start, 1);
    uint16_t manual[] = {0xB5CB, 0xF7C3, 0x00};
    epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, SETTING_ITEM_HEIGHT * 2 + TEXT_PADDING_Y,
                             (char *) manual, &Font_HZK16, 1);


    // 3. upgrade
    epd_paint_draw_bitmap(epd_paint, 8, SETTING_ITEM_HEIGHT * 3 + PADDING_Y, 32, 32,
                          (uint8_t *) ic_upgrade_bmp_start,
                          ic_upgrade_bmp_end - ic_upgrade_bmp_start, 1);
    uint16_t upgrade[] = {0xFDC9, 0xB6BC, 0x00};
    epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, SETTING_ITEM_HEIGHT * 3 + TEXT_PADDING_Y,
                             (char *) upgrade, &Font_HZK16, 1);

    // 4. reboot
    epd_paint_draw_bitmap(epd_paint, 9, SETTING_ITEM_HEIGHT * 4 + PADDING_Y, 32, 32,
                          (uint8_t *) ic_reboot_bmp_start,
                          ic_reboot_bmp_end - ic_reboot_bmp_start, 1);
    uint16_t reboot[] = {0xD8D6, 0xF4C6, 0x00};
    epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, SETTING_ITEM_HEIGHT * 4 + TEXT_PADDING_Y,
                             (char *) reboot, &Font_HZK16, 1);

    // select item
    uint8_t start_y = current_index * SETTING_ITEM_HEIGHT;
    epd_paint_reverse_range(epd_paint, 0, start_y + 2, LCD_H_RES, SETTING_ITEM_HEIGHT - 3);
}

void setting_list_page_after_draw(uint32_t loop_cnt) {
    switching_index = false;
}

static void select_next() {
    switching_index = true;
    current_index = (current_index + 1) % 5;
    int full_update = 0;
    post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, &full_update, sizeof(full_update));
}

static void handle_click_event() {
    if (current_index == 0) {
        page_manager_close_page();
    } else if (current_index == 1) {
        page_manager_switch_page("info");
    } else if (current_index == 2) {
        page_manager_switch_page("manual");
    } else if (current_index == 3) {
        page_manager_switch_page("upgrade");
    } else if (current_index == 4) {
        esp_restart();
    }
    page_manager_request_update(false);
}

bool setting_list_page_key_click(key_event_id_t key_event_type) {
    if (switching_index) {
        ESP_LOGW(TAG, "handle pre click ignore %d", key_event_type);
        return true;
    }
    switch (key_event_type) {
        case KEY_1_SHORT_CLICK:
            handle_click_event();
            break;
        case KEY_2_SHORT_CLICK:
            select_next();
            break;
        default:
            return false;
    }
    return true;
}

int setting_list_page_on_enter_sleep(void *args) {
    return DEFAULT_SLEEP_TS;
}

void setting_list_page_on_destroy(void *arg) {

}