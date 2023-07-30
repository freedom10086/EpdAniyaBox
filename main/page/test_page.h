#ifndef TEST_PAGE_H
#define TEST_PAGE_H

#include "lcd/epdpaint.h"
#include "key.h"
#include "lcd/display.h"

void test_page_on_create(void *arg);

void test_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool test_page_key_click(key_event_id_t key_event_type);

void test_page_on_destroy(void *arg);

int test_page_on_enter_sleep(void *arg);

#endif