#ifndef DISPLAY_H
#define DISPLAY_H

#include "key.h"

#define DEFAULT_SLEEP_TS 180

ESP_EVENT_DECLARE_BASE(BIKE_REQUEST_UPDATE_DISPLAY_EVENT);

void display_init(uint32_t boot_count);

#endif