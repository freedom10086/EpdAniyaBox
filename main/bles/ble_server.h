//
// Created by yang on 2023/7/28.
//

#ifndef EPD_ANIYA_BOX_BLE_SERVER_H
#define EPD_ANIYA_BOX_BLE_SERVER_H

#include <esp_err.h>
#include "bike_common.h"

esp_err_t ble_server_init();

esp_err_t ble_server_start_adv(uint8_t duration);

esp_err_t ble_server_stop_adv();

esp_err_t ble_server_deinit();

#endif //EPD_ANIYA_BOX_BLE_SERVER_H
