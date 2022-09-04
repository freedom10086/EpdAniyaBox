#ifndef UPGRADE_PAGE_H
#define UPGRADE_PAGE_H

#include "epdpaint.h"
#include "key.h"

void upgrade_page_on_create(void *args);

void upgrade_page_on_destroy(void *args);

void upgrade_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool upgrade_page_key_click_handle(key_event_id_t key_event_type);

#endif