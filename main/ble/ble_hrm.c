#include "esp_log.h"
#include "esp_gattc_api.h"
#include "ble_hrm.h"

#define TAG "BLE_HR"

ESP_EVENT_DEFINE_BASE(BIKE_BLE_HRM_SENSOR_EVENT);

// ble hrm event data
static ble_hrm_data_t d;

void ble_parse_hrm_data(char *device_name, esp_ble_gattc_cb_param_t *p_data) {
    uint8_t flag = (*p_data->notify.value) & 0x01;
    p_data->notify.value += 1;

    uint16_t hrValue;
    if (flag) {
        hrValue = *((uint16_t *) p_data->notify.value);
    } else {
        hrValue = *((uint8_t *) p_data->notify.value);
    }

    d.heart_rate = hrValue;
    esp_event_post_to(event_loop_handle, BIKE_BLE_HRM_SENSOR_EVENT, BLE_HRM_SENSOR_UPDATE,
                      &d, sizeof(ble_hrm_data_t), 100 / portTICK_PERIOD_MS);
    //ESP_LOGI(TAG, "heart rate: %d", hrValue);
}