#ifndef __BLE_DEVICE_H
#define __BLE_DEVICE_H

#include "esp_event_base.h"
#include "esp_types.h"
#include "esp_event.h"
#include "esp_err.h"
#include "driver/uart.h"

#include "ble_csc.h"
#include "ble_hrm.h"

ESP_EVENT_DECLARE_BASE(BLE_DEVICE_EVENT);

typedef struct {

} ble_device_config_t;

esp_err_t ble_device_init(const ble_device_config_t *config);

esp_err_t ble_device_deinit(esp_event_loop_handle_t hdl);

#endif