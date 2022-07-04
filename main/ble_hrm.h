#ifndef __BLE_HRM_H
#define __BLE_HRM_H

#include "esp_event_base.h"
#include "esp_types.h"
#include "esp_event.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "esp_gattc_api.h"

#define HEART_RATE_SERVICE_UUID 0x180D // Cycling Speed and Cadence
#define HEART_RATE_MEASUREMENT_CHARACTERISTIC 0x2A37
#define HR_SENSOR_LOCATION_CHARACTERISTIC_UUID 0x2A38

static struct csc_measure_sensor {
    uint16_t heart_rate;
} hr_measure_sensor_t;

void ble_parse_hr_data(esp_ble_gattc_cb_param_t *p_data);

#endif