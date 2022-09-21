#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "bike_common.h"
#include "key.h"


#define TAG "keyboard"
#define KEY_CLICK_MIN_GAP 30

ESP_EVENT_DEFINE_BASE(BIKE_KEY_EVENT);

//static TaskHandle_t xTaskToNotify = NULL;
//const UBaseType_t xArrayIndex = 0;

static QueueHandle_t event_queue;

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;

//    task notification
//    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//    /* Notify the task that the transmission is complete. */
    // vTaskNotifyGiveIndexedFromISR(xTaskToNotify, xArrayIndex, &xHigherPriorityTaskWoken);

    xQueueSendFromISR(event_queue, &gpio_num, NULL);
}

static void key_task_entry(void *arg) {

    //xTaskToNotify = xTaskGetCurrentTaskHandle();
    event_queue = xQueueCreate(10, sizeof(gpio_num_t));

//    while (1) {
//
//        //if (!gpio_get_level(zw800_dev->touch_pin)) {
//
//        /* Wait to be notified that the transmission is complete.  Note the first parameter is pdTRUE, which has the effect of clearing
//        the task's notification value back to 0, making the notification value act like a binary (rather than a counting) semaphore.  */
//        uint32_t ulNotificationValueCount = ulTaskNotifyTakeIndexed(xArrayIndex,
//                                                               pdTRUE,
//                                                               pdMS_TO_TICKS(30000));
//        ESP_LOGI(TAG, "ulTaskGenericNotifyTake %ld", ulNotificationValueCount);
//        if (ulNotificationValueCount > 0) { // may > 1 more data ws send
//            /* The transmission ended as expected. */
//            //gpio_isr_handler_remove(zw800_dev->touch_pin);
//            ESP_LOGI(TAG, "key click detect...");
//        } else {
//            /* The call to ulTaskNotifyTake() timed out. */
//        }
//        vTaskDelay(pdMS_TO_TICKS(5));
//    }

    gpio_num_t clicked_gpio;
    static uint32_t tick_count[2];
    static uint32_t key_up_tick_count[2];
    uint32_t tick_diff, key_up_tick_diff;
    uint32_t time_diff_ms;

    while (1) {
        if (xQueueReceive(event_queue, &clicked_gpio, portMAX_DELAY)) {
            // vTaskDelay(pdMS_TO_TICKS(2));
            uint8_t index = KEY_1_NUM == clicked_gpio ? 0 : 1;

            if (gpio_get_level(clicked_gpio) == 0) {
                //ESP_LOGI(TAG, "key %d click down detect...", clicked_gpio);
            } else if (gpio_get_level(clicked_gpio) == 1) {
                //ESP_LOGI(TAG, "key %d click up detect...", clicked_gpio);
                tick_diff = xTaskGetTickCount() - tick_count[index];
                time_diff_ms = pdTICKS_TO_MS(tick_diff);

                key_up_tick_diff = xTaskGetTickCount() - key_up_tick_count[index];

                // configTICK_RATE_HZ = 1s
                if (tick_diff > configTICK_RATE_HZ * 0.5f) {
                    ESP_LOGI(TAG, "key %d long press", clicked_gpio);
                    esp_event_post_to(event_loop_handle,
                                      BIKE_KEY_EVENT,
                                      index == 0 ? KEY_1_LONG_CLICK : KEY_2_LONG_CLICK,
                                      &time_diff_ms,
                                      sizeof(time_diff_ms),
                                      100 / portTICK_PERIOD_MS);
                } else if (pdTICKS_TO_MS(key_up_tick_diff) > KEY_CLICK_MIN_GAP) {
                    ESP_LOGI(TAG, "key %d short press", clicked_gpio);
                    esp_event_post_to(event_loop_handle,
                                      BIKE_KEY_EVENT,
                                      index == 0 ? KEY_1_SHORT_CLICK : KEY_2_SHORT_CLICK,
                                      &time_diff_ms,
                                      sizeof(time_diff_ms),
                                      100 / portTICK_PERIOD_MS);
                } else {
                    ESP_LOGW(TAG, "key up to quickly gpio:%d, time diff:%ldms", clicked_gpio, pdTICKS_TO_MS(key_up_tick_diff));
                }

                key_up_tick_count[index] = xTaskGetTickCount();
            }

            tick_count[index] = xTaskGetTickCount();
        }
    }

    vTaskDelete(NULL);
}

void key_init() {
    TaskHandle_t tsk_hdl;
    /* Create NMEA Parser task */
    BaseType_t err = xTaskCreate(
            key_task_entry,
            "key_task",
            2048,
            NULL,
            uxTaskPriorityGet(NULL),
            &tsk_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "create key detect task failed");
    }

    ESP_LOGI(TAG, "keyboard task init OK");

    gpio_config_t io_config = {
            .pin_bit_mask = (1ull << KEY_1_NUM) | (1ull << KEY_2_NUM),
            .mode = GPIO_MODE_INPUT,
            .intr_type = GPIO_INTR_ANYEDGE,
            .pull_up_en = 1,
            .pull_down_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    //ESP_LOGI(TAG, "io %d, level %d", KEY_1_NUM, gpio_get_level(KEY_1_NUM));
    //ESP_LOGI(TAG, "io %d, level %d", KEY_2_NUM, gpio_get_level(KEY_2_NUM));

    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(KEY_1_NUM, gpio_isr_handler, (void *) KEY_1_NUM);
    gpio_isr_handler_add(KEY_2_NUM, gpio_isr_handler, (void *) KEY_2_NUM);
}