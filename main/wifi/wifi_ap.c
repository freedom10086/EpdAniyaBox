#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi_ap.h"

#define WIFI_SSID      CONFIG_WIFI_SSID
#define WIFI_PASS      CONFIG_WIFI_PASSWORD
#define WIFI_CHANNEL   CONFIG_WIFI_CHANNEL
#define MAX_STA_CONN   CONFIG_MAX_STA_CONN

static const char *TAG = "wifi_softAP";

esp_event_loop_handle_t _event_loop_hdl = NULL;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                MAC2STR(event->mac), event->aid);

    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(esp_event_loop_handle_t event_loop_hdl) {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "start WIFI_MODE_AP");

    ESP_ERROR_CHECK(esp_netif_init());
    //ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
//                                                        ESP_EVENT_ANY_ID,
//                                                        &wifi_event_handler,
//                                                        NULL,
//                                                        NULL));
    esp_event_handler_register_with(event_loop_hdl,
                                    WIFI_EVENT, ESP_EVENT_ANY_ID,
                                    wifi_event_handler, NULL);
    _event_loop_hdl = event_loop_hdl;

    wifi_config_t wifi_config = {
            .ap = {
                    .ssid = WIFI_SSID,
                    .ssid_len = strlen(WIFI_SSID),
                    .channel = WIFI_CHANNEL,
                    .password = WIFI_PASS,
                    .max_connection = MAX_STA_CONN,
                    .authmode = WIFI_AUTH_WPA2_PSK,
            },
    };
    if (strlen(WIFI_SSID) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
}

void wifi_deinit_softap() {
    if (_event_loop_hdl == NULL) {
        return;
    }
    esp_event_handler_unregister_with(_event_loop_hdl, WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
    _event_loop_hdl = NULL;
    ESP_ERROR_CHECK(esp_wifi_stop());
    esp_wifi_deinit();
}