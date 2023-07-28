//
// Created by yang on 2023/7/28.
//

#include "ble_svc_hrm.h"
#include "ble_hrm.h"

static int ble_on_hrm_on_subscribe(uint16_t conn_handle,
                                   const struct ble_gatt_error *error,
                                   struct ble_gatt_attr *attr,
                                   void *arg) {
    ESP_LOGI(GATTC_TAG,
             "Subscribe to the custom subscribable characteristic complete; "
             "status=%d conn_handle=%d", error->status, conn_handle);

    if (error->status == 0) {
        ESP_LOGI(GATTC_TAG, " attr_handle=%d value=", attr->handle);
    }
    ESP_LOGI(GATTC_TAG, "\n");
    return 0;
}

int ble_hrm_service_handle_notification(const struct peer *peer, struct os_mbuf *om,
                                        uint16_t attr_handle,
                                        uint16_t conn_handle,
                                        uint8_t indication) {
    if (indication) {
        return 0;
    }

    ble_parse_hrm_data("HRM", om->om_data);
    return 0;
}

int ble_hrm_service_handle_peer(const struct peer *peer) {
    const struct peer_chr *chr;
    int rc;

    uint8_t value[2];
    struct peer_dsc *dsc = peer_dsc_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLE_SVC_HRM_UUID),
                             BLE_UUID16_DECLARE(BLE_UUID_CHAR_Heart_Rate_Measurement),
                             BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL) {
        ESP_LOGE(GATTC_TAG, "Error: Peer lacks a CCCD for the subscribable characterstic\n");
    }

    /*** Write 0x00 and 0x01 (The subscription code) to the CCCD ***/
    value[0] = 1;
    value[1] = 0;
    rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle,
                              value, sizeof(value), ble_on_hrm_on_subscribe, NULL);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG,
                 "Error: Failed to subscribe to the subscribable characteristic; rc=%d\n", rc);
    }

    return rc;
}