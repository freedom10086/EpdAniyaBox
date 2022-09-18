#ifndef INFO_PAGE_H
#define INFO_PAGE_H

#include "lcd/epdpaint.h"
#include "lcd/display.h"

void info_page_on_create(void *arg);

void info_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool info_page_key_click(key_event_id_t key_event_type);

int info_page_on_enter_sleep(void *args);

#endif