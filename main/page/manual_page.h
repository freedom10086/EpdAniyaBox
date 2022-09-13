#ifndef MY_SUPER_BIKE_MANUAL_PAGE_H
#define MY_SUPER_BIKE_MANUAL_PAGE_H

#include "lcd/epdpaint.h"
#include "lcd/display.h"

void manual_page_on_create(void *arg);

void manual_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool manual_page_key_click(key_event_id_t key_event_type);

void manual_page_on_destroy(void *arg);


#endif //MY_SUPER_BIKE_MANUAL_PAGE_H
