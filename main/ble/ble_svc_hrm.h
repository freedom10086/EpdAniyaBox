//
// Created by yang on 2023/7/28.
//

#ifndef EPD_ANIYA_BOX_BLE_SVC_HRM_H
#define EPD_ANIYA_BOX_BLE_SVC_HRM_H

#include "esp_bt.h"
#include "ble_peer.h"
#include "ble_uuid.h"

#define GATTC_TAG "BLE_SVC_HRM"

int ble_hrm_service_handle_peer(const struct peer *peer);

int ble_hrm_service_handle_notification(const struct peer *peer, struct os_mbuf *om,
                                        uint16_t attr_handle,
                                        uint16_t conn_handle,
                                        uint8_t indication);

#endif //EPD_ANIYA_BOX_BLE_SVC_HRM_H
