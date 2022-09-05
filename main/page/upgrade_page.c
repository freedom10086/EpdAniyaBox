#include <stdio.h>
#include <stdlib.h>

#include <esp_log.h>
#include "static/static.h"
#include "upgrade_page.h"


void upgrade_page_on_create(void *args) {

}

void upgrade_page_on_destroy(void *args) {

}

void upgrade_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);

    // upgrade icon
    epd_paint_draw_bitmap(epd_paint, 75, 70, 48, 38,
                          (uint8_t *) icon_upgrade_bmp_start,
                          icon_upgrade_bmp_end - icon_upgrade_bmp_start, 1);

    // 固件升级中.
    uint16_t data[] = {0xCCB9, 0xFEBC, 0xFDC9, 0xB6BC, 0xD0D6, 0x2E, 0x00};
    epd_paint_draw_string_at(epd_paint, 60, 114, (char *) data, &Font_HZK16, 1);

    char buff[8];
    sprintf(buff, "%.1f%%", 18.99f);
    epd_paint_draw_string_at(epd_paint, 80, 134, (char *) buff, &Font16, 1);
}

bool upgrade_page_key_click_handle(key_event_id_t key_event_type) {
    return true;
}