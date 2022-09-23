//
// Created by yang on 2022/9/13.
//

#ifndef IMAGE_MANAGE_PAGE_H
#define IMAGE_MANAGE_PAGE_H

#include "lcd/epdpaint.h"
#include "lcd/display.h"

void image_manage_page_on_create(void *arg);

void image_manage_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool image_manage_page_key_click(key_event_id_t key_event_type);

int image_manage_page_on_enter_sleep(void *args);

void image_manage_page_on_destroy(void *arg);

#endif //IMAGE_MANAGE_PAGE_H
