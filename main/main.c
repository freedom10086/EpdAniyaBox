#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_sleep.h"

#include "lcd/display.h"
#include "sht31.h"
#include "key.h"
#include "battery.h"
#include "bike_common.h"

static const char *TAG = "BIKE_MAIN";

esp_event_loop_handle_t event_loop_handle;

RTC_DATA_ATTR static uint32_t boot_count = 0;

static void application_task(void *args) {
    while (1) {
        esp_err_t err = esp_event_loop_run(event_loop_handle, portMAX_DELAY);
        if (err != ESP_OK) {
            break;
        }
    }

    ESP_LOGE(TAG, "suspended task for loop %p", event_loop_handle);
    vTaskSuspend(NULL);
}

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {
    esp_log_level_set("*", ESP_LOG_WARN);
    //esp_log_level_set("battery", ESP_LOG_WARN);
    //esp_log_level_set("lcd_panel.ssd1680", ESP_LOG_WARN);
    //esp_log_level_set("keyboard", ESP_LOG_WARN);
    //esp_log_level_set("display", ESP_LOG_WARN);

    boot_count++;
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "wake up by cause  %d", cause);

    printf("Hello world!, boot count %ld\n", boot_count);

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%luMB %s flash\n", flash_size / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %ld bytes\n", esp_get_minimum_free_heap_size());

    // create event loop
    esp_event_loop_args_t loop_args = {
            .queue_size = 16,
            .task_name = NULL // no task will be created
    };

    // Create the event loops
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &event_loop_handle));

    //esp_event_loop_delete(esp_gps->event_loop_hdl);

    ESP_LOGI(TAG, "starting application task");
    // Create the application task with the same priority as the current task
    xTaskCreate(application_task, "application_task", 3072, NULL, uxTaskPriorityGet(NULL), NULL);

    /**
     * key
     */
    key_init();

    /**
     * battery detect
     */
    battery_init();

    /**
     * lcd
     */
    display_init(boot_count);
}