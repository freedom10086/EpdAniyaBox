//
// Created by yang on 2023/7/27.
//

#include "ble_svc_csc.h"
#include "ble_csc.h"
#include "common_utils.h"

static int ble_on_csc_on_subscribe(uint16_t conn_handle,
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

static int ble_on_csc_read(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           struct ble_gatt_attr *attr,
                           void *arg) {
    ESP_LOGI(GATTC_TAG, "Read csc complete; status=%d conn_handle=%d", error->status, conn_handle);
    if (error->status == 0) {
        ESP_LOGI(GATTC_TAG, " attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om);
    }
    ESP_LOGI(GATTC_TAG, "\n");

    const struct peer_dsc *dsc;
    int rc;
    uint8_t value[2];

    const struct peer *peer = peer_find(conn_handle);
    dsc = peer_dsc_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLE_SVC_CSC_UUID),
                             BLE_UUID16_DECLARE(BLE_UUID_CHAR_CSC_MSRMT),
                             BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL) {
        ESP_LOGE(GATTC_TAG, "Error: Peer lacks a CCCD for the subscribable characterstic\n");
        goto err;
    }

    /*** Write 0x00 and 0x01 (The subscription code) to the CCCD ***/
    value[0] = 1;
    value[1] = 0;
    rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle,
                              value, sizeof(value), ble_on_csc_on_subscribe, NULL);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG,
                 "Error: Failed to subscribe to the subscribable characteristic; rc=%d\n", rc);
        goto err;
    }

    return rc;
    err:
    /* Terminate the connection */
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    return BLE_ERR_REM_USER_CONN_TERM;
}

int ble_csc_service_handle_peer(const struct peer *peer) {
    const struct peer_chr *chr;
    int rc;

    chr = peer_chr_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLE_SVC_CSC_UUID),
                             BLE_UUID16_DECLARE(BLE_UUID_CHAR_CSC_FEATURE));
    if (chr != NULL) {
        rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle, ble_on_csc_read, NULL);
        if (rc != 0) {
            ESP_LOGE(GATTC_TAG, "Error: Failed to read characteristic; rc=%d\n", rc);
        }

        return rc;
    }

    return 0;
}

int ble_csc_handle_notification(const struct peer *peer, struct os_mbuf *om,
                                uint16_t attr_handle,
                                uint16_t conn_handle,
                                uint8_t indication) {
    if (indication) {
        return 0;
    }

    ble_parse_csc_data("CSC", om->om_data);
    return 0;
}