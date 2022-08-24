#ifndef BITMAP_PAGE_H
#define BITMAP_PAGE_H

#include "epdpaint.h"

void bitmap_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool bitmap_page_key_click_handle(key_event_id_t key_event_type);

#endif