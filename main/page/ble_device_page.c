#include "ble_device_page.h"

#include "esp_log.h"

#include "lcd/epd_lcd_ssd1680.h"
#include "bike_common.h"
#include "static/static.h"
#include "page_manager.h"

#define TAG "ble_device_page"

#define ITEM_HEIGHT 40
#define PADDING_X 12
#define PADDING_Y 4
#define TEXT_PADDING_Y 11

static bool switching_index = false;
static int16_t current_index = 0;
static int16_t offset_item = 0;

static uint8_t total_item_count = 1;

typedef struct item_data {
    uint8_t *icon;
    uint8_t text[20];
    struct item_data *next;
} item_data_t;

static item_data_t *datas;

void ble_device_page_on_create(void *arg) {
    ESP_LOGI(TAG, "on_create");
}

void ble_device_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);

    uint8_t y = 0;
    uint8_t index = 0;
    for (int16_t i = offset_item; i < total_item_count; ++i) {
        if (i == 0) {

        } else {
            item_data_t *d = datas;
            index = i - offset_item - 1;
            while (index > 0) {
                d = d->next;
                index--;
            }

            epd_paint_draw_string_at(epd_paint, ITEM_HEIGHT + PADDING_X, y + TEXT_PADDING_Y,
                                     (char *) d->text, &Font_HZK16, 1);
            y += ITEM_HEIGHT;
        }

        if (i > 0 && i != total_item_count - 1) {
            epd_paint_draw_horizontal_line(epd_paint, 0, ITEM_HEIGHT * (i - offset_item), LCD_H_RES, 1);
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
        page_manager_switch_page("image-manage");
    } else if (current_index == 4) {
        page_manager_switch_page("upgrade");
    } else if (current_index == 5) {
        page_manager_switch_page("ble-device");
    } else if (current_index == 6) {
        esp_restart();
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

}

int ble_device_page_on_enter_sleep(void *args) {
    return -1;
}