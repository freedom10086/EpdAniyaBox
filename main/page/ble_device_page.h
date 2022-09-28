#ifndef EPD_ANIYA_BOX_BLE_DEVICE_PAGE_H
#define EPD_ANIYA_BOX_BLE_DEVICE_PAGE_H

#include "sdkconfig.h"

#ifdef CONFIG_ENABLE_BLE_DEVICES

#include "lcd/epdpaint.h"
#include "lcd/display.h"

void ble_device_page_on_create(void *arg);

void ble_device_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

void ble_device_page_after_draw(uint32_t loop_cnt);

bool ble_device_page_key_click(key_event_id_t key_event_type);

void ble_device_page_on_destroy(void *arg);

int ble_device_page_on_enter_sleep(void *args);

#endif

#endif //EPD_ANIYA_BOX_BLE_DEVICE_PAGE_H
