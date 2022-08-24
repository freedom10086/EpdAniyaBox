#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <string.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include <math.h>

#include "bike_common.h"
#include "list_view.h"

#define DIVIDER 1
#define PADDING_TOP 1
#define PADDING_BOTTOM 1
#define PADDING_START 4
#define PADDING_END 4

#define TAG "list-view"

list_view_t *list_vew_create(int x, int y, int width, int height, sFONT *font) {
    ESP_LOGI(TAG, "init list view");
    list_view_t *list_view = malloc(sizeof(list_view_t));
    if (!list_view) {
        ESP_LOGE(TAG, "no memory for init list_view");
        return NULL;
    }

    list_view->element_count = 0;
    list_view->current_index = -1;
    list_view->head = NULL;

    list_view->x = x;
    list_view->y = y;
    list_view->width = width;
    list_view->height = height;
    list_view->font = font;
    ESP_LOGI(TAG, "list view created");
    return list_view;
}

void list_view_add_element(list_view_t *list_view, char *text) {
    ESP_LOGI(TAG, "list view add item");
    struct list_view_element_t *head = list_view->head;
    while (head != NULL && head->next != NULL) {
        head = head->next;
    }

    struct list_view_element_t *ele = malloc(sizeof(struct list_view_element_t));
    if (!ele) {
        ESP_LOGE(TAG, "no memory for new list_view element");
    }

    //ele->text = calloc(strlen(text), sizeof(char));
    //strcpy(ele->text, text);
    ele->text = text;
    ele->next = NULL;

    if (head == NULL) {
        // first element
        list_view->head = ele;
    } else { // head -> next is null
        head->next = ele;
    }

    list_view->element_count += 1;
    if (list_view->current_index == -1) {
        list_view->current_index = 0;
    }
}

struct list_view_element_t *list_view_get_element(list_view_t *list_view, uint8_t index) {
    assert(index >= 0 && index < list_view->element_count);
    struct list_view_element_t *pre = NULL;
    struct list_view_element_t *item = list_view->head;
    for (; index > 0; index--) {
        item = item->next;
    }
    return item;
}

void list_view_update_item(list_view_t *list_view, uint8_t index, char *newText) {
    struct list_view_element_t *ele = list_view_get_element(list_view, index);
    ele->text = newText;
}

void list_view_remove_element(list_view_t *list_view, uint8_t index) {
    assert(index >= 0 && index < list_view->element_count);
    struct list_view_element_t *pre = NULL;
    struct list_view_element_t *item = list_view->head;
    for (; index > 0; index--) {
        pre = item;
        item = item->next;
    }

    if (pre == NULL) {
        list_view->head = item->next;
    } else {
        pre->next = item->next;
    }

    list_view->element_count -= 1;
    if (list_view->current_index >= list_view->element_count) {
        list_view->current_index = list_view->element_count - 1;
    }

    item->next = NULL;
    free(item);
}

void list_view_remove_first_element(list_view_t *list_view) {
    list_view_remove_element(list_view, 0);
}

void list_view_remove_last_element(list_view_t *list_view) {
    list_view_remove_element(list_view, list_view->element_count - 1);
}

void list_vew_draw(list_view_t *list_view, epd_paint_t *epd_paint, uint32_t loop_cnt) {

    epd_paint_clear_range(epd_paint, list_view->x, list_view->y,
                          list_view->width, list_view->height, 0);

    int y = list_view->y;
    int x = list_view->x;

    struct list_view_element_t *head = list_view->head;

    int item_start_y;
    int index = 0;
    while (head != NULL) {
        item_start_y = y;

        y += PADDING_TOP;
        epd_paint_draw_string_at(epd_paint, x + PADDING_START, y, head->text, list_view->font, 1);
        y += list_view->font->Height;
        y += PADDING_BOTTOM;

        head = head->next;
        if (head != NULL && DIVIDER) {
            // not last draw divider
            epd_paint_draw_horizontal_line(epd_paint, x, y, list_view->width, 1);
            y += 1;
        }

        // selected item
        if (index == list_view->current_index) {
            epd_paint_reverse_range(epd_paint,
                                    x, item_start_y + PADDING_TOP,
                                    list_view->width,
                                    list_view->font->Height);
        }
        index++;
    }
}

void list_view_deinit(list_view_t *list_view) {
    struct list_view_element_t *head = list_view->head;
    struct list_view_element_t *ele = NULL;

    while (head != NULL) {
        ele = head;
        head = head->next;
        free(ele);
    }

    free(list_view);
}