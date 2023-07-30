#ifndef BLE_DEVICE_H
#define BLE_DEVICE_H

#include "esp_bt.h"
#include "ble_csc.h"
#include "ble_hrm.h"
#include "ble_svc_battery_level.h"
#include "ble_svc_csc.h"
#include "ble_svc_device_information.h"
#include "ble_svc_hrm.h"
#include "modlog/modlog.h"
#include "ble_peer.h"

ESP_EVENT_DECLARE_BASE(BLE_DEVICE_EVENT);

#define MAX_BLE_DEVICE_NAME_LEN 20

#ifdef __cplusplus
extern "C" {
#endif

struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

#ifdef __cplusplus
}
#endif

typedef struct {

} ble_device_config_t;

typedef struct {
    uint8_t dev_name[MAX_BLE_DEVICE_NAME_LEN];
    int dev_name_len;
    ble_addr_t addr;
    int8_t rssi;
} scan_result_t;

typedef enum {
    BT_INIT,
    BT_START_SCAN,
    BT_STOP_SCAN,
    BT_NEW_SCAN_RESULT,
    BT_START_CONNECT, // 4
    BT_CONNECT_SUCCESS, // 5
    BT_CONNECT_FAILED,
    BT_DISCONNECT,
    BT_DEINIT,
} my_bt_event_id_t;

typedef int (*ble_service_handle_peer)(const struct peer *peer);
//* indication;
//*     o 0: Notification;
//*     o 1: Indication.
typedef int (*ble_service_handle_notification)(const struct peer *peer, struct os_mbuf *om,
                                               uint16_t attr_handle,
                                               uint16_t conn_handle,
                                               uint8_t indication);

typedef struct {
    ble_uuid_t* uuid;
    ble_service_handle_peer handle_peer;
    ble_service_handle_notification handle_notification;
} ble_svc_inst_t;

esp_err_t ble_device_init(const ble_device_config_t *config);

esp_err_t ble_device_start_scan(uint8_t duration);

scan_result_t *ble_device_get_scan_rst(uint8_t *result_count);

esp_err_t ble_device_connect(ble_addr_t addr);

esp_err_t ble_device_deinit();

#endif