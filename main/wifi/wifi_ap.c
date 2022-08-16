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
#include "my_http_server.h"

#define WIFI_SSID      CONFIG_WIFI_SSID
#define WIFI_PASS      CONFIG_WIFI_PASSWORD
#define WIFI_CHANNEL   CONFIG_WIFI_CHANNEL
#define MAX_STA_CONN   CONFIG_MAX_STA_CONN

static const char *TAG = "wifi_softAP";

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "=====get wifi event %ld=====", event_id);
    if (event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "soft ap start success");
        my_http_server_start();
    } else if (event_id == WIFI_EVENT_AP_STOP) {
        ESP_LOGI(TAG, "soft ap stop...");
        my_http_server_stop();
    } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);

    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void print_current_ip_info(esp_netif_ip_info_t info_t) {
    char buff[20];
    esp_ip4addr_ntoa((const esp_ip4_addr_t *)&info_t.ip.addr, buff, sizeof(buff));
    ESP_LOGI(TAG, "current ip %s", buff);
    esp_ip4addr_ntoa((const esp_ip4_addr_t *)&info_t.gw.addr, buff, sizeof(buff));
    ESP_LOGI(TAG, "current gw %s", buff);
    esp_ip4addr_ntoa((const esp_ip4_addr_t *)&info_t.netmask.addr, buff, sizeof(buff));
    ESP_LOGI(TAG, "current msk %s", buff);
}

void wifi_init_softap(esp_event_loop_handle_t event_loop_hdl) {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 接收系统事件只能用default loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_LOGI(TAG, "start WIFI_MODE_AP");
    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    esp_netif_ip_info_t info_t;
    esp_netif_get_ip_info(ap_netif, &info_t);
    print_current_ip_info(info_t);

//    // change ip
//    char* ip= "192.168.5.241";
//    char* gateway = "192.168.5.1";
//    char* netmask = "255.255.255.0";
//    memset(&info_t, 0, sizeof(esp_netif_ip_info_t));
//
//    if (ap_netif) {
//        ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
//        info_t.ip.addr = esp_ip4addr_aton((const char *)ip);
//        info_t.netmask.addr = esp_ip4addr_aton((const char *)netmask);
//        info_t.gw.addr = esp_ip4addr_aton((const char *)gateway);
//        esp_netif_set_ip_info(ap_netif, &info_t);
//        ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
//
//        print_current_ip_info(info_t);
//    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
            .ap = {
                    .ssid = WIFI_SSID,
                    .ssid_len = strlen(WIFI_SSID),
                    .channel = WIFI_CHANNEL,
                    .password = WIFI_PASS,
                    .max_connection = MAX_STA_CONN,
                    .authmode = WIFI_AUTH_WPA2_PSK,
                    .pairwise_cipher = WIFI_CIPHER_TYPE_CCMP
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
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
    ESP_ERROR_CHECK(esp_wifi_stop());
    esp_wifi_deinit();
}