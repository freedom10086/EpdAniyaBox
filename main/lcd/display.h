#ifndef DISPLAY_H
#define DISPLAY_H

#include "key.h"

ESP_EVENT_DECLARE_BASE(BIKE_REQUEST_UPDATE_DISPLAY_EVENT);

typedef void  (*request_display_update_cb)(bool full_refresh);

typedef void (*draw_page_cb)(epd_paint_t *epd_paint, uint32_t loop_cnt);

typedef void (*on_create_page_cb)(void *args);

typedef void (*on_destroy_page_cb)(void *args);

typedef bool (*key_click_handler)(key_event_id_t key_event_type);

typedef struct{
    char* page_name;
    draw_page_cb draw_cb;
    key_click_handler key_click_handler;
    on_create_page_cb on_create_page;
    on_destroy_page_cb on_destroy_page;
} page_inst_t;

void display_init(uint32_t boot_count);

#endif