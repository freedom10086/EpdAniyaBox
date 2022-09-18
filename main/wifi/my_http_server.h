#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

ESP_EVENT_DECLARE_BASE(BIKE_OTA_EVENT);

typedef enum {
    OTA_START,
    OTA_PROGRESS,
    OTA_FAILED,
    OTA_SUCCESS
} my_ota_event_t;

esp_err_t my_http_server_start();

esp_err_t my_http_server_stop();
#endif