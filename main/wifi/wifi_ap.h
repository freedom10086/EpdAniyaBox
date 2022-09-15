#ifndef WIFI_AP_H
#define WIFI_AP_H

#include "esp_netif_types.h"

void wifi_init_softap();

void wifi_deinit_softap();

esp_err_t wifi_ap_get_ip(esp_netif_ip_info_t *ip_info);

#endif