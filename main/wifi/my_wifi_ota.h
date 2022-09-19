#ifndef MY_WIFI_OTA_H
#define MY_WIFI_OTA_H

#include "esp_http_server.h"

ESP_EVENT_DECLARE_BASE(BIKE_OTA_EVENT);

#define MAX_OTA_FILE_SIZE   (2*1024*1024) // 2MB
#define MAX_OTA_FILE_SIZE_STR "2MB"

typedef enum {
    OTA_START,
    OTA_PROGRESS,
    OTA_FAILED,
    OTA_SUCCESS
} my_ota_event_t;

esp_err_t current_version_handler(httpd_req_t *req);

esp_err_t ota_get_handler(httpd_req_t *req);

esp_err_t ota_post_handler(httpd_req_t *req);

#endif