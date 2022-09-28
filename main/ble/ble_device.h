#ifndef BLE_DEVICE_H
#define BLE_DEVICE_H

#include "ble_csc.h"
#include "ble_hrm.h"

ESP_EVENT_DECLARE_BASE(BLE_DEVICE_EVENT);

typedef struct {

} ble_device_config_t;

esp_err_t ble_device_init(const ble_device_config_t *config);

esp_err_t ble_device_deinit(esp_event_loop_handle_t hdl);

#endif