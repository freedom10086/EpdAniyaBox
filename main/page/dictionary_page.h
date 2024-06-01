//
// Created by yang on 2024/5/21.
//

#include "lcd/epdpaint.h"
#include "key.h"
#include "lcd/display.h"

#ifndef ANIYA_BOX_V2_DICTIONARY_PAGE_H
#define ANIYA_BOX_V2_DICTIONARY_PAGE_H

void dictionary_page_on_create(void *arg);

void dictionary_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool dictionary_page_key_click(key_event_id_t key_event_type);

void dictionary_page_on_destroy(void *arg);

int dictionary_page_on_enter_sleep(void *arg);

#endif //ANIYA_BOX_V2_DICTIONARY_PAGE_H
