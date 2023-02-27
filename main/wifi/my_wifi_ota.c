#include <esp_app_format.h>
#include "esp_http_server.h"
#include "esp_event.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_wifi.h"
#include "esp_log.h"

#include "bike_common.h"
#include "my_wifi_ota.h"

#define TAG "wifi-ota"

#define OTA_BUFFSIZE (8*1024)
#define HASH_LEN 32 /* SHA-256 digest length */

ESP_EVENT_DEFINE_BASE(BIKE_OTA_EVENT);

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char *get_path_from_uri(char *dest, const char *uri, size_t destsize) {
    size_t pathlen = strlen(uri);
    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }
    strlcpy(dest, uri, pathlen + 1);
    return dest;
}

static void print_sha256(const uint8_t *image_hash, const char *label) {
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s: %s", label, hash_print);
}

//当前固件信息
esp_err_t current_version_handler(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");        //跨域传输协议

    static char json_response[1024];

    esp_app_desc_t running_app_info;
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_get_partition_description(running, &running_app_info);

    char *p = json_response;
    *p++ = '{';
    p += sprintf(p, "\"ota_subtype\":%d,", running->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);     //OTA分区
    p += sprintf(p, "\"address\":%ld,", running->address);               //地址
    p += sprintf(p, "\"version\":\"%s\",", running_app_info.version);   //版本号
    p += sprintf(p, "\"date\":\"%s\",", running_app_info.date);         //日期
    p += sprintf(p, "\"time\":\"%s\"", running_app_info.time);          //时间
    *p++ = '}';
    *p++ = 0;

    httpd_resp_set_type(req, "application/json");       // 设置http响应类型
    return httpd_resp_send(req, json_response, strlen(json_response));
}

esp_err_t ota_get_handler(httpd_req_t *req) {
    extern const unsigned char ota_html_start[] asm("_binary_ota_html_start");
    extern const unsigned char ota_html_end[]   asm("_binary_ota_html_end");

    const size_t response_size = (ota_html_end - ota_html_start);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *) ota_html_start, response_size);
    return ESP_OK;
}

/* Handler to upload a file onto the server */
esp_err_t ota_post_handler(httpd_req_t *req) {
    char filepath[64];

    /* Skip leading "/ota" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, req->uri + sizeof("/ota") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_OTA_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
        MAX_OTA_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filename);

    ESP_LOGI(TAG, "OTA start");
    uint8_t sha_256[HASH_LEN] = {0};
    esp_partition_t partition;

    // get sha256 digest for the partition table
    partition.address = ESP_PARTITION_TABLE_OFFSET;
    partition.size = ESP_PARTITION_TABLE_MAX_LEN;
    partition.type = ESP_PARTITION_TYPE_DATA;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for the partition table: ");

    // get sha256 digest for bootloader
    partition.address = ESP_BOOTLOADER_OFFSET;
    partition.size = ESP_PARTITION_TABLE_OFFSET;
    partition.type = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // run diagnostic function ...
            ESP_LOGI(TAG, "Diagnostics completed successfully! Continuing execution ...");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    ESP_ERROR_CHECK(common_init_nvs());

    /* Ensure to disable any WiFi power save mode, this allows best throughput
     * and hence timings for overall OTA operation.
     */
    esp_wifi_set_ps(WIFI_PS_NONE);


    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG, "Starting OTA");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08lx, but running from offset 0x%08lx",
                 configured->address, running->address);
        ESP_LOGW(TAG,
                 "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08lx)",
             running->type, running->subtype, running->address);

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    update_partition = esp_ota_get_next_update_partition(NULL);
    assert(update_partition != NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx",
             update_partition->subtype, update_partition->address);

    esp_err_t err;
    int binary_file_length = 0;
    /*deal with all receive packet*/
    bool image_header_was_checked = false;
    int remaining = req->content_len;
    int data_read;
    char *ota_write_data = malloc(sizeof(char) * OTA_BUFFSIZE);
    float upgrade_progress;

    while (remaining > 0) {
        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((data_read = httpd_req_recv(req, ota_write_data, MIN(remaining, OTA_BUFFSIZE))) <= 0) {
            if (data_read == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            // todo clean stuff

            ESP_LOGE(TAG, "File reception failed!");
            free(ota_write_data);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        remaining -= data_read;

        if (image_header_was_checked == false) {
            esp_app_desc_t new_app_info;
            if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                // check current version with downloading
                memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)],
                       sizeof(esp_app_desc_t));
                ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                esp_app_desc_t running_app_info;
                if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
                    ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
                }

                const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
                esp_app_desc_t invalid_app_info;
                if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK) {
                    ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
                }

                // check current version with last invalid partition
                if (last_invalid_app != NULL) {
                    if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0) {
                        ESP_LOGW(TAG, "New version is the same as invalid version.");
                        ESP_LOGW(TAG,
                                 "Previously, there was an attempt to launch the firmware with %s version, but it failed.",
                                 invalid_app_info.version);
                        ESP_LOGW(TAG, "The firmware has been rolled back to the previous version.");

                        free(ota_write_data);
                        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid firmware version");
                        return ESP_FAIL;
                    }
                }

                if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0) {
                    ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");

                    free(ota_write_data);
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "same firmware version");
                    return ESP_FAIL;
                }

                image_header_was_checked = true;
                err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                    esp_ota_abort(update_handle);

                    free(ota_write_data);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_begin failed..");
                    return ESP_FAIL;
                }
                ESP_LOGI(TAG, "esp_ota_begin succeeded");
                common_post_event(BIKE_OTA_EVENT, OTA_START);
            } else {
                ESP_LOGE(TAG, "received package is not fit len");
                esp_ota_abort(update_handle);

                free(ota_write_data);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "received package is not fit len");
                common_post_event(BIKE_OTA_EVENT, OTA_FAILED);
                return ESP_FAIL;
            }
        }
        err = esp_ota_write(update_handle, (const void *) ota_write_data, data_read);
        if (err != ESP_OK) {
            esp_ota_abort(update_handle);

            free(ota_write_data);
            ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_write failed..");
            common_post_event(BIKE_OTA_EVENT, OTA_FAILED);
            return ESP_FAIL;
        }
        binary_file_length += data_read;
        ESP_LOGD(TAG, "Written image length %d", binary_file_length);
        upgrade_progress = (float) binary_file_length * 1.0f / req->content_len;
        common_post_event_data(BIKE_OTA_EVENT, OTA_PROGRESS, &upgrade_progress, sizeof(float *));
    }

    free(ota_write_data);

    ESP_LOGI(TAG, "Total Write binary data length: %d", binary_file_length);
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }

        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_end failed..");
        common_post_event(BIKE_OTA_EVENT, OTA_FAILED);
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_set_boot_partition failed..");
        common_post_event(BIKE_OTA_EVENT, OTA_FAILED);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Ota success Prepare to restart system!");
    common_post_event(BIKE_OTA_EVENT, OTA_SUCCESS);

    httpd_resp_set_type(req, "text/html");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    char *response = "{\"status\": 200, \"message\": \"ota success\"}";
    httpd_resp_send(req, response, strlen(response));

//    vTaskDelay(pdMS_TO_TICKS(100));
//    esp_restart();

    return ESP_OK;
}