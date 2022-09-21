#ifndef MY_SUPER_BIKE_SETTING_LIST_PAGE_H
#define MY_SUPER_BIKE_SETTING_LIST_PAGE_H

#include "lcd/epdpaint.h"
#include "lcd/display.h"

void setting_list_page_on_create(void *arg);

void setting_list_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

void setting_list_page_after_draw(uint32_t loop_cnt);

bool setting_list_page_key_click(key_event_id_t key_event_type);

void setting_list_page_on_destroy(void *arg);

int setting_list_page_on_enter_sleep(void *args);

#endif //MY_SUPER_BIKE_SETTING_LIST_PAGE_H
