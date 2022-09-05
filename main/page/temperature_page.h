#ifndef TEMPERATURE_PAGE_H
#define TEMPERATURE_PAGE_H

#include "lcd/epdpaint.h"
#include "key.h"

void temperature_page_on_create(void *args);

void temperature_page_on_destroy(void *args);

void temperature_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool temperature_page_key_click_handle(key_event_id_t key_event_type);

#endif