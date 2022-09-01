#include "bike_common.h"
#include "esp_log.h"
#include "nvs_flash.h"

static bool nvs_inited = false;

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