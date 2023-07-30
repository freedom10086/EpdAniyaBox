//
// Created by yang on 2023/7/28.
//

#ifndef EPD_ANIYA_BOX_BLE_SVC_DEVICE_INFOMATION_H
#define EPD_ANIYA_BOX_BLE_SVC_DEVICE_INFOMATION_H

#include "esp_bt.h"
#include "modlog/modlog.h"
#include "ble_peer.h"
#include "ble_uuid.h"

#define GATTC_TAG "DEV_INFO_SVC"


int ble_dev_info_service_handle_peer(const struct peer *peer);


#endif //EPD_ANIYA_BOX_BLE_SVC_DEVICE_INFOMATION_H
