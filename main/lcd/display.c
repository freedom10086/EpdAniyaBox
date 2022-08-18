#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <string.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "common.h"
#include "main_page.h"
#include "test_page.h"
#include "info_page.h"
#include "epd_lcd_ssd1680.h"
#include "epdpaint.h"
#include "key.h"
#include "display.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "DISPLAY"

#if defined CONFIG_IDF_TARGET_ESP32C3
#define TFT_SPI_HOST    SPI2_HOST

#define DISP_SPI_MISO  -1
#define DISP_SPI_MOSI  10
#define DISP_SPI_CLK 4
#define DISP_PIN_RST 7
#define DISP_SPI_CS 5
#define DISP_PIN_DC 6
#define DISP_PIN_BUSY 8

#elif CONFIG_IDF_TARGET_ESP32S3

#define TFT_SPI_HOST SPI2_HOST
#define DISP_SPI_MISO  CONFIG_DISP_SPI_MISO
#define DISP_SPI_MOSI CONFIG_DISP_SPI_MOSI
#define DISP_SPI_CLK CONFIG_DISP_SPI_CLK
#define DISP_PIN_RST CONFIG_DISP_PIN_RST
#define DISP_SPI_CS CONFIG_DISP_SPI_CS
#define DISP_PIN_DC CONFIG_DISP_PIN_DC
#define DISP_PIN_BUSY CONFIG_DISP_PIN_BUSY
#endif

#define DISP_BUFF_SIZE LCD_H_RES * LCD_V_RES

ESP_EVENT_DEFINE_BASE(BIKE_REQUEST_UPDATE_DISPLAY_EVENT);

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void guiTask(void *pvParameter);

static TaskHandle_t xTaskToNotify = NULL;
const UBaseType_t xArrayIndex = 0;

static uint8_t current_page_index = 1;
static page_inst_t pages[3] = {
        [0] = {
                .page_name = "main_page",
                .draw_cb = main_page_draw,
        },
        [1] = {
                .page_name = "info_page",
                .draw_cb = info_page_draw,
                .key_click_cb = info_page_key_click,
        },
        [2] = {
                .page_name = "test_page",
                .draw_cb = test_page_draw,
        }
};

bool spi_driver_init(int host,
                     int miso_pin, int mosi_pin, int sclk_pin,
                     int max_transfer_sz,
                     int dma_channel) {

    ESP_LOGI(TAG, "Initialize SPI bus");

    assert((0 <= host));
    const char *spi_names[] = {
            "SPI1_HOST", "SPI2_HOST", "SPI3_HOST"
    };

    ESP_LOGI(TAG, "Configuring SPI host %s [%d]", spi_names[host], host);
    ESP_LOGI(TAG, "MISO pin: %d, MOSI pin: %d, SCLK pin: %d", miso_pin, mosi_pin, sclk_pin);

    ESP_LOGI(TAG, "Max transfer size: %d (bytes)", max_transfer_sz);

    spi_bus_config_t buscfg = {
            .miso_io_num = miso_pin,
            .mosi_io_num = mosi_pin,
            .sclk_io_num = sclk_pin,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = max_transfer_sz
    };

    ESP_LOGI(TAG, "Initializing SPI bus...");
#if defined (CONFIG_IDF_TARGET_ESP32C3)
    dma_channel = SPI_DMA_CH_AUTO;
#elif defined (CONFIG_IDF_TARGET_ESP32S3)
    dma_channel = SPI_DMA_CH_AUTO;
#endif
    ESP_LOGI(TAG, "SPI DMA channel %d", dma_channel);
    esp_err_t ret = spi_bus_initialize(host, &buscfg, (spi_dma_chan_t) dma_channel);
    assert(ret == ESP_OK);

    return ESP_OK != ret;
}

void switch_page() {

}

void draw_page(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    page_inst_t current_page = pages[current_page_index];
    current_page.draw_cb(epd_paint, loop_cnt);
}

static void guiTask(void *pvParameter) {

    (void) pvParameter;
    xTaskToNotify = xTaskGetCurrentTaskHandle();

    spi_driver_init(TFT_SPI_HOST,
                    DISP_SPI_MISO, DISP_SPI_MOSI, DISP_SPI_CLK,
                    DISP_BUFF_SIZE * sizeof(uint8_t), SPI_DMA_CH_AUTO);

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
            .dc_gpio_num = DISP_PIN_DC,
            .cs_gpio_num = DISP_SPI_CS,
            .pclk_hz = 400000,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .spi_mode = 0,
            .trans_queue_depth = 8,
            //.on_color_trans_done = ,
    };

    lcd_ssd1680_panel_t panel = {
            .busy_gpio_num = DISP_PIN_BUSY,
            .reset_gpio_num = DISP_PIN_RST,
            .reset_level = 0,
    };

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(new_panel_ssd1680(&panel, TFT_SPI_HOST, &io_config));

    ESP_LOGI(TAG, "Reset SSD1680 panel driver");
    panel_ssd1680_reset(&panel);
    panel_ssd1680_init_full(&panel);
    // panel_ssd1680_init_partial(&panel);

    //panel_ssd1680_clear_display(panel, 0xff);

    // for test
    epd_paint_t *epd_paint = malloc(sizeof(epd_paint_t));
    uint8_t *image = malloc(sizeof(uint8_t) * LCD_H_RES * LCD_V_RES / 8);
    if (!image) {
        ESP_LOGE(TAG, "no memory for display driver");
        return;
    }

    epd_paint_init(epd_paint, image, LCD_H_RES, LCD_V_RES);

    epd_paint_clear(epd_paint, 0);
    panel_ssd1680_clear_display(&panel, 0xff);

    //panel_ssd1680_draw_bitmap(&panel, 0, 0, LCD_H_RES, LCD_V_RES, epd_paint->image);
    //panel_ssd1680_refresh(&panel, false);

    panel_ssd1680_init_partial(&panel);

    uint32_t loop_cnt = 0;
    static uint32_t last_full_refresh_tick;
    static uint32_t current_tick;

    while (1) {
        ESP_LOGI(TAG, "draw loop %ld", loop_cnt);
        loop_cnt += 1;
        current_tick = xTaskGetTickCount();

        if (loop_cnt % 20 == 0 || current_tick - last_full_refresh_tick >= configTICK_RATE_HZ * 300) {
            // request full fresh
            panel_ssd1680_init_full(&panel);
            panel_ssd1680_clear_display(&panel, 0xff);

            panel_ssd1680_init_partial(&panel);
            last_full_refresh_tick = current_tick;
        }

        draw_page(epd_paint, loop_cnt);

        panel_ssd1680_draw_bitmap(&panel, 0, 0, LCD_H_RES, LCD_V_RES, epd_paint->image);
        panel_ssd1680_refresh(&panel, true);

        uint32_t ulNotificationValue = ulTaskNotifyTakeIndexed(xArrayIndex, pdTRUE, pdMS_TO_TICKS(30000));
        ESP_LOGI(TAG, "ulTaskGenericNotifyTake %ld", ulNotificationValue);
        if (ulNotificationValue > 0) { // may > 1 more data ws send
            /* The transmission ended as expected. */
            //gpio_isr_handler_remove(zw800_dev->touch_pin);
        } else {
            /* The call to ulTaskNotifyTake() timed out. */
        }
        // vTaskDelay(pdTICKS_TO_MS(30000));
    }

    panel_ssd1680_sleep(&panel);
    epd_paint_deinit(epd_paint);
    free(image);
    free(epd_paint);

    vTaskDelete(NULL);
}

void request_display_update_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                                    void *event_data) {
    if (BIKE_REQUEST_UPDATE_DISPLAY_EVENT == event_base) {
        ESP_LOGI(TAG, "request for update...");
        int *full_update = (int *) event_data;
        xTaskGenericNotify(xTaskToNotify, xArrayIndex, *full_update,
                           eNoAction, NULL);
    }
}

static void key_click_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                                    void *event_data) {
    ESP_LOGI(TAG, "rev key click event %ld", event_id);
    switch (event_id) {
        case KEY_1_SHORT_CLICK:
            break;
        case KEY_2_SHORT_CLICK:
            break;
        default:
            break;
    }
    // if not handle passed to view
    page_inst_t current_page = pages[current_page_index];
    if (current_page.key_click_cb) {
        current_page.key_click_cb(event_id);
    }
}

void display_init() {
    xTaskCreatePinnedToCore(guiTask, "gui", 4096 * 2, NULL, 0, NULL, 1);

    // key click event
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_KEY_EVENT, ESP_EVENT_ANY_ID,
                                    key_click_event_handler, NULL);

    // update display
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_REQUEST_UPDATE_DISPLAY_EVENT, ESP_EVENT_ANY_ID,
                                    request_display_update_handler, NULL);
}

