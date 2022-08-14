#ifndef SHT31_H
#define SHT31_H

#include <esp_event_base.h>

ESP_EVENT_DECLARE_BASE(BIKE_TEMP_HUM_SENSOR_EVENT);

typedef struct {
    float temp;
    float hum;
} sht31_data_t;

typedef struct {
    bool data_valid;

    int32_t raw_temp;
    int32_t raw_hum;

    sht31_data_t data;

    esp_event_loop_handle_t event_loop_hdl;
} sht31_t;

/**
 * sht31 event
 */
typedef enum {
    SHT31_SENSOR_UPDATE,
} sht31_event_id_t;

sht31_t * sht31_init(esp_event_loop_handle_t event_loop_hdl);
void sht31_reset();
bool sht31_read_temp_hum();
void sht31_deinit();
#endif