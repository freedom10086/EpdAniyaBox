#ifndef BLE_DEVICE_H
#define BLE_DEVICE_H

#include "esp_bt.h"
#include "ble_csc.h"
#include "ble_hrm.h"
#include "esp_gattc_api.h"

ESP_EVENT_DECLARE_BASE(BLE_DEVICE_EVENT);

#define MAX_BLE_DEVICE_NAME_LEN 20

typedef struct {

} ble_device_config_t;

typedef struct {
    uint8_t dev_name[MAX_BLE_DEVICE_NAME_LEN];
    int dev_name_len;
    esp_bd_addr_t bda;
    esp_ble_addr_type_t ble_addr_type;
    int rssi;
} scan_result_t;

typedef enum {
    BT_INIT,
    BT_START_SCAN,
    BT_STOP_SCAN,
    BT_NEW_SCAN_RESULT,
} my_bt_event_id_t;

esp_err_t ble_device_init(const ble_device_config_t *config);

esp_err_t ble_device_start_scan(uint8_t duration);

scan_result_t *ble_device_get_scan_rst(uint8_t *result_count);

esp_err_t ble_device_deinit(esp_event_loop_handle_t hdl);

#endif