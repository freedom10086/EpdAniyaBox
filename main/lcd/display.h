#ifndef DISPLAY_H
#define DISPLAY_H

#include "key.h"

ESP_EVENT_DECLARE_BASE(BIKE_REQUEST_UPDATE_DISPLAY_EVENT);

void display_init(uint32_t boot_count);

#endif