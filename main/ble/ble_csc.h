#ifndef __BLE_CSC_H
#define __BLE_CSC_H

#include "esp_event_base.h"
#include "esp_types.h"
#include "esp_event.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "esp_gattc_api.h"

#include "event_common.h"

#define CYCLING_SPEED_AND_CADENCE_SERVICE_UUID 0x1816 // Cycling Speed and Cadence
#define DEVICE_INFORMATION_SERVICE_UUID 0x180A
#define BATTERY_LEVEL_SERVICE_UUID 0x180F

#define BATTERY_LEVEL_CHARACTERISTIC_UUID 0x2A19 // read and notify
#define CSC_MEASUREMENT_CHARACTERISTIC 0x2A5B
#define CSC_FEATURE_CHARACTERISTIC 0x2A5C

extern esp_event_loop_handle_t event_loop_handle;

ESP_EVENT_DECLARE_BASE(BIKE_BLE_CSC_SENSOR_EVENT);

typedef struct {
    bool first_wheel_data_get;
    uint32_t first_wheel_revolutions;
    uint32_t last_wheel_revolutions;
    uint16_t last_wheel_event_time; // 1/1024s

    bool first_crank_data_get;
    uint16_t last_crank_revolutions;
    uint16_t last_crank_event_time; // 1/1024s
} csc_measure_sensor_t;

void ble_parse_csc_data(char *device_name, esp_ble_gattc_cb_param_t *p_data);

#endif