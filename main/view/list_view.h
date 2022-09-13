#ifndef LIST_VIEW_H
#define LIST_VIEW_H

#include <stdio.h>
#include <stdlib.h>

#include "lcd/epdpaint.h"

struct list_view_element_t {
    char *text;
    struct list_view_element_t *next;
};

typedef struct {
    int element_count;
    struct list_view_element_t *head;
    int current_index;
    int x, y;
    int width, height;
    sFONT *font;
} list_view_t;

list_view_t *list_vew_create(int x, int y, int width, int height, sFONT *font);

void list_view_set_position(list_view_t *list_view, int x, int y);

int get_select_index(list_view_t *list_view);

int set_select_index(list_view_t *list_view, int index);

void list_view_add_element(list_view_t *list_view, char *text);

void list_view_remove_element(list_view_t *list_view, uint8_t index);

void list_view_update_item(list_view_t *list_view, uint8_t index, char *newText);

void list_view_remove_first_element(list_view_t *list_view);

void list_view_remove_last_element(list_view_t *list_view);

void list_vew_draw(list_view_t *list_view, epd_paint_t *epd_paint, uint32_t loop_cnt);

void list_view_deinit(list_view_t *list_view);

#endif