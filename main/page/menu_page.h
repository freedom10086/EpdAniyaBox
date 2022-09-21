#ifndef MENU_PAGE_H
#define MENU_PAGE_H

#include "lcd/epdpaint.h"
#include "lcd/display.h"

void menu_page_on_create(void *arg);

void menu_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

void menu_page_after_draw(uint32_t loop_cnt);

bool menu_page_key_click(key_event_id_t key_event_type);

void menu_page_on_destroy(void *arg);

#endif