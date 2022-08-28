#ifndef KEY_H
#define KEY_H

#include "esp_types.h"
#include "esp_event.h"

#include "event_common.h"

ESP_EVENT_DECLARE_BASE(BIKE_KEY_EVENT);

#define KEY_1_NUM 0
#define KEY_2_NUM 9

/**
 * key click event
 */
typedef enum {
    KEY_1_SHORT_CLICK,
    KEY_1_LONG_CLICK,
    KEY_2_SHORT_CLICK,
    KEY_2_LONG_CLICK,
} key_event_id_t;

void key_init();

#endif