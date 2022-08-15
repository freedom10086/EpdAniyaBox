#ifndef WIFI_AP_H
#define WIFI_AP_H

void wifi_init_softap(esp_event_loop_handle_t event_loop_hdl);

void wifi_deinit_softap();

#endif