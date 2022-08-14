#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "lcd/main_page.h"
#include "nmea_parser.h"
#include "ble/ble_device.h"
#include "sd_card.h"
#include "tools/kalman_filter.h"
#include "ms5611.h"
#include "led/ws2812.h"
#include "spl06.h"
#include "lcd/display.h"
#include "zw800.h"
#include "sht31.h"

static const char *TAG = "BIKE_MAIN";

#define TIME_ZONE (+8)   //Beijing Time
#define YEAR_BASE (2000) //date in GPS starts from 2000

esp_event_loop_handle_t event_loop_handle;

static void application_task(void *args) {
    while (1) {
        // ESP_LOGI(TAG, "application_task: running application task");
        esp_event_loop_run(event_loop_handle, 100);
        vTaskDelay(10);
    }
}

static void
gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    gps_t *gps = NULL;
    switch (event_id) {
        case GPS_UPDATE:
            gps = (gps_t *) event_data;
            /* print information parsed from GPS statements */
            ESP_LOGI(TAG, "%d/%d/%d %d:%d:%d => \r\n"
                          "\t\t\t\t\t\tvalid   = %d\r\n"
                          "\t\t\t\t\t\tlatitude   = %.05f°N\r\n"
                          "\t\t\t\t\t\tlongitude = %.05f°E\r\n"
                          "\t\t\t\t\t\taltitude   = %.02fm\r\n"
                          "\t\t\t\t\t\tspeed      = %fm/s",
                     gps->date.year + YEAR_BASE, gps->date.month, gps->date.day,
                     gps->tim.hour + TIME_ZONE, gps->tim.minute, gps->tim.second,
                     gps->valid,
                     gps->latitude, gps->longitude, gps->altitude, gps->speed);
            break;
        case GPS_UNKNOWN:
            /* print unknown statements */
            ESP_LOGW(TAG, "Unknown statement:%s", (char *) event_data);
            break;
        default:
            break;
    }
}

static void
pressure_sensor_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data) {
    spl06_data_t *data = NULL;
    switch (event_id) {
        case SPL06_SENSOR_UPDATE:
            data = (spl06_data_t *) event_data;
            ESP_LOGI(TAG, "pressure: %.2f,temp: %.2f, altitude: %.2f", data->pressure, data->temp, data->altitude);
            // main_page_update_temperature(data->temp);
            main_page_update_altitude(data->altitude);
            break;
        default:
            break;
    }
}

static void
ble_csc_sensor_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                             void *event_data) {
    ble_csc_data_t *data = NULL;
    switch (event_id) {
        case BLE_CSC_SENSOR_UPDATE:
            data = (ble_csc_data_t *) event_data;
            ESP_LOGI(TAG, "wheel_total_distance: %.2f,\r\n"
                          "wheel_cadence: %.2f,\r\n"
                          "wheel_speed: %.2f\r\n"
                          "crank_cadence: %.2f\r\n",
                     data->wheel_total_distance,
                     data->wheel_cadence,
                     data->wheel_speed,
                     data->crank_cadence);
            main_page_update_speed(data->wheel_speed);
            main_page_update_crank_cadence(data->crank_cadence);
            break;
        default:
            break;
    }
}

static void
ble_hrm_sensor_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                             void *event_data) {
    ble_hrm_data_t *data = NULL;
    switch (event_id) {
        case BLE_HRM_SENSOR_UPDATE:
            data = (ble_hrm_data_t *) event_data;
            ESP_LOGI(TAG, "heart_rate: %d", data->heart_rate);
            main_page_update_heart_rate(data->heart_rate);
            break;
        default:
            break;
    }
}

static void
ble_temp_sensor_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data) {
    sht31_data_t *data = NULL;
    switch (event_id) {
        case SHT31_SENSOR_UPDATE:
            data = (sht31_data_t *) event_data;
            ESP_LOGI(TAG, "temp: %f, hum: %f", data->temp, data->hum);
            main_page_update_temp_hum(data->temp, data->hum);
            break;
        default:
            break;
    }
}

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {
    // esp_log_level_set("*", ESP_LOG_WARN);

    printf("Hello world!\n");

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

    printf("%uMB %s flash\n", flash_size / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

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

//    /* NMEA parser configuration */
//    nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
//    /* init NMEA parser library */
//    nmea_parser_handle_t nmea_hdl = nmea_parser_init(&config, event_loop_handle);
//    /* register event handler for NMEA parser library */
//    esp_event_handler_register_with(event_loop_handle, BIKE_GPS_EVENT, ESP_EVENT_ANY_ID,
//                                    gps_event_handler, NULL);
    /* deinit NMEA parser library */
    // nmea_parser_deinit(nmea_hdl);

    //ws2812_start();

    /**
     * lcd
     */
    display_init();

    /**
     *  sd card
     */
    //sd_card_init();

    /**
     *  init ble device
     */
//    ble_device_init(NULL);
//    esp_event_handler_register_with(event_loop_handle,
//                                    BIKE_BLE_HRM_SENSOR_EVENT, ESP_EVENT_ANY_ID,
//                                    ble_hrm_sensor_event_handler, NULL);
//    esp_event_handler_register_with(event_loop_handle,
//                                    BIKE_BLE_CSC_SENSOR_EVENT, ESP_EVENT_ANY_ID,
//                                    ble_csc_sensor_event_handler, NULL);

//    spl06_t *spl06 = spl06_init(event_loop_handle);
//    esp_event_handler_register_with(event_loop_handle,
//                                    BIKE_PRESSURE_SENSOR_EVENT, ESP_EVENT_ANY_ID,
//                                    pressure_sensor_event_handler, NULL);

    // zw800 finger print sensor
//    zw800_config_t zw800_config = ZW800_CONFIG_DEFAULT();
//    zw800_init(&zw800_config, event_loop_handle);

    /**
     * sht31
     */
    sht31_t *sht31 = sht31_init(event_loop_handle);
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_TEMP_HUM_SENSOR_EVENT, ESP_EVENT_ANY_ID,
                                    ble_temp_sensor_event_handler, NULL);
}