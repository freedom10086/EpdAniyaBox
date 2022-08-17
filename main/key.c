#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "key.h"


#define TAG "keyboard"

const gpio_num_t KEY_1_NUM = 0;
const gpio_num_t KEY_2_NUM = 9;

static TaskHandle_t xTaskToNotify = NULL;
const UBaseType_t xArrayIndex = 0;

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    // xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Notify the task that the transmission is complete. */
    vTaskNotifyGiveIndexedFromISR(xTaskToNotify, xArrayIndex, &xHigherPriorityTaskWoken);
}

static void key_task_entry(void *arg) {
    xTaskToNotify = xTaskGetCurrentTaskHandle();
    while (1) {
        //if (!gpio_get_level(zw800_dev->touch_pin)) {

        /* Wait to be notified that the transmission is complete.  Note the first parameter is pdTRUE, which has the effect of clearing
        the task's notification value back to 0, making the notification value act like a binary (rather than a counting) semaphore.  */
        uint32_t ulNotificationValue = ulTaskNotifyTakeIndexed(xArrayIndex,
                                                               pdTRUE,
                                                               pdMS_TO_TICKS(5000));
        //ESP_LOGI(TAG, "ulTaskGenericNotifyTake %ld", ulNotificationValue);
        if (ulNotificationValue == 1) { // may > 1 more data ws send
            /* The transmission ended as expected. */
            //gpio_isr_handler_remove(zw800_dev->touch_pin);
            ESP_LOGI(TAG, "key click detect...");
        } else {
            /* The call to ulTaskNotifyTake() timed out. */
        }
        vTaskDelay(pdMS_TO_TICKS(5));
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
            .intr_type = GPIO_INTR_NEGEDGE,
            .pull_up_en = 1,
            .pull_down_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    ESP_LOGI(TAG, "io %d, level %d", KEY_1_NUM, gpio_get_level(KEY_1_NUM));
    ESP_LOGI(TAG, "io %d, level %d", KEY_2_NUM, gpio_get_level(KEY_2_NUM));

    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(KEY_1_NUM, gpio_isr_handler, (void *) KEY_1_NUM);
    gpio_isr_handler_add(KEY_2_NUM, gpio_isr_handler, (void *) KEY_2_NUM);
}