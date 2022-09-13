//
// Created by yang on 2022/9/13.
//

#ifndef MY_SUPER_BIKE_SETTING_PAGE_H
#define MY_SUPER_BIKE_SETTING_PAGE_H

#include "lcd/epdpaint.h"
#include "lcd/display.h"

void setting_page_on_create(void *arg);

void setting_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool setting_page_key_click(key_event_id_t key_event_type);

bool setting_page_on_enter_sleep(void *args);

void setting_page_on_destroy(void *arg);

#endif //MY_SUPER_BIKE_SETTING_PAGE_H
