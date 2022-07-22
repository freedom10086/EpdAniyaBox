/* LVGL Example project
 *
 * Basic project to test LVGL on ESP32 based projects.
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else

#include "lvgl/lvgl.h"

#endif

#include "status_bar.h"
#include "main_page.h"
#include "nmea_parser.h"
#include "ble_device.h"
#include "sd_card.h"
#include "kalman_filter.h"
#include "ms5611.h"
#include "spl06.h"

static const char *TAG = "BIKE_MAIN";

#define TIME_ZONE (+8)   //Beijing Time
#define YEAR_BASE (2000) //date in GPS starts from 2000

/**
 * @brief GPS Event Handler
 *
 * @param event_handler_arg handler specific arguments
 * @param event_base event base, here is fixed to ESP_NMEA_EVENT
 * @param event_id event id
 * @param event_data event specific arguments
 */
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

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {
    // esp_log_level_set("*", ESP_LOG_WARN);
    /* NMEA parser configuration */
    nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
    /* init NMEA parser library */
    nmea_parser_handle_t nmea_hdl = nmea_parser_init(&config);
    /* register event handler for NMEA parser library */
    nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    /* unregister event handler */
    // nmea_parser_remove_handler(nmea_hdl, gps_event_handler);
    /* deinit NMEA parser library */
    // nmea_parser_deinit(nmea_hdl);

    /**
     * main page
     */
    init_main_page();

    /**
     * status bar
     */
    //esp_event_loop_handle_t status_bar_event_hdl = init_status_bar(NULL);

    //vTaskDelay(1000 / portTICK_PERIOD_MS);

    //update_status_bar(status_bar_event_hdl);

    //vTaskDelay(10000 / portTICK_PERIOD_MS);

    //deinit_status_bar(status_bar_event_hdl);

    /**
     *  sd card
     */
    //sd_card_init();

    /**
     *  init ble device
     */
    // esp_event_loop_handle_t ble_dev_evt_hdl = ble_device_init(NULL);

    /*
    // ms5611
    ms5611_init();

    float sumHeight = 0;
    for (int i = 0; i < 10; ++i) {
        if (i == 0) {
            vTaskDelay(pdMS_TO_TICKS(2));
            ms5611_read_temp_pre();
            vTaskDelay(pdMS_TO_TICKS(20));
            ms5611_read_temp();
        }
        ms5611_read_pressure_pre();
        vTaskDelay(pdMS_TO_TICKS(25));
        ms5611_read_pressure();

        float height = ms5611_pressure_caculate();
        ESP_LOGI(TAG, "ms5611 height: %f", height);

        sumHeight += height;
    }

    ESP_LOGI(TAG, "avg height ms5611 height: %f", sumHeight / 10);

    */

    /*
    spl06_t spl06;
    spl06_init(&spl06);

    bool en_fifo = false;
    spl06_start(&spl06, en_fifo);

    while (1) {
        spl06_meassure_state(&spl06);
        spl06_fifo_state(&spl06);
        vTaskDelay(pdMS_TO_TICKS(30));

        if (spl06.en_fifo) {
            // read from fifo
            if (spl06.fifo_full) {
                spl06_read_raw_fifo(&spl06);
                ESP_LOGI(TAG, "spl06 read fifo %d", spl06.fifo_len);
                for (uint8_t i = 0; i < spl06.fifo_len; i++) {
                    bool is_pressure = spl06.fifo[i] & 0x01;
                    int32_t item = spl06.fifo[i]; // >> 1;
                    if (is_pressure) {
                        spl06.raw_pressure = item;
                        ESP_LOGI(TAG, "spl06 fifo pressure data %d %f, ", i, spl06_get_pressure(&spl06));
                    } else {
                        spl06.raw_temp = item;
                        ESP_LOGI(TAG, "spl06 fifo temp data %d %f, ", i, spl06_get_temperature(&spl06));
                    }
                }
            }
        } else {
            if (spl06.pressure_ready && spl06.raw_temp_valid) {
                spl06_read_raw_pressure(&spl06);
                float pressure = spl06_get_pressure(&spl06);
                //float kal_pressure = kalman1_filter(&state1, pressure);
                ESP_LOGI(TAG, "spl06 raw_pressure %d,  pressure: %f altitude:%f altitudeV2:%f",
                         spl06.raw_pressure, pressure, calc_altitude(pressure), calc_altitude_v2(pressure));
                //printf("pressure:%f,raw_pressure:%d,kal_kal_pressure:%f\n", pressure, spl06.raw_pressure, kal_pressure);
            }

            if (spl06.temp_ready) {
                spl06_read_raw_temp(&spl06);
                float temp = spl06_get_temperature(&spl06);
                ESP_LOGI(TAG, "spl06 raw_temp %d,  temp: %f", spl06.raw_temp, temp);
                //printf("temp:%f\n", temp);
            }
        }
    }
     */
}