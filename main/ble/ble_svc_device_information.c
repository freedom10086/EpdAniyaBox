//
// Created by yang on 2023/7/28.
//

#include "ble_svc_device_information.h"

uint16_t char_uuid_list[] = {
        BLE_UUID_CHAR_Manufacturer_Name,
        BLE_UUID_CHAR_Model_Number,
        BLE_UUID_CHAR_Serial_Number,
        BLE_UUID_CHAR_Hardware_Revision,
        BLE_UUID_CHAR_Firmware_Revision,
        BLE_UUID_CHAR_Software_Revision,
        BLE_UUID_CHAR_System_ID,
        BLE_UUID_CHAR_PnP_ID,
        BLE_UUID_END
};

static int read_char(const struct peer *peer, int idx);

static int ble_on_device_info_read(uint16_t conn_handle,
                                   const struct ble_gatt_error *error,
                                   struct ble_gatt_attr *attr,
                                   void *arg) {
    int idx = (int) arg;
    ESP_LOGI(GATTC_TAG, "Read dev information complete; status=%d conn_handle=%d idx=%d", error->status, conn_handle,
             idx);
    if (error->status == 0) {
        ESP_LOGI(GATTC_TAG, "attr_handle=%d value was %.*s\n", attr->handle, attr->om->om_len, attr->om->om_data);
    }
    ESP_LOGI(GATTC_TAG, "\n");
    const struct peer *peer = peer_find(conn_handle);
    // read next char
    read_char(peer, idx + 1);
    return 0;
}

static int read_char(const struct peer *peer, int idx) {
    const struct peer_chr *chr;
    int rc;

    do {
        uint16_t char_uuid = char_uuid_list[idx];
        if (char_uuid == BLE_UUID_END) {
            // arrive end
            return 0;
        }
        chr = peer_chr_find_uuid(peer,
                                 BLE_UUID16_DECLARE(BLE_SVC_DEVICE_INFORMATION_UUID),
                                 BLE_UUID16_DECLARE(char_uuid));
        idx++;
    } while (chr == NULL);

    if (chr != NULL) {
        rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle, ble_on_device_info_read,
                            (void *) (idx - 1));
        if (rc != 0) {
            ESP_LOGE(GATTC_TAG, "Error: Failed to read characteristic; rc=%d\n", rc);
        }

        return rc;
    }

    return 0;
}

int ble_dev_info_service_handle_peer(const struct peer *peer) {
    read_char(peer, 0);
    return 0;
}