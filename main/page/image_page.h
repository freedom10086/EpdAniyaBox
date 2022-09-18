#ifndef BITMAP_PAGE_H
#define BITMAP_PAGE_H

#include "lcd/epdpaint.h"

void image_page_on_create(void *arg);

void image_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool image_page_key_click_handle(key_event_id_t key_event_type);

void image_page_on_destroy(void *arg);

int image_page_on_enter_sleep(void *args);

#endif