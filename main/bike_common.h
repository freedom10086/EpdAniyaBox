#ifndef __COMMON_H
#define __COMMON_H

#include "esp_types.h"
#include "esp_event.h"
#include "esp_event_base.h"

#define max(a, b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b;       \
})

#define min(a, b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})


extern esp_event_loop_handle_t event_loop_handle;

inline esp_err_t post_event(esp_event_base_t event_base, int32_t event_id) {
    return esp_event_post_to(event_loop_handle, event_base, event_id,
                             NULL, 0, 100 / portTICK_PERIOD_MS);
}

inline esp_err_t
post_event_data(esp_event_base_t event_base, int32_t event_id, const void *event_data, size_t event_data_size) {
    return esp_event_post_to(event_loop_handle, event_base, event_id,
                             event_data, event_data_size, 100 / portTICK_PERIOD_MS);
}

esp_err_t common_init_nvs();

#endif