#include <stdio.h>
#include <stdlib.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"

#include "common.h"
#include "status_bar.h"

#define TAG "status_bar"

/**
 * @brief Define of NMEA Parser Event base
 *
 */
ESP_EVENT_DEFINE_BASE(STATUS_BAR_UPDATE_EVENT);

static void status_bar_update_event_handler(void *, esp_event_base_t, int32_t, void *);

esp_event_loop_handle_t init_status_bar(lv_obj_t * parent)
{

    esp_event_loop_args_t loop_args = {
        .queue_size = 20,
        .task_name = NULL
    };
    
    esp_event_loop_handle_t loop_handle;

    if (esp_event_loop_create(&loop_args, &loop_handle) != ESP_OK) {
        ESP_LOGE(TAG, "create event loop faild");
        return NULL;
    }

    // reg event
    if (esp_event_handler_register_with(loop_handle,
                                    STATUS_BAR_UPDATE_EVENT, ESP_EVENT_ANY_ID,
                                    status_bar_update_event_handler, NULL) != ESP_OK) {
        
        ESP_LOGE(TAG, "register event loop handler faild");
        return NULL;
    }

    ESP_LOGI(TAG, "init success!");
    return loop_handle;
}

void update_status_bar(esp_event_loop_handle_t loop_handle) {
    // push event to status bar
    ESP_LOGI(TAG, "push update event to status bar!");
    esp_event_post_to(loop_handle, STATUS_BAR_UPDATE_EVENT, ESP_EVENT_ANY_ID,
                                      NULL, 0, 100 / portTICK_PERIOD_MS);
}

esp_err_t deinit_status_bar(esp_event_loop_handle_t loop_handle)
{
    esp_event_handler_unregister_with(loop_handle,
        STATUS_BAR_UPDATE_EVENT, ESP_EVENT_ANY_ID,
        status_bar_update_event_handler);

    esp_err_t r = esp_event_loop_delete(loop_handle);
    ESP_LOGI(TAG, "deinit status bar!");
    return r;
}

static void status_bar_update_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG,  "rev status_bar_update_event, id: %d \n\n", event_id);
    status_bar_t *data = (status_bar_t *)event_data;
}