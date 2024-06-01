#include "setting_list_page.h"

#include "esp_log.h"

#include "lcd/epd_lcd_ssd1680.h"
#include "bike_common.h"
#include "static/static.h"
#include "page_manager.h"

#define TAG "setting_list_page"

#define SETTING_ITEM_HEIGHT 40
#define PADDING_X 12
#define PADDING_Y 4
#define TEXT_PADDING_Y 11
#define TOTAL_SETTING_ITEM_COUNT 8

static bool switching_index = false;
static int16_t current_index = 0;
static int16_t offset_item = 0;

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

    uint16_t y = 0;

    if (offset_item < 1) {
        // 0 close
        epd_paint_draw_bitmap(epd_paint, 15, y + PADDING_Y, 17, 32,
                              (uint8_t *) ic_back_bmp_start,
                              ic_back_bmp_end - ic_back_bmp_start, 1);
        uint16_t close[] = {0xCBCD, 0xF6B3, 0x00};
        epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, y + TEXT_PADDING_Y,
                                 (char *) close, &Font_HZK16, 1);
        y += SETTING_ITEM_HEIGHT;
    }

    if (offset_item < 2) {
        // 1. info
        epd_paint_draw_bitmap(epd_paint, 9, y + PADDING_Y, 32, 32,
                              (uint8_t *) ic_info_bmp_start,
                              ic_info_bmp_end - ic_info_bmp_start, 1);
        uint16_t info[] = {0xD8B9, 0xDAD3, 0x00};
        epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, y + TEXT_PADDING_Y,
                                 (char *) info, &Font_HZK16, 1);
        y += SETTING_ITEM_HEIGHT;
    }

    if (offset_item < 3) {
        // 2. manual
        epd_paint_draw_bitmap(epd_paint, 10, y + PADDING_Y, 30, 32,
                              (uint8_t *) ic_manual_bmp_start,
                              ic_manual_bmp_end - ic_manual_bmp_start, 1);
        uint16_t manual[] = {0xB5CB, 0xF7C3, 0x00};
        epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, y + TEXT_PADDING_Y,
                                 (char *) manual, &Font_HZK16, 1);
        y += SETTING_ITEM_HEIGHT;
    }

    if (offset_item < 4) {
        // 3. upload
        epd_paint_draw_bitmap(epd_paint, 7, y + PADDING_Y, 30, 32,
                              (uint8_t *) ic_image_bmp_start,
                              ic_image_bmp_end - ic_image_bmp_start, 1);
        uint16_t upload[] = {0xCFC9, 0xABB4, 0xBCCD, 0xACC6, 0x00};
        epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, y + TEXT_PADDING_Y,
                                 (char *) upload, &Font_HZK16, 1);
        y += SETTING_ITEM_HEIGHT;
    }

    if (offset_item < 5) {
        // 4. upgrade
        epd_paint_draw_bitmap(epd_paint, 8, y + PADDING_Y, 32, 32,
                              (uint8_t *) ic_upgrade_bmp_start,
                              ic_upgrade_bmp_end - ic_upgrade_bmp_start, 1);
        uint16_t upgrade[] = {0xFDC9, 0xB6BC, 0x00};
        epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, y + TEXT_PADDING_Y,
                                 (char *) upgrade, &Font_HZK16, 1);
        y += SETTING_ITEM_HEIGHT;
    }
    if (offset_item < 6) {
        // 5. ble
        epd_paint_draw_bitmap(epd_paint, 11, y + PADDING_Y, 28, 32,
                              (uint8_t *) ic_ble_bmp_start,
                              ic_ble_bmp_end - ic_ble_bmp_start, 1);
        uint16_t ble_device[] = {0xB6C0, 0xC0D1, 0xE8C9, 0xB8B1, 0x00};
        epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, y + TEXT_PADDING_Y,
                                 (char *) ble_device, &Font_HZK16, 1);
        y += SETTING_ITEM_HEIGHT;
    }
    if (offset_item < 7) {
        // 6. dict
        epd_paint_draw_bitmap(epd_paint, 9, y + PADDING_Y, 32, 32,
                              (uint8_t *) ic_dict_bmp_start,
                              ic_dict_bmp_end - ic_dict_bmp_start, 1);
        uint16_t dict[] = {0xA5B5, 0xCAB4, 0x00};
        epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, y + TEXT_PADDING_Y,
                                 (char *) dict, &Font_HZK16, 1);
        y += SETTING_ITEM_HEIGHT;
    }
    if (offset_item < 8) {
        // 7. reboot
        epd_paint_draw_bitmap(epd_paint, 9, y + PADDING_Y, 32, 32,
                              (uint8_t *) ic_reboot_bmp_start,
                              ic_reboot_bmp_end - ic_reboot_bmp_start, 1);
        uint16_t reboot[] = {0xD8D6, 0xF4C6, 0x00};
        epd_paint_draw_string_at(epd_paint, SETTING_ITEM_HEIGHT + PADDING_X, y + TEXT_PADDING_Y,
                                 (char *) reboot, &Font_HZK16, 1);
        y += SETTING_ITEM_HEIGHT;
    }

    // select item
    uint16_t start_y = current_index * SETTING_ITEM_HEIGHT - offset_item * SETTING_ITEM_HEIGHT;
    epd_paint_reverse_range(epd_paint, 0, start_y + 2, LCD_H_RES, SETTING_ITEM_HEIGHT - 3);
}

void setting_list_page_after_draw(uint32_t loop_cnt) {
    switching_index = false;
}

static void change_select(bool next) {
    switching_index = true;
    current_index = (current_index + (next ? 1 : -1)) % TOTAL_SETTING_ITEM_COUNT;
    if (current_index < 0) {
        current_index += TOTAL_SETTING_ITEM_COUNT;
    }

    int16_t select_start_y = current_index * SETTING_ITEM_HEIGHT - offset_item * SETTING_ITEM_HEIGHT;
    int16_t select_end_y = select_start_y + SETTING_ITEM_HEIGHT;

    if (select_start_y < 0) {
        offset_item -= (0 - select_start_y + SETTING_ITEM_HEIGHT - 1) / SETTING_ITEM_HEIGHT;
    } else if (select_end_y > LCD_V_RES) {
        offset_item += (select_end_y - LCD_V_RES + SETTING_ITEM_HEIGHT - 1) / SETTING_ITEM_HEIGHT;
    }

    ESP_LOGI(TAG, "current index:%d, current offset:%d", current_index, offset_item);

    int full_update = 0;
    common_post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, &full_update, sizeof(full_update));
}

static void handle_click_event() {
    if (current_index == 0) {
        page_manager_close_page();
    } else if (current_index == 1) {
        page_manager_switch_page("info");
    } else if (current_index == 2) {
        page_manager_switch_page("manual");
    } else if (current_index == 3) {
        page_manager_switch_page("image-manage");
    } else if (current_index == 4) {
        page_manager_switch_page("upgrade");
    } else if (current_index == 5) {
#ifdef CONFIG_ENABLE_BLE_DEVICES
        page_manager_switch_page("ble-device");
#endif
    } else if (current_index == 6) {
        page_manager_switch_page("dict-page");
    } else if (current_index == 7) {
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
            change_select(true);
            break;
        case KEY_2_LONG_CLICK:
            change_select(false);
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