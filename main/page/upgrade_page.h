#ifndef UPGRADE_PAGE_H
#define UPGRADE_PAGE_H

#include "lcd/epdpaint.h"
#include "key.h"

enum upgrade_state_t {
    INIT_LOW_BATTERY,
    INIT,
    UPGRADING,
    UPGRADE_FAILED,
    UPGRADE_SUCCESS
};

void upgrade_page_on_create(void *args);

void upgrade_page_on_destroy(void *args);

void upgrade_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool upgrade_page_key_click_handle(key_event_id_t key_event_type);

int upgrade_page_on_enter_sleep(void *args);

#endif