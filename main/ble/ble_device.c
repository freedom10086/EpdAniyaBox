#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "esp_bt.h"
#include "esp_log.h"

#include "ble_csc.h"
#include "ble_device.h"
#include "bike_common.h"

#include "esp_log.h"
#include "nvs_flash.h"
/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

#define GATTC_TAG "BLE_DEVICE"
#define MAX_SCAN_RESULT_COUNT 30

ESP_EVENT_DEFINE_BASE(BLE_DEVICE_EVENT);

static bool scanning = false;
static bool inited = false;

static uint8_t scan_result_count = 0;
static scan_result_t scan_rst_list[MAX_SCAN_RESULT_COUNT];

//static struct service_char_map_t service_char_map[] = {
//        {.service_uuid = ESP_GATT_UUID_CYCLING_SPEED_CADENCE_SVC, .char_uuid = CSC_MEASUREMENT_CHARACTERISTIC, .en_notify = true},
//        {.service_uuid = ESP_GATT_UUID_BATTERY_SERVICE_SVC, .char_uuid = BATTERY_LEVEL_CHARACTERISTIC_UUID, .en_notify = true, .en_read = true},
//        {.service_uuid = ESP_GATT_UUID_HEART_RATE_SVC, .char_uuid = ESP_GATT_HEART_RATE_MEAS, .en_notify = true},
//        {.service_uuid = 0x0000,} // stop flag
//};

//struct gattc_service_inst {
//    ble_uuid16_t uuid;
//    uint16_t service_start_handle;
//    uint16_t service_end_handle;
//    uint16_t char_handle;
//};
//
//struct gattc_profile_inst {
//    char *device_name;
//    esp_gattc_cb_t gattc_cb;
//    uint16_t gattc_if;
//    uint16_t conn_id;
//    esp_bd_addr_t remote_bda;
//    bool connect;
//
//    struct gattc_service_inst services[10];
//    uint16_t service_count;
//};

/*** The UUID of the service containing the subscribable characterstic ***/
static const ble_uuid_t *remote_svc_uuid =
        BLE_UUID128_DECLARE(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
                            0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);

/*** The UUID of the subscribable chatacteristic ***/
static const ble_uuid_t *remote_chr_uuid =
        BLE_UUID128_DECLARE(0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11,
                            0x22, 0x22, 0x22, 0x22, 0x33, 0x33, 0x33, 0x33);

static int ble_gap_event(struct ble_gap_event *event, void *arg);

static uint8_t peer_addr[6];

/**
 * Application Callback. Called when the custom subscribable chatacteristic
 * in the remote GATT server is read.
 * Expect to get the recently written data.
 **/
static int ble_on_custom_read(uint16_t conn_handle,
                                  const struct ble_gatt_error *error,
                                  struct ble_gatt_attr *attr,
                                  void *arg) {
    ESP_LOGI(GATTC_TAG,
             "Read complete for the subscribable characteristic; "
             "status=%d conn_handle=%d", error->status, conn_handle);
    if (error->status == 0) {
        ESP_LOGI(GATTC_TAG, " attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om);
    }
    ESP_LOGI(GATTC_TAG, "\n");

    return 0;
}

/**
 * Application Callback. Called when the custom subscribable characteristic
 * in the remote GATT server is written to.
 * Client has previously subscribed to this characeteristic,
 * so expect a notification from the server.
 **/
static int ble_on_custom_write(uint16_t conn_handle,
                                   const struct ble_gatt_error *error,
                                   struct ble_gatt_attr *attr,
                                   void *arg) {
    const struct peer_chr *chr;
    const struct peer *peer;
    int rc;

    ESP_LOGI(GATTC_TAG,
             "Write to the custom subscribable characteristic complete; "
             "status=%d conn_handle=%d attr_handle=%d\n",
             error->status, conn_handle, attr->handle);

    peer = peer_find(conn_handle);
    chr = peer_chr_find_uuid(peer, remote_svc_uuid, remote_chr_uuid);
    if (chr == NULL) {
        ESP_LOGE(GATTC_TAG,
                 "Error: Peer doesn't have the custom subscribable characteristic\n");
        goto err;
    }

    /*** Performs a read on the characteristic, the result is handled in ble_on_new_read callback ***/
    rc = ble_gattc_read(conn_handle, chr->chr.val_handle,ble_on_custom_read, NULL);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG,
                 "Error: Failed to read the custom subscribable characteristic; "
                 "rc=%d\n", rc);
        goto err;
    }

    return 0;
    err:
    /* Terminate the connection */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Application Callback. Called when the custom subscribable characteristic
 * is subscribed to.
 **/
static int ble_on_custom_subscribe(uint16_t conn_handle,
                                       const struct ble_gatt_error *error,
                                       struct ble_gatt_attr *attr,
                                       void *arg) {
    const struct peer_chr *chr;
    uint8_t value;
    int rc;
    const struct peer *peer;

    ESP_LOGI(GATTC_TAG,
             "Subscribe to the custom subscribable characteristic complete; "
             "status=%d conn_handle=%d", error->status, conn_handle);

    if (error->status == 0) {
        ESP_LOGI(GATTC_TAG, " attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om);
    }
    ESP_LOGI(GATTC_TAG, "\n");

    peer = peer_find(conn_handle);
    chr = peer_chr_find_uuid(peer,
                             remote_svc_uuid,
                             remote_chr_uuid);
    if (chr == NULL) {
        ESP_LOGE(GATTC_TAG, "Error: Peer doesn't have the subscribable characteristic\n");
        goto err;
    }

    /* Write 1 byte to the new characteristic to test if it notifies after subscribing */
    value = 0x19;
    rc = ble_gattc_write_flat(conn_handle, chr->chr.val_handle,
                              &value, sizeof(value), ble_on_custom_write, NULL);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG,
                 "Error: Failed to write to the subscribable characteristic; "
                 "rc=%d\n", rc);
        goto err;
    }

    return 0;
    err:
    /* Terminate the connection */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Performs 3 operations on the remote GATT server.
 * 1. Subscribes to a characteristic by writing 0x10 to it's CCCD.
 * 2. Writes to the characteristic and expect a notification from remote.
 * 3. Reads the characteristic and expect to get the recently written information.
 **/
static void ble_custom_gatt_operations(const struct peer *peer) {
    const struct peer_dsc *dsc;
    int rc;
    uint8_t value[2];

    dsc = peer_dsc_find_uuid(peer,
                             remote_svc_uuid,
                             remote_chr_uuid,
                             BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL) {
        ESP_LOGE(GATTC_TAG, "Error: Peer lacks a CCCD for the subscribable characterstic\n");
        goto err;
    }

    /*** Write 0x00 and 0x01 (The subscription code) to the CCCD ***/
    value[0] = 1;
    value[1] = 0;
    rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle,
                              value, sizeof(value), ble_on_custom_subscribe, NULL);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG,
                 "Error: Failed to subscribe to the subscribable characteristic; "
                 "rc=%d\n", rc);
        goto err;
    }

    return;
    err:
    /* Terminate the connection */
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Application callback.  Called when the attempt to subscribe to notifications
 * for the ANS Unread Alert Status characteristic has completed.
 */
static int ble_on_subscribe(uint16_t conn_handle,
                     const struct ble_gatt_error *error,
                     struct ble_gatt_attr *attr,
                     void *arg) {
    struct peer *peer;

    ESP_LOGI(GATTC_TAG, "Subscribe complete; status=%d conn_handle=%d "
                        "attr_handle=%d\n",
             error->status, conn_handle, attr->handle);

    peer = peer_find(conn_handle);
    if (peer == NULL) {
        ESP_LOGE(GATTC_TAG, "Error in finding peer, aborting...");
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    /* Subscribe to, write to, and read the custom characteristic*/
    ble_custom_gatt_operations(peer);

    return 0;
}

/**
 * Application callback.  Called when the write to the ANS Alert Notification
 * Control Point characteristic has completed.
 */
static int ble_on_write(uint16_t conn_handle,
                        const struct ble_gatt_error *error,
                        struct ble_gatt_attr *attr,
                        void *arg) {
    ESP_LOGI(GATTC_TAG,
             "Write complete; status=%d conn_handle=%d attr_handle=%d\n",
             error->status, conn_handle, attr->handle);

    /* Subscribe to notifications for the Unread Alert Status characteristic.
     * A central enables notifications by writing two bytes (1, 0) to the
     * characteristic's client-characteristic-configuration-descriptor (CCCD).
     */
    const struct peer_dsc *dsc;
    uint8_t value[2];
    int rc;
    const struct peer *peer = peer_find(conn_handle);

    dsc = peer_dsc_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLECENT_SVC_ALERT_UUID),
                             BLE_UUID16_DECLARE(BLECENT_CHR_UNR_ALERT_STAT_UUID),
                             BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL) {
        ESP_LOGE(GATTC_TAG, "Error: Peer lacks a CCCD for the Unread Alert "
                            "Status characteristic\n");
        goto err;
    }

    value[0] = 1;
    value[1] = 0;
    rc = ble_gattc_write_flat(conn_handle, dsc->dsc.handle,
                              value, sizeof value, ble_on_subscribe, NULL);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG, "Error: Failed to subscribe to characteristic; "
                            "rc=%d\n", rc);
        goto err;
    }

    return 0;
    err:
    /* Terminate the connection. */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Application callback.  Called when the read of the ANS Supported New Alert
 * Category characteristic has completed.
 */
static int ble_on_read(uint16_t conn_handle,
                       const struct ble_gatt_error *error,
                       struct ble_gatt_attr *attr,
                       void *arg) {
    ESP_LOGI(GATTC_TAG, "Read complete; status=%d conn_handle=%d", error->status,
             conn_handle);
    if (error->status == 0) {
        ESP_LOGI(GATTC_TAG, " attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om);
    }
    ESP_LOGI(GATTC_TAG, "\n");

    /* Write two bytes (99, 100) to the alert-notification-control-point
     * characteristic.
     */
    const struct peer_chr *chr;
    uint8_t value[2];
    int rc;
    const struct peer *peer = peer_find(conn_handle);

    chr = peer_chr_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLECENT_SVC_ALERT_UUID),
                             BLE_UUID16_DECLARE(BLECENT_CHR_ALERT_NOT_CTRL_PT));
    if (chr == NULL) {
        ESP_LOGE(GATTC_TAG, "Error: Peer doesn't support the Alert "
                            "Notification Control Point characteristic\n");
        goto err;
    }

    value[0] = 99;
    value[1] = 100;
    rc = ble_gattc_write_flat(conn_handle, chr->chr.val_handle, value, sizeof value, ble_on_write, NULL);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG, "Error: Failed to write characteristic; rc=%d\n",
                 rc);
        goto err;
    }

    return 0;
    err:
    /* Terminate the connection. */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Performs three GATT operations against the specified peer:
 * 1. Reads the ANS Supported New Alert Category characteristic.
 * 2. After read is completed, writes the ANS Alert Notification Control Point characteristic.
 * 3. After write is completed, subscribes to notifications for the ANS Unread Alert Status
 *    characteristic.
 *
 * If the peer does not support a required service, characteristic, or
 * descriptor, then the peer lied when it claimed support for the alert
 * notification service!  When this happens, or if a GATT procedure fails,
 * this function immediately terminates the connection.
 */
static void ble_read_write_subscribe(const struct peer *peer) {
    const struct peer_chr *chr;
    int rc;

    /* Read the supported-new-alert-category characteristic. */
    chr = peer_chr_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLECENT_SVC_ALERT_UUID),
                             BLE_UUID16_DECLARE(BLECENT_CHR_SUP_NEW_ALERT_CAT_UUID));
    if (chr == NULL) {
        ESP_LOGE(GATTC_TAG, "Error: Peer doesn't support the Supported New "
                            "Alert Category characteristic\n");
        goto err;
    }

    rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle, ble_on_read, NULL);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG, "Error: Failed to read characteristic; rc=%d\n",
                 rc);
        goto err;
    }

    return;
    err:
    /* Terminate the connection. */
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Called when service discovery of the specified peer has completed.
 */
static void ble_on_disc_complete(const struct peer *peer, int status, void *arg) {

    if (status != 0) {
        /* Service discovery failed.  Terminate the connection. */
        ESP_LOGE(GATTC_TAG, "Error: Service discovery failed; status=%d "
                            "conn_handle=%d\n", status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    /* Service discovery has completed successfully.  Now we have a complete
     * list of services, characteristics, and descriptors that the peer
     * supports.
     */
    ESP_LOGI(GATTC_TAG, "Service discovery complete; status=%d "
                        "conn_handle=%d\n", status, peer->conn_handle);

    /* Now perform three GATT procedures against the peer: read,
     * write, and subscribe to notifications for the ANS service.
     */
    ble_read_write_subscribe(peer);
}


/**
 * Indicates whether we should try to connect to the sender of the specified
 * advertisement.  The function returns a positive result if the device
 * advertises connectability and support for the Alert Notification service.
 */
#if CONFIG_EXAMPLE_EXTENDED_ADV
static int
ext_ble_should_connect(const struct ble_gap_ext_disc_desc *disc)
{
    int offset = 0;
    int ad_struct_len = 0;

    if (disc->legacy_event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
            disc->legacy_event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
        return 0;
    }
    if (strlen(CONFIG_EXAMPLE_PEER_ADDR) && (strncmp(CONFIG_EXAMPLE_PEER_ADDR, "ADDR_ANY", strlen    ("ADDR_ANY")) != 0)) {
        ESP_LOGI(tag, "Peer address from menuconfig: %s", CONFIG_EXAMPLE_PEER_ADDR);
        /* Convert string to address */
        sscanf(CONFIG_EXAMPLE_PEER_ADDR, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &peer_addr[5], &peer_addr[4], &peer_addr[3],
               &peer_addr[2], &peer_addr[1], &peer_addr[0]);
        if (memcmp(peer_addr, disc->addr.val, sizeof(disc->addr.val)) != 0) {
            return 0;
        }
    }

    /* The device has to advertise support for the Alert Notification
    * service (0x1811).
    */
    do {
        ad_struct_len = disc->data[offset];

        if (!ad_struct_len) {
            break;
        }

    /* Search if ANS UUID is advertised */
        if (disc->data[offset] == 0x03 && disc->data[offset + 1] == 0x03) {
            if ( disc->data[offset + 2] == 0x18 && disc->data[offset + 3] == 0x11 ) {
                return 1;
            }
        }

        offset += ad_struct_len + 1;

     } while ( offset < disc->length_data );

    return 0;
}
#else

static int ble_should_connect(const struct ble_gap_disc_desc *disc) {
    struct ble_hs_adv_fields fields;
    int rc;
    int i;

    ESP_LOGI(GATTC_TAG, "event_type: %d", disc->event_type);
    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND
        && disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND
        && disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
        return 0;
    }

    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0) {
        return 0;
    }

    ESP_LOGI(GATTC_TAG, "device addr: %s", addr_str(disc->addr.val));
    if (fields.name != NULL && fields.name_len) {
        char s[BLE_HS_ADV_MAX_SZ];
        memcpy(s, fields.name, fields.name_len);
        s[fields.name_len] = '\0';
        ESP_LOGI(GATTC_TAG, "device name: %s", s);
    }

//    char * CONFIG_EXAMPLE_PEER_ADDR = "aa:bb:cc:dd:ee:ff";
//    if (strlen(CONFIG_EXAMPLE_PEER_ADDR)) {
//        ESP_LOGI(GATTC_TAG, "Peer address from menuconfig: %s", CONFIG_EXAMPLE_PEER_ADDR);
//        /* Convert string to address */
//        sscanf(CONFIG_EXAMPLE_PEER_ADDR, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
//               &peer_addr[5], &peer_addr[4], &peer_addr[3],
//               &peer_addr[2], &peer_addr[1], &peer_addr[0]);
//        if (memcmp(peer_addr, disc->addr.val, sizeof(disc->addr.val)) != 0) {
//            return 0;
//        }
//    }

    /* The device has to advertise support HRM service (0x1811).
     */
    for (i = 0; i < fields.num_uuids16; i++) {
        if (ble_uuid_u16(&fields.uuids16[i].u) == BLE_SVC_HRM_UUID) {
            return 1;
        }
    }

    return 0;
}

#endif

/**
 * Connects to the sender of the specified advertisement of it looks
 * interesting.  A device is "interesting" if it advertises connectability and
 * support for the Alert Notification service.
 */
static void ble_connect_if_interesting(void *disc) {
    uint8_t own_addr_type;
    int rc;
    ble_addr_t *addr;

    /* Don't do anything if we don't care about this advertiser. */
#if CONFIG_EXAMPLE_EXTENDED_ADV
    if (!ext_ble_should_connect((struct ble_gap_ext_disc_desc *)disc)) {
        return;
    }
#else
    if (!ble_should_connect((struct ble_gap_disc_desc *) disc)) {
        return;
    }
#endif

    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        ESP_LOGD(GATTC_TAG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }

    /* Figure out address to use for connect (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for
     * timeout.
     */
#if CONFIG_EXAMPLE_EXTENDED_ADV
    addr = &((struct ble_gap_ext_disc_desc *)disc)->addr;
#else
    addr = &((struct ble_gap_disc_desc *) disc)->addr;
#endif

    common_post_event(BLE_DEVICE_EVENT, BT_START_CONNECT);
    rc = ble_gap_connect(own_addr_type, addr, 30000, NULL,
                         ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG, "Error: Failed to connect to device; addr_type=%d "
                            "addr=%s; rc=%d\n",
                 addr->type, addr_str(addr->val), rc);
        return;
    }
}

#if MYNEWT_VAL(BLE_POWER_CONTROL)
static void blecent_power_control(uint16_t conn_handle)
{
    int rc;

    rc = ble_gap_read_remote_transmit_power_level(conn_handle, 0x01 );  // Attempting on LE 1M phy
    assert (rc == 0);

    rc = ble_gap_set_transmit_power_reporting_enable(conn_handle, 0x01, 0x01);
    assert (rc == 0);

    rc = ble_gap_set_path_loss_reporting_param(conn_handle, 60, 10, 30, 10, 2 ); //demo values
    assert (rc == 0);

    rc = ble_gap_set_path_loss_reporting_enable(conn_handle, 0x01);
    assert (rc == 0);
}
#endif

/**
 * @return  0 if the application successfully handled the event; nonzero on failure.
 */
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;

    if (event->type != BLE_GAP_EVENT_DISC) {
        ESP_LOGI(GATTC_TAG, "rev ble_gap_event %d", event->type);
    }
    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            // 主动扫描回复包
            if (event->disc.event_type != BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
                return 0;
            }

            // 扫描到广播数据
            rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
            if (rc != 0) {
                return 0;
            }

            if (fields.name == NULL || !fields.name_len) {
                // no device name
                return 0;
            }

            /* An advertisment report was received during GAP discovery. */
            print_adv_fields(&fields);

            // common_post_event(BLE_DEVICE_EVENT, BT_NEW_SCAN_RESULT);

            /* Try to connect to the advertiser if it looks interesting. */
            ble_connect_if_interesting(&event->disc);
            return 0;

        case BLE_GAP_EVENT_CONNECT:
            /* A new connection was established or a connection attempt failed. */
            if (event->connect.status == 0) {
                /* Connection successfully established. */
                ESP_LOGI(GATTC_TAG, "Connection established ");

                common_post_event(BLE_DEVICE_EVENT, BT_CONNECT_SUCCESS);

                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                assert(rc == 0);
                print_conn_desc(&desc);
                ESP_LOGI(GATTC_TAG, "\n");

                /* Remember peer. */
                rc = peer_add(event->connect.conn_handle);
                if (rc != 0) {
                    ESP_LOGE(GATTC_TAG, "Failed to add peer; rc=%d\n", rc);
                    return 0;
                }

#if MYNEWT_VAL(BLE_POWER_CONTROL)
                blecent_power_control(event->connect.conn_handle);
#endif

#if MYNEWT_VAL(BLE_HCI_VS)
#if MYNEWT_VAL(BLE_POWER_CONTROL)
                int8_t vs_cmd[10]= {0, 0,-70,-60,-68,-58,-75,-65,-80,-70};

                vs_cmd[0] = ((uint8_t)(event->connect.conn_handle & 0xFF));
                vs_cmd[1] = ((uint8_t)(event->connect.conn_handle >> 8) & 0xFF);

                rc = ble_hs_hci_send_vs_cmd(BLE_HCI_OCF_VS_PCL_SET_RSSI ,
                                            &vs_cmd, sizeof(vs_cmd), NULL, 0);
                if (rc != 0) {
                    ESP_LOGI(GATTC_TAG, "Failed to send VSC  %x \n", rc);
                    return 0;
                }
                else
                    ESP_LOGI(GATTC_TAG, "Successfully issued VSC , rc = %d \n", rc);
#endif
#endif

                /* Perform service discovery */
                rc = peer_disc_all(event->connect.conn_handle, ble_on_disc_complete, NULL);
                if (rc != 0) {
                    ESP_LOGE(GATTC_TAG, "Failed to discover services; rc=%d\n", rc);
                    return 0;
                }
            } else {
                /* Connection attempt failed; resume scanning. */
                ESP_LOGE(GATTC_TAG, "Error: Connection failed; status=%d\n", event->connect.status);

                common_post_event(BLE_DEVICE_EVENT, BT_CONNECT_FAILED);

                ble_device_start_scan(10);
            }

            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            /* Connection terminated. */
            ESP_LOGI(GATTC_TAG, "disconnect; reason=%d ", event->disconnect.reason);
            print_conn_desc(&event->disconnect.conn);
            ESP_LOGI(GATTC_TAG, "\n");

            /* Forget about peer. */
            peer_delete(event->disconnect.conn.conn_handle);

            /* Resume scanning. */
            ble_device_start_scan(10);
            return 0;

        case BLE_GAP_EVENT_DISC_COMPLETE:
            // 搜索结束调用
            ESP_LOGI(GATTC_TAG, "discovery complete; reason=%d\n", event->disc_complete.reason);
            common_post_event(BLE_DEVICE_EVENT, BT_STOP_SCAN);
            scanning = false;
            return 0;
        case BLE_GAP_EVENT_ENC_CHANGE:
            /* Encryption has been enabled or disabled for this connection. */
            ESP_LOGI(GATTC_TAG, "encryption change event; status=%d ",
                     event->enc_change.status);
            rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            assert(rc == 0);
            print_conn_desc(&desc);
            return 0;

        case BLE_GAP_EVENT_NOTIFY_RX:
            /* Peer sent us a notification or indication. */
            ESP_LOGI(GATTC_TAG, "received %s; conn_handle=%d attr_handle=%d "
                                "attr_len=%d\n",
                     event->notify_rx.indication ?
                     "indication" :
                     "notification",
                     event->notify_rx.conn_handle,
                     event->notify_rx.attr_handle,
                     OS_MBUF_PKTLEN(event->notify_rx.om));

            /* Attribute data is contained in event->notify_rx.om. Use
             * `os_mbuf_copydata` to copy the data received in notification mbuf */
            return 0;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(GATTC_TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                     event->mtu.conn_handle,
                     event->mtu.channel_id,
                     event->mtu.value);
            return 0;

        case BLE_GAP_EVENT_REPEAT_PAIRING:
            /* We already have a bond with the peer, but it is attempting to
             * establish a new secure link.  This app sacrifices security for
             * convenience: just throw away the old bond and accept the new link.
             */

            /* Delete the old bond. */
            rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            assert(rc == 0);
            ble_store_util_delete_peer(&desc.peer_id_addr);

            /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
             * continue with the pairing operation.
             */
            return BLE_GAP_REPEAT_PAIRING_RETRY;

#if CONFIG_EXAMPLE_EXTENDED_ADV
            case BLE_GAP_EVENT_EXT_DISC:
        /* An advertisment report was received during GAP discovery. */
        ext_print_adv_report(&event->disc);

        blecent_connect_if_interesting(&event->disc);
        return 0;
#endif

#if MYNEWT_VAL(BLE_POWER_CONTROL)
            case BLE_GAP_EVENT_TRANSMIT_POWER:
    ESP_LOGI(GATTC_TAG, "Transmit power event : status=%d conn_handle=%d reason=%d "
                          "phy=%d power_level=%x power_level_flag=%d delta=%d",
            event->transmit_power.status,
            event->transmit_power.conn_handle,
            event->transmit_power.reason,
            event->transmit_power.phy,
            event->transmit_power.transmit_power_level,
            event->transmit_power.transmit_power_level_flag,
            event->transmit_power.delta);
    return 0;

    case BLE_GAP_EVENT_PATHLOSS_THRESHOLD:
    ESP_LOGI(GATTC_TAG, "Pathloss threshold event : conn_handle=%d current path loss=%d "
                          "zone_entered =%d",
            event->pathloss_threshold.conn_handle,
            event->pathloss_threshold.current_path_loss,
            event->pathloss_threshold.zone_entered);
    return 0;
#endif
        default:
            return 0;
    }
}

esp_err_t ble_device_start_scan(uint8_t duration) {
    if (scanning) {
        return ESP_OK;
    }

    scanning = true;

    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG, "error determining address type; rc=%d\n", rc);
        return rc;
    }

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 1;

    /**
     * 主动扫描，主动扫描的设备会发送ScanReq给到广播设备要求更多数据
     * 被动扫描，仅仅接收广播，不会去主动要求更多数据
     */
    disc_params.passive = 0;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0;
    disc_params.window = 0;

    disc_params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
    disc_params.limited = 0;

    // BLE_HS_FOREVER
    rc = ble_gap_disc(own_addr_type, duration * 1000, &disc_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(GATTC_TAG, "Error initiating GAP discovery procedure; rc=%d\n", rc);
    }

    common_post_event(BLE_DEVICE_EVENT, BT_START_SCAN);

    return rc;
}

static void ble_on_reset(int reason) {
    ESP_LOGE(GATTC_TAG, "Resetting state; reason=%d\n", reason);
}

static void ble_on_sync(void) {
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin scanning for a peripheral to connect to. */
    ble_device_start_scan(15);
}

void ble_host_task(void *param) {
    ESP_LOGI(GATTC_TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

esp_err_t ble_device_init(const ble_device_config_t *config) {
    if (inited) {
        return ESP_OK;
    }

    int ret;
    ESP_ERROR_CHECK(common_init_nvs());

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(GATTC_TAG, "Failed to init nimble %d ", ret);
        return ret;
    }

    /* Configure the host. */
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Initialize data structures to track connected peers. */
    ret = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(ret == 0);

    /* Set the default device name. */
    ret = ble_svc_gap_device_name_set("esp32-ble");
    assert(ret == 0);

    /* XXX Need to have template for store */
    // ble_store_config_init();

    nimble_port_freertos_init(ble_host_task);

    common_post_event(BLE_DEVICE_EVENT, BT_INIT);
    inited = true;
    return ESP_OK;
}

scan_result_t *ble_device_get_scan_rst(uint8_t *result_count) {
    *result_count = scan_result_count;
    return scan_rst_list;
}

esp_err_t ble_device_connect() {
    return ESP_OK;
}

esp_err_t ble_device_deinit(esp_event_loop_handle_t hdl) {
    vTaskDelay(1000);

    ESP_LOGI(GATTC_TAG, "Deinit ble host");

    int rc = nimble_port_stop();
    if (rc == 0) {
        nimble_port_deinit();
    } else {
        ESP_LOGI(GATTC_TAG, "Nimble port stop failed, rc = %d", rc);
        return rc;
    }

    common_post_event(BLE_DEVICE_EVENT, BT_DEINIT);
    inited = false;
    return ESP_OK;
}