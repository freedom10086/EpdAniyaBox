#include "esp_log.h"
#include "esp_gattc_api.h"
#include "ble_hrm.h"

#define TAG "BLE_HR"


void ble_parse_hr_data(esp_ble_gattc_cb_param_t *p_data) {
    uint8_t flag = (*p_data->notify.value) & 0x01;
    p_data->notify.value += 1;

    uint16_t hrValue;
    if (flag) {
        hrValue = *((uint16_t *) p_data->notify.value);
    } else {
        hrValue = *((uint8_t *) p_data->notify.value);
    }

    ESP_LOGI(TAG, "heart rate: %d", hrValue);
}