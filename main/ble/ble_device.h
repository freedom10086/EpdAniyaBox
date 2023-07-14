#ifndef BLE_DEVICE_H
#define BLE_DEVICE_H

#include "esp_bt.h"
#include "ble_csc.h"
#include "ble_hrm.h"

ESP_EVENT_DECLARE_BASE(BLE_DEVICE_EVENT);

#define MAX_BLE_DEVICE_NAME_LEN 20

#include "modlog/modlog.h"
#include "esp_central.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

#define BLECENT_SVC_ALERT_UUID              0x1811
#define BLECENT_CHR_SUP_NEW_ALERT_CAT_UUID  0x2A47
#define BLECENT_CHR_NEW_ALERT               0x2A46
#define BLECENT_CHR_SUP_UNR_ALERT_CAT_UUID  0x2A48
#define BLECENT_CHR_UNR_ALERT_STAT_UUID     0x2A45
#define BLECENT_CHR_ALERT_NOT_CTRL_PT       0x2A44

#define BLE_SVC_HRM_UUID 0x180D

#ifdef __cplusplus
}
#endif

typedef struct {

} ble_device_config_t;

typedef struct {
    uint8_t dev_name[MAX_BLE_DEVICE_NAME_LEN];
    int dev_name_len;
    ble_addr_t addr;
    int rssi;
} scan_result_t;

typedef enum {
    BT_INIT,
    BT_START_SCAN,
    BT_STOP_SCAN,
    BT_NEW_SCAN_RESULT,
    BT_START_CONNECT,
    BT_CONNECT_SUCCESS,
    BT_CONNECT_FAILED,
    BT_DEINIT,
} my_bt_event_id_t;

esp_err_t ble_device_init(const ble_device_config_t *config);

esp_err_t ble_device_start_scan(uint8_t duration);

scan_result_t *ble_device_get_scan_rst(uint8_t *result_count);

esp_err_t ble_device_connect();

esp_err_t ble_device_deinit(esp_event_loop_handle_t hdl);

#endif