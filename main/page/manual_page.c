//
// Created by yang on 2022/9/13.
//

#include "manual_page.h"
#include "page_manager.h"

// 还没想好，动动脑瓜子！
static uint8_t content[] = {
        0xBB, 0xB9, 0xC3, 0xBB, 0xCF, 0xEB, 0xBA, 0xC3, 0xA3,
        0xAC, 0xB6, 0xAF, 0xB6, 0xAF, 0xC4, 0xD4, 0xB9, 0xCF,
        0xD7, 0xD3, 0xA3, 0xA1, 0x00
};

void manual_page_on_create(void *arg) {

}

void manual_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    uint8_t y = 16;
    epd_paint_draw_string_at(epd_paint, 8, y, (char *) content, &Font_HZK16, 1);
}

bool manual_page_key_click(key_event_id_t key_event_type) {
    switch (key_event_type) {
        case KEY_1_SHORT_CLICK:
        case KEY_2_SHORT_CLICK:
            page_manager_close_page();
            page_manager_request_update(false);
            return true;
        default:
            return false;
    }
}

void manual_page_on_destroy(void *arg) {

}