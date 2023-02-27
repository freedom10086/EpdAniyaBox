#include "bike_common.h"
#include "esp_log.h"
#include "nvs_flash.h"

static bool nvs_inited = false;

extern esp_event_loop_handle_t event_loop_handle;

inline esp_err_t common_post_event(esp_event_base_t event_base, int32_t event_id) {
    return esp_event_post_to(event_loop_handle, event_base, event_id,
                             NULL, 0, 100 / portTICK_PERIOD_MS);
}

inline esp_err_t
common_post_event_data(esp_event_base_t event_base, int32_t event_id, const void *event_data, size_t event_data_size) {
    return esp_event_post_to(event_loop_handle, event_base, event_id,
                             event_data, event_data_size, 100 / portTICK_PERIOD_MS);
}

esp_err_t common_init_nvs() {
    if (nvs_inited) {
        return ESP_OK;
    }

    nvs_inited = true;
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    return ret;
}