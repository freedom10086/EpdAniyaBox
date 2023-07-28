#include "esp_log.h"
#include "ble_hrm.h"

#define TAG "BLE_HR"

ESP_EVENT_DEFINE_BASE(BIKE_BLE_HRM_SENSOR_EVENT);

// ble hrm event data
static ble_hrm_data_t d;

void ble_parse_hrm_data(char *device_name, uint8_t *value) {
    uint8_t flag = (*value) & 0x01;
    value += 1;

    uint16_t hrValue;
    if (flag) {
        hrValue = *((uint16_t *) value);
    } else {
        hrValue = *((uint8_t *) value);
    }

    d.heart_rate = hrValue;
    esp_event_post_to(event_loop_handle, BIKE_BLE_HRM_SENSOR_EVENT, BLE_HRM_SENSOR_UPDATE,
                      &d, sizeof(ble_hrm_data_t), 100 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "heart rate: %d", hrValue);
}