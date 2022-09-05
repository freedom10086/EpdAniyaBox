#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "stdio.h"
#include "stdlib.h"
#include "lcd/epdpaint.h"
#include "key.h"

typedef void (*on_draw_page_cb)(epd_paint_t *epd_paint, uint32_t loop_cnt);

typedef void (*on_create_page_cb)(void *args);

typedef void (*on_destroy_page_cb)(void *args);

typedef bool (*key_click_handler)(key_event_id_t key_event_type);

typedef struct{
    char* page_name;
    on_draw_page_cb on_draw_page;
    key_click_handler key_click_handler;
    on_create_page_cb on_create_page;
    on_destroy_page_cb on_destroy_page;
} page_inst_t;

void page_manager_init(char *default_page);

void page_manager_reg_page(page_inst_t page);

void page_manager_switch_page(char *page_name);

page_inst_t page_manager_get_current_page();

#endif