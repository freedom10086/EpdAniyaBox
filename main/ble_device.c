#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "ble_csc.h"
#include "ble_device.h"

#define GATTC_TAG "BLE_DEVICE"

#define PROFILE_NUM      2
#define PROFILE_A_APP_ID 0
#define PROFILE_B_APP_ID 1

#define INVALID_HANDLE   0

ESP_EVENT_DEFINE_BASE(BLE_DEVICE_EVENT);

static bool scanning = false;

/* Declare static functions */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static void
gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static esp_bt_uuid_t notify_descr_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

struct service_char_map_t {
    uint16_t service_uuid;
    uint16_t char_uuid;
    bool en_read;
    bool en_notify;
};

static struct service_char_map_t service_char_map[] = {
        {.service_uuid = CYCLING_SPEED_AND_CADENCE_SERVICE_UUID, .char_uuid = CSC_MEASUREMENT_CHARACTERISTIC, .en_notify = true},
        {.service_uuid = BATTERY_LEVEL_SERVICE_UUID, .char_uuid = BATTERY_LEVEL_CHARACTERISTIC_UUID, .en_notify = true, .en_read = true},
        {.service_uuid = ESP_GATT_UUID_HEART_RATE_SVC, .char_uuid = ESP_GATT_HEART_RATE_MEAS, .en_notify = true},
        {.service_uuid = 0x0000,} // stop flag
};

static struct service_char_map_t find_service_char(esp_bt_uuid_t uuid) {
    if (uuid.len == ESP_UUID_LEN_16) {
        int i = 0;
        while (1) {
            struct service_char_map_t item = service_char_map[i];
            i++;

            if (item.service_uuid == 0x0000) {
                return item;
            }

            if (item.service_uuid == uuid.uuid.uuid16) {
                return item;
            }
        }
    }

    struct service_char_map_t t = {.service_uuid = 0x0000,};
    return t;
}

static esp_ble_scan_params_t ble_scan_params = {
        .scan_type              = BLE_SCAN_TYPE_PASSIVE,
        .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval          = 0x80, // 0x50 = 100ms
        .scan_window            = 0x30,
        .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

struct gattc_service_inst {
    esp_bt_uuid_t uuid;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
};

struct gattc_profile_inst {
    char *device_name;
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
    bool connect;

    struct gattc_service_inst services[10];
    uint16_t service_count;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
        [PROFILE_A_APP_ID] = {
                .device_name ="24564-1", // e6 10 d5 83 b7 1c
                .gattc_cb = gattc_profile_event_handler,
                .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
        },
        [PROFILE_B_APP_ID] = {
                .device_name = "XOSS_VOR_S1091",
                .gattc_cb = gattc_profile_event_handler,
                .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
        }
};

static struct gattc_service_inst find_service_inst_by_handle(uint8_t idx, uint16_t handle) {
    for (int i = 0; i < gl_profile_tab[idx].service_count; ++i) {
        if (gl_profile_tab[idx].services[i].char_handle == handle) {
            return gl_profile_tab[idx].services[i];
        }
    }

    ESP_LOGE(GATTC_TAG, "can not find handle %d", handle);

    struct gattc_service_inst empty = {
            .char_handle = 0
    };

    return empty;
}

static bool all_client_connected() {
    bool all_connect = true;
    for (int i = 0; i < PROFILE_NUM; ++i) {
        if (!gl_profile_tab[i].connect) {
            all_connect = false;
            break;
        }
    }
    return all_connect;
}

static void
gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *) param;

    int idx;
    for (idx = 0; idx < PROFILE_NUM; idx++) {
        if(gattc_if == gl_profile_tab[idx].gattc_if) {
            break;
        }
    }

    switch (event) {
        case ESP_GATTC_REG_EVT:
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_REG_EVT");
            if (!scanning) {
                esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
                if (scan_ret) {
                    ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
                }
            }
            break;
        case ESP_GATTC_CONNECT_EVT: {
            // 连接成功
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
            gl_profile_tab[idx].conn_id = p_data->connect.conn_id;
            memcpy(gl_profile_tab[idx].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
            ESP_LOGI(GATTC_TAG, "REMOTE BDA:");
                    esp_log_buffer_hex(GATTC_TAG, gl_profile_tab[idx].remote_bda, sizeof(esp_bd_addr_t));
            esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if, p_data->connect.conn_id);
            if (mtu_ret) {
                ESP_LOGE(GATTC_TAG, "config MTU error, error code = %x", mtu_ret);
            }
            break;
        }
        case ESP_GATTC_OPEN_EVT:
            if (param->open.status != ESP_GATT_OK) {
                ESP_LOGE(GATTC_TAG, "open failed, status %d", p_data->open.status);
                break;
            }
            ESP_LOGI(GATTC_TAG, "open success");
            break;
        case ESP_GATTC_DIS_SRVC_CMPL_EVT:
            if (param->dis_srvc_cmpl.status != ESP_GATT_OK) {
                ESP_LOGE(GATTC_TAG, "discover service failed, status %d", param->dis_srvc_cmpl.status);
                break;
            }
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_DIS_SRVC_CMPL_EVT discover service complete conn_id %d",
                     param->dis_srvc_cmpl.conn_id);
            // esp_ble_gattc_get_service 查找多个service 返回结果不通过回调
            // filter 为null就查询所有的
            esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, /*&remote_filter_service_uuid*/ NULL);
            // 开始查找服务
            break;
        case ESP_GATTC_CFG_MTU_EVT:
            // mtu设置完成
            if (param->cfg_mtu.status != ESP_GATT_OK) {
                ESP_LOGE(GATTC_TAG, "config mtu failed, error status = %x", param->cfg_mtu.status);
            }
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status,
                     param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
            break;
        case ESP_GATTC_SEARCH_RES_EVT: {
            // 查找服务结果（esp_ble_gattc_search_service 回调）
            ESP_LOGI(GATTC_TAG,
                     "   ESP_GATTC_SEARCH_RES_EVT conn_id = %x is primary service %d uuid:%x, start handle:%d end handle %d current handle %d",
                     p_data->search_res.conn_id,
                     p_data->search_res.is_primary, p_data->search_res.srvc_id.uuid.uuid.uuid16,
                     p_data->search_res.start_handle, p_data->search_res.end_handle,
                     p_data->search_res.srvc_id.inst_id);

            struct gattc_service_inst p = {
                    .uuid = p_data->search_res.srvc_id.uuid,
                    .service_start_handle = p_data->search_res.start_handle,
                    .service_end_handle = p_data->search_res.end_handle,
            };
            gl_profile_tab[idx].services[gl_profile_tab[idx].service_count] = p;
            gl_profile_tab[idx].service_count += 1;
            break;
        }
        case ESP_GATTC_SEARCH_CMPL_EVT:
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_SEARCH_CMPL_EVT service count: %d", gl_profile_tab[idx].service_count);

            // 所有服务查询结束
            if (p_data->search_cmpl.status != ESP_GATT_OK) {
                ESP_LOGE(GATTC_TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
                break;
            }
            if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
                ESP_LOGI(GATTC_TAG, "Get service information from remote device");
            } else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
                ESP_LOGI(GATTC_TAG, "Get service information from flash");
            } else {
                ESP_LOGI(GATTC_TAG, "unknown service source");
            }

            for (int i = 0; i < gl_profile_tab[idx].service_count; ++i) {
                struct gattc_service_inst ser_inst = gl_profile_tab[idx].services[i];
                esp_gattc_char_elem_t *char_elem_result = NULL;

                uint16_t count = 0;
                // 查询服务属性attribute数量
                esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                        p_data->search_cmpl.conn_id,
                                                                        ESP_GATT_DB_CHARACTERISTIC,
                                                                        ser_inst.service_start_handle,
                                                                        ser_inst.service_end_handle,
                                                                        INVALID_HANDLE,
                                                                        &count);
                if (status != ESP_GATT_OK) {
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
                }

                if (count == 0) {
                    ESP_LOGI(GATTC_TAG, "   service %x has no char", ser_inst.uuid.uuid.uuid16);
                    continue;
                }
                ESP_LOGI(GATTC_TAG, "   service %x has %d char", ser_inst.uuid.uuid.uuid16, count);

                char_elem_result = (esp_gattc_char_elem_t *) malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result) {
                    ESP_LOGE(GATTC_TAG, "gattc no mem");
                    continue;
                }

                struct service_char_map_t find_ser_char = find_service_char(ser_inst.uuid);
                if (find_ser_char.service_uuid == 0x0000) {
                    free(char_elem_result);
                    continue;
                }

                esp_bt_uuid_t remote_filter_char_uuid = {
                        .len = ESP_UUID_LEN_16,
                        .uuid = {.uuid16 = find_ser_char.char_uuid,},
                };

                // 查询csc measurement characteristic
                // esp_ble_gattc_get_all_char
                status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                        p_data->search_cmpl.conn_id,
                                                        ser_inst.service_start_handle,
                                                        ser_inst.service_end_handle,
                                                        remote_filter_char_uuid,
                                                        char_elem_result,
                                                        &count);
                if (status != ESP_GATT_OK) {
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error %x, %x", ser_inst.uuid.uuid.uuid16, find_ser_char.char_uuid);
                }

                if (count > 0) {
                    ESP_LOGI(GATTC_TAG, "find %d char from ser %x", count, ser_inst.uuid.uuid.uuid16);
                    for (int char_idx = 0; char_idx < count; char_idx++) {
                        esp_gattc_char_elem_t elem = char_elem_result[char_idx];
                        ESP_LOGI(GATTC_TAG, "\t char uuid:%x r:%d, w:%d, n:%d",
                                 elem.uuid.uuid.uuid16,
                                 (elem.properties & ESP_GATT_CHAR_PROP_BIT_READ) > 0,
                                 (elem.properties & ESP_GATT_CHAR_PROP_BIT_WRITE) > 0,
                                 (elem.properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY) > 0);

                        if (elem.uuid.uuid.uuid16 == find_ser_char.char_uuid) {
                            if (find_ser_char.en_notify && (elem.properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)) {
                                gl_profile_tab[idx].services[i].char_handle = elem.char_handle;
                                // 注册notify
                                ESP_LOGI(GATTC_TAG, "       service %x char %x connid:%d handle:%d register_for_notify",
                                         ser_inst.uuid.uuid.uuid16, elem.uuid.uuid.uuid16,
                                         p_data->search_cmpl.conn_id, elem.char_handle);
                                esp_ble_gattc_register_for_notify(gattc_if, gl_profile_tab[idx].remote_bda,
                                                                  elem.char_handle);
                            }

                            if (find_ser_char.en_read && (elem.properties & ESP_GATT_CHAR_PROP_BIT_READ)) {
                                gl_profile_tab[idx].services[i].char_handle = elem.char_handle;
                                ESP_LOGI(GATTC_TAG, "       service:%x char:%x connid:%d handle:%d read for value",
                                         ser_inst.uuid.uuid.uuid16, elem.uuid.uuid.uuid16,
                                         p_data->search_cmpl.conn_id, elem.char_handle);

                                esp_ble_gattc_read_char(gattc_if, p_data->search_cmpl.conn_id, elem.char_handle,
                                                        ESP_GATT_AUTH_REQ_NONE);
                            }
                        }
                    }
                }

                /* free char_elem_result */
                free(char_elem_result);
            }
            break;
        case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
            // 注册成功回调
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT handle %d", p_data->reg_for_notify.handle);
            if (p_data->reg_for_notify.status != ESP_GATT_OK) {
                ESP_LOGE(GATTC_TAG, "REG FOR NOTIFY failed: error status = %d handle %d", p_data->reg_for_notify.status, p_data->reg_for_notify.handle);
            } else {
                struct gattc_service_inst service_inst = find_service_inst_by_handle(idx, p_data->reg_for_notify.handle);
                uint16_t count = 0;
                uint16_t notify_en = 1;

                esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                            gl_profile_tab[idx].conn_id,
                                                                            ESP_GATT_DB_DESCRIPTOR,
                                                                            service_inst.service_start_handle,
                                                                             service_inst.service_end_handle,
                                                                            service_inst.char_handle,
                                                                            &count);

                ESP_LOGI(GATTC_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT find service %x desc count:%d", service_inst.uuid.uuid.uuid16, count);

                if (ret_status != ESP_GATT_OK) {
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
                }
                if (count > 0) {
                    esp_gattc_descr_elem_t *descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                    if (!descr_elem_result) {
                        ESP_LOGE(GATTC_TAG, "malloc error, gattc no mem");
                    } else {
                        ret_status = esp_ble_gattc_get_descr_by_char_handle(gattc_if,
                                                                            gl_profile_tab[idx].conn_id,
                                                                            p_data->reg_for_notify.handle,
                                                                            notify_descr_uuid,
                                                                            descr_elem_result,
                                                                            &count);
                        if (ret_status != ESP_GATT_OK) {
                            ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                        }
                        /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                        if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 &&
                            descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {

                            ESP_LOGI(GATTC_TAG, "   enable notify service:%x", service_inst.uuid.uuid.uuid16);

                            // 向0x2902写入开启notify
                            ret_status = esp_ble_gattc_write_char_descr(gattc_if,
                                                                        gl_profile_tab[idx].conn_id,
                                                                        descr_elem_result[0].handle,
                                                                        sizeof(notify_en),
                                                                        (uint8_t *) &notify_en,
                                                                        ESP_GATT_WRITE_TYPE_RSP,
                                                                        ESP_GATT_AUTH_REQ_NONE);
                        }

                        if (ret_status != ESP_GATT_OK) {
                            ESP_LOGE(GATTC_TAG, "esp_ble_gattc_write_char_descr error");
                        }

                        /* free descr_elem_result */
                        free(descr_elem_result);
                    }
                } else {
                    ESP_LOGE(GATTC_TAG, "decsr not found");
                }

            }
            break;
        }
        case ESP_GATTC_NOTIFY_EVT:
            // notify通知数据
            if (p_data->notify.is_notify) {
                //ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, receive notify conn_id:%d handle:%d  value:",
                //         p_data->notify.conn_id, p_data->notify.handle);
            } else {
                //ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate conn_id:%d handle:%d  value:",
                //         p_data->notify.conn_id, p_data->notify.handle);
            }
            //esp_log_buffer_hex(GATTC_TAG, p_data->notify.value, p_data->notify.value_len);

            esp_bt_uuid_t uuid = find_service_inst_by_handle(idx, p_data->notify.handle).uuid;
            if (CYCLING_SPEED_AND_CADENCE_SERVICE_UUID == uuid.uuid.uuid16) {
                ble_parse_csc_data(gl_profile_tab[idx].device_name, p_data);
            } else if (BATTERY_LEVEL_SERVICE_UUID == uuid.uuid.uuid16) {
                ESP_LOGI(GATTC_TAG, "notify battery level %d", *p_data->notify.value);
            }
            break;
        case ESP_GATTC_WRITE_DESCR_EVT:
            if (p_data->write.status != ESP_GATT_OK) {
                ESP_LOGE(GATTC_TAG, "write descr failed, error status = %x", p_data->write.status);
                break;
            }
            ESP_LOGI(GATTC_TAG, "write descr success ");
            //uint8_t write_char_data[35];
            //for (int i = 0; i < sizeof(write_char_data); ++i)
            //{
            //    write_char_data[i] = i % 256;
            //}
            //esp_ble_gattc_write_char( gattc_if,
            //                          gl_profile_tab[idx].conn_id,
            //                          gl_profile_tab[idx].char_handle,
            //                          sizeof(write_char_data),
            //                          write_char_data,
            //                          ESP_GATT_WRITE_TYPE_RSP,
            //                          ESP_GATT_AUTH_REQ_NONE);
            break;
        case ESP_GATTC_SRVC_CHG_EVT: {
            esp_bd_addr_t bda;
            memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
                    esp_log_buffer_hex(GATTC_TAG, bda, sizeof(esp_bd_addr_t));
            break;
        }
        case ESP_GATTC_READ_CHAR_EVT:
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_READ_CHAR_EVT: conn_id: %d handle %d len:%d", p_data->read.conn_id,
                     p_data->read.handle,  p_data->read.value_len);
                    esp_log_buffer_hex(GATTC_TAG, p_data->read.value, p_data->read.value_len);

            uuid = find_service_inst_by_handle(idx, p_data->read.handle).uuid;
            if (BATTERY_LEVEL_SERVICE_UUID == uuid.uuid.uuid16) {
                uint8_t battery_level = *p_data->read.value;
                ESP_LOGI(GATTC_TAG, "read battery level %d", battery_level);
            }
            break;
        case ESP_GATTC_WRITE_CHAR_EVT:
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_WRITE_CHAR_EVT:");
            if (p_data->write.status != ESP_GATT_OK) {
                ESP_LOGE(GATTC_TAG, "write char failed, error status = %x", p_data->write.status);
                break;
            }
            ESP_LOGI(GATTC_TAG, "write char success ");
            break;
        case ESP_GATTC_DISCONNECT_EVT:
            gl_profile_tab[idx].connect = false;
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);
            break;
        default:
            break;
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    uint8_t *adv_manufacturer_specific_type = NULL;
    uint8_t adv_manufacturer_specific_type_len = 0;

    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
            ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");
            // esp_ble_gap_set_scan_params call back
            //the unit of the duration is second
            uint32_t duration = 15;
            esp_ble_gap_start_scanning(duration);
            break;
        }
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            scanning = true;
            //scan start complete event to indicate scan start successfully or failed
            if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTC_TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
                break;
            }
            ESP_LOGI(GATTC_TAG, "scan start success");

            break;
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            // 搜索到新设备
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *) param;
            switch (scan_result->scan_rst.search_evt) {
                case ESP_GAP_SEARCH_INQ_RES_EVT:  // Inquiry result for a peer device.
                    //case ESP_GAP_SEARCH_DISC_BLE_RES_EVT:  // Discovery result for BLE GATT based service on a peer device.
                    //esp_log_buffer_hex(GATTC_TAG, scan_result->scan_rst.bda, 6);
                    //ESP_LOGI(GATTC_TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
                    adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                        ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
                    if (adv_name == NULL) {
                        break;
                    }

                    ESP_LOGI(GATTC_TAG, "searched Device:");
                            esp_log_buffer_char(GATTC_TAG, adv_name, adv_name_len);
                    //ESP_LOGI(GATTC_TAG, "RSSI of packet:%d dbm", scan_result->scan_rst.rssi);

                    // 只有主动搜索才有 被动搜索没有
                    // BLE_SCAN_TYPE_PASSIVE ACTIVE
                    if (scan_result->scan_rst.scan_rsp_len > 0) {
                        ESP_LOGI(GATTC_TAG, "scan resp:");
                                esp_log_buffer_hex(GATTC_TAG,
                                                   &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len],
                                                   scan_result->scan_rst.scan_rsp_len);
                    }

                    if (scan_result->scan_rst.adv_data_len > 0) {
                        //ESP_LOGI(GATTC_TAG, "adv data:");
                        //esp_log_buffer_hex(GATTC_TAG, &scan_result->scan_rst.ble_adv[0],scan_result->scan_rst.adv_data_len);
                    }

                    uint8_t length;
                    uint8_t *part_uuid = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_16SRV_PART, &length);
                    if(length != 0) {
                        esp_log_buffer_hex("searched part uuid", part_uuid, length);
                    }

                    // 支持的service的uuid
                    uint8_t *cmpl_uuid  = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_16SRV_CMPL, &length);
                    if(length != 0) {
                        // eg  16 18 0a 18 0f 18 ->0x1816 0x180f 0x180a
                        esp_log_buffer_hex("searched cmpl uuid", cmpl_uuid, length);
                    }

                    ESP_LOGI(GATTC_TAG, "\n");
                    for (int i = 0; i < PROFILE_NUM; i++) {
                        char *remote_device_name = gl_profile_tab[i].device_name;

                        if (strlen(remote_device_name) == adv_name_len &&
                            strncmp((char *) adv_name, remote_device_name, adv_name_len) == 0) {
                            ESP_LOGI(GATTC_TAG, "searched device 【%s】", remote_device_name);
                            if (!gl_profile_tab[i].connect) {
                                gl_profile_tab[i].connect = true;
                                ESP_LOGI(GATTC_TAG, "connect to the remote device. %s", remote_device_name);
                                esp_log_buffer_hex("bda", scan_result->scan_rst.bda, ESP_BD_ADDR_LEN);
                                if (all_client_connected()) {
                                    esp_ble_gap_stop_scanning();
                                }
                                esp_ble_gattc_open(gl_profile_tab[i].gattc_if, scan_result->scan_rst.bda,
                                                   scan_result->scan_rst.ble_addr_type, true);
                            }
                        }
                    }
                    break;
                case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                    scanning = false;
                    // 搜索duration时间到了
                    ESP_LOGI(GATTC_TAG, "ESP_GAP_SEARCH_INQ_CMPL_EVT");
                    for (int i = 0; i < PROFILE_NUM; ++i) {
                        if (!gl_profile_tab[i].connect) {
                            uint32_t duration = 15;

                            // 重新搜索
                            esp_ble_gap_set_scan_params(&ble_scan_params);
                            // esp_ble_gap_start_scanning(duration);
                            break;
                        }
                    }
                    break;
                case ESP_GAP_SEARCH_DISC_BLE_RES_EVT:
                    ESP_LOGI(GATTC_TAG, "ESP_GAP_SEARCH_DISC_BLE_RES_EVT");
                    break;
                default:
                    break;
            }
            break;
        }

        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            // esp_ble_gap_stop_scanning 主动取消事件
            if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTC_TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
                break;
            }
            scanning = false;
            ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT stop scan successfully");
            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTC_TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
                break;
            }
            ESP_LOGI(GATTC_TAG, "stop adv successfully");
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(GATTC_TAG,
                     "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                     param->update_conn_params.status,
                     param->update_conn_params.min_int,
                     param->update_conn_params.max_int,
                     param->update_conn_params.conn_int,
                     param->update_conn_params.latency,
                     param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(GATTC_TAG, "reg app failed, app_id %04x, status %d",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE ||
                /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

esp_err_t ble_device_init(const ble_device_config_t *config) {
    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return NULL;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return NULL;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return NULL;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return NULL;
    }

    // 白名单
    // esp_ble_gap_update_whitelist

    //register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s gap register failed, error code = %x\n", __func__, ret);
        return NULL;
    }

    //register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s gattc register failed, error code = %x\n", __func__, ret);
        return NULL;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s gattc app a register failed, error code = %x\n", __func__, ret);
    }

    ret = esp_ble_gattc_app_register(PROFILE_B_APP_ID);
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s gattc app b register failed, error code = %x\n", __func__, ret);
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret) {
        ESP_LOGE(GATTC_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }
    return ESP_OK;
}

esp_err_t ble_device_deinit(esp_event_loop_handle_t hdl) {
    return NULL;
}