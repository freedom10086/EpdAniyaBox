#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <esp_log.h>

#include "bike_common.h"
#include "lcd/epdpaint.h"
#include "view/digi_view.h"
#include "view/battery_view.h"

#include "temperature_page.h"
#include "sht31.h"
#include "battery.h"
#include "bike_common.h"
#include "static/static.h"

#define TAG "temp-page"

// 温度
static uint16_t temp[] = {0xC2CE, 0xC8B6, 0x00};
// ℃
static uint16_t temp_f[] = {0xE6A1, 0x00};

// 湿度
static uint16_t hum[] = {0xAACA, 0xC8B6, 0x00};
// %
static uint16_t hum_f[] = {0x25, 0x00};

static float temperature;
static bool temperature_valid = false;

static float humility;
static bool humility_valid = false;

static void
ble_temp_sensor_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data) {
    sht31_data_t *data = NULL;
    switch (event_id) {
        case SHT31_SENSOR_UPDATE:
            data = (sht31_data_t *) event_data;
            ESP_LOGI(TAG, "temp: %f, hum: %f", data->temp, data->hum);
            temperature = data->temp;
            temperature_valid = true;

            humility = data->hum;
            humility_valid = true;
            break;
        case SHT31_SENSOR_READ_FAILED:
            ESP_LOGE(TAG, "read temp and hum failed!");
            break;
        default:
            break;
    }
}

void temperature_page_on_create(void *args) {
    ESP_LOGI(TAG, "=== on create ===");
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_TEMP_HUM_SENSOR_EVENT, ESP_EVENT_ANY_ID,
                                    ble_temp_sensor_event_handler, NULL);
    sht31_init();
}

void temperature_page_on_destroy(void *args) {
    ESP_LOGI(TAG, "=== on destroy ===");
    esp_event_handler_unregister_with(event_loop_handle,
                                      BIKE_TEMP_HUM_SENSOR_EVENT, ESP_EVENT_ANY_ID,
                                      ble_temp_sensor_event_handler);
}

void temperature_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    ESP_LOGI(TAG, "=== on draw ===");
    epd_paint_clear(epd_paint, 0);

    //epd_paint_draw_string_at(epd_paint, 167, 2, (char *)temp, &Font_HZK16, 1);
    digi_view_t *temp_label = digi_view_create(8, 24, 44, 7, 2);

    if (temperature_valid) {
        if (temperature > 100) {
            temperature = 100;
        }
        bool is_minus = temperature < 0;
        epd_paint_draw_string_at(epd_paint, 183, 24, (char *) temp_f, &Font_HZK16, 1);
        digi_view_set_text(temp_label, (int) temperature, (int) (temperature * 10 + (is_minus ? -0.5f : 0.5f)) % 10, 1);
        digi_view_draw(temp_label, epd_paint, loop_cnt);
    } else {
        digi_view_draw_ee(temp_label, epd_paint, 3, loop_cnt);
    }

    digi_view_deinit(temp_label);

    //epd_paint_draw_string_at(epd_paint, 167, 130, (char *)hum, &Font_HZK16, 1);
    digi_view_t *hum_label = digi_view_create(102, 144, 22, 3, 2);
    if (humility_valid) {
        if (humility < 0) {
            humility = 0;
        } else if (humility > 99) {
            humility = 99;
        }
        epd_paint_draw_string_at(epd_paint, 183, 172, (char *) hum_f, &Font_HZK16, 1);
        digi_view_set_text(hum_label, (int) humility, (int) (humility * 10 + 0.5f) % 10, 1);
        digi_view_draw(hum_label, epd_paint, loop_cnt);
    } else {
        digi_view_draw_ee(temp_label, epd_paint, 3, loop_cnt);
    }
    digi_view_deinit(hum_label);

    // battery
    battery_view_t *battery_view = battery_view_create(4, 183, 26, 16);
    battery_view_draw(battery_view, epd_paint, battery_get_level(), loop_cnt);
    battery_view_deinit(battery_view);

    // wifi icon
    epd_paint_draw_bitmap(epd_paint, 34, 183, 22, 16,
                          (uint8_t *) icon_wifi_bmp_start,
                          icon_wifi_bmp_end - icon_wifi_bmp_start, 1);

    // ble icon
    epd_paint_draw_bitmap(epd_paint, 60, 183, 11, 16,
                          (uint8_t *) icon_ble_bmp_start,
                          icon_ble_bmp_end - icon_ble_bmp_start, 1);

    // ble icon
    epd_paint_draw_bitmap(epd_paint, 75, 183, 16, 16,
                          (uint8_t *) icon_sat_bmp_start,
                          icon_sat_bmp_end - icon_sat_bmp_start, 1);
}

bool temperature_page_key_click_handle(key_event_id_t key_event_type) {
    return false;
}

