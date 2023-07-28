#include "ble_device_page.h"

#ifdef CONFIG_ENABLE_BLE_DEVICES

#include "esp_log.h"
#include "string.h"

#include "lcd/epd_lcd_ssd1680.h"
#include "bike_common.h"
#include "page_manager.h"
#include "ble/ble_device.h"

#define TAG "ble_device_page"

#define BUFF_LEN 20

#define ITEM_HEIGHT 22
#define PADDING_X 2
#define PADDING_Y 2
#define TEXT_PADDING_Y 2

static bool switching_index = false;
static int16_t current_index = 0;
static int16_t offset_item = 0;
static uint8_t total_item_count = 1;

static bool scanning = false;
static char buff[BUFF_LEN];

static void ble_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data) {
    ESP_LOGI(TAG, "rev bt event %ld", event_id);
    switch (event_id) {
        case BT_INIT:
            break;
        case BT_START_SCAN:
            scanning = true;
            page_manager_request_update(false);
            break;
        case BT_STOP_SCAN:
            scanning = false;
            page_manager_request_update(false);
            break;
        case BT_NEW_SCAN_RESULT:
            page_manager_request_update(false);
            break;
    }
}

void ble_device_page_on_create(void *arg) {
    ESP_LOGI(TAG, "on_create");

    esp_event_handler_register_with(event_loop_handle,
                                    BLE_DEVICE_EVENT, ESP_EVENT_ANY_ID,
                                    ble_event_handler, NULL);

    //ble_device_config_t config;
    //ble_device_init(NULL);
}

void ble_device_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);

    uint8_t scan_rst_count = 0;
    scan_result_t *scan_rst = ble_device_get_scan_rst(&scan_rst_count);
    total_item_count = scan_rst_count + 1;
    ESP_LOGI(TAG, "total item count %d", total_item_count);

    uint8_t y = 0;
    uint8_t index = 0;
    for (int16_t i = offset_item; i < total_item_count; ++i) {
        if (i == 0) {
            if (scanning) {
                // 搜索中...
                uint8_t search_txt[] = {0xCB, 0xD1, 0xCB, 0xF7, 0xD6, 0xD0, 0x2E, 0x2E, 0x2E, 0x00};
                epd_paint_draw_string_at(epd_paint, ITEM_HEIGHT + PADDING_X, y + TEXT_PADDING_Y,
                                         (char *) search_txt, &Font_HZK16, 1);
            } else {
                // 搜索设备
                uint8_t search_txt[] = {0xCB, 0xD1, 0xCB, 0xF7, 0xC9, 0xE8, 0xB1, 0xB8, 0x00};
                epd_paint_draw_string_at(epd_paint, ITEM_HEIGHT + PADDING_X, y + TEXT_PADDING_Y,
                                         (char *) search_txt, &Font_HZK16, 1);
            }
            y += ITEM_HEIGHT;
            epd_paint_draw_horizontal_line(epd_paint, 0, y, LCD_H_RES, 1);
        } else {
            index = i - offset_item - 1;
            scan_result_t d = scan_rst[index];
            memcpy(buff, d.dev_name, min(d.dev_name_len, BUFF_LEN - 1));
            buff[d.dev_name_len] = '\0';

            epd_paint_draw_string_at(epd_paint, PADDING_X, y + TEXT_PADDING_Y,
                                     buff, &Font_HZK16, 1);
            y += ITEM_HEIGHT;
            epd_paint_draw_horizontal_line(epd_paint, 0, y, LCD_H_RES, 1);
        }
    }

    // select item
    uint16_t start_y = current_index * ITEM_HEIGHT - offset_item * ITEM_HEIGHT;
    epd_paint_reverse_range(epd_paint, 0, start_y + 2, LCD_H_RES, ITEM_HEIGHT - 3);
}

void ble_device_page_after_draw(uint32_t loop_cnt) {
    switching_index = false;
}

static void change_select(bool next) {
    switching_index = true;
    current_index = (current_index + (next ? 1 : -1)) % total_item_count;
    if (current_index < 0) {
        current_index += total_item_count;
    }

    int16_t select_start_y = current_index * ITEM_HEIGHT - offset_item * ITEM_HEIGHT;
    int16_t select_end_y = select_start_y + ITEM_HEIGHT;

    if (select_start_y < 0) {
        offset_item -= (0 - select_start_y + ITEM_HEIGHT - 1) / ITEM_HEIGHT;
    } else if (select_end_y > LCD_V_RES) {
        offset_item += (select_end_y - LCD_V_RES + ITEM_HEIGHT - 1) / ITEM_HEIGHT;
    }

    ESP_LOGI(TAG, "current index:%d, current offset:%d", current_index, offset_item);

    int full_update = 0;
    common_post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, &full_update, sizeof(full_update));
}

static void handle_click_event() {
    if (current_index == 0) {
        // refresh click
        ble_device_init(NULL);
        ble_device_start_scan(10);
    } else {
        // bt item click try to connect bt device
        uint8_t scan_rst_count = 0;
        scan_result_t *scan_rst = ble_device_get_scan_rst(&scan_rst_count);
        ble_device_connect(scan_rst[current_index - 1].addr);
    }
    page_manager_request_update(false);
}

bool ble_device_page_key_click(key_event_id_t key_event_type) {
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

void ble_device_page_on_destroy(void *arg) {
    ble_device_deinit();
}

int ble_device_page_on_enter_sleep(void *args) {
    return -1;
}

#endif