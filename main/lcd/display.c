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
#include "esp_sleep.h"
#include "driver/rtc_io.h"

#include "bike_common.h"
#include "epd_lcd_ssd1680.h"
#include "epdpaint.h"
#include "key.h"
#include "display.h"
#include "page_manager.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "DISPLAY"

#define TFT_SPI_HOST SPI2_HOST
#define DISP_SPI_MISO  CONFIG_DISP_SPI_MISO
#define DISP_SPI_MOSI CONFIG_DISP_SPI_MOSI
#define DISP_SPI_CLK CONFIG_DISP_SPI_CLK
#define DISP_PIN_RST CONFIG_DISP_PIN_RST
#define DISP_SPI_CS CONFIG_DISP_SPI_CS
#define DISP_PIN_DC CONFIG_DISP_PIN_DC
#define DISP_PIN_BUSY CONFIG_DISP_PIN_BUSY

#define DISP_BUFF_SIZE LCD_H_RES * LCD_V_RES

ESP_EVENT_DEFINE_BASE(BIKE_REQUEST_UPDATE_DISPLAY_EVENT);

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void guiTask(void *pvParameter);

static TaskHandle_t xTaskToNotify = NULL;
static uint32_t boot_cnt = 0;

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

void enter_deep_sleep(int sleep_ts, lcd_ssd1680_panel_t *panel) {
    panel_ssd1680_sleep(panel);

    esp_sleep_enable_timer_wakeup(sleep_ts * 1000000);
    //esp_sleep_enable_ext1_wakeup(1 << KEY_1_NUM, ESP_EXT1_WAKEUP_ALL_LOW);
#ifdef SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
    const gpio_config_t config = {
            .pin_bit_mask = 1 << KEY_1_NUM,
            .mode = GPIO_MODE_INPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup(1 << KEY_1_NUM, ESP_GPIO_WAKEUP_GPIO_LOW));
    printf("Enabling GPIO wakeup on pins GPIO%d\n", KEY_1_NUM);
#else
    esp_sleep_enable_ext0_wakeup(KEY_1_NUM, 0);
    rtc_gpio_pullup_en(KEY_1_NUM);
    rtc_gpio_pulldown_dis(KEY_1_NUM);
#endif

    ESP_LOGI(TAG, "enter deep sleep mode");
    esp_deep_sleep_start();
}

void draw_page(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    page_inst_t current_page = page_manager_get_current_page();
    current_page.on_draw_page(epd_paint, loop_cnt);
}

static void guiTask(void *pvParameter) {
    page_manager_init("temperature");

    xTaskToNotify = xTaskGetCurrentTaskHandle();
    spi_driver_init(TFT_SPI_HOST,
                    DISP_SPI_MISO, DISP_SPI_MOSI, DISP_SPI_CLK,
                    DISP_BUFF_SIZE, SPI_DMA_CH_AUTO);

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
            .dc_gpio_num = DISP_PIN_DC,
            .cs_gpio_num = DISP_SPI_CS,
            .pclk_hz = 10 * 1000 * 1000, // 10M
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .spi_mode = 0,
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

    // for test
    epd_paint_t *epd_paint = malloc(sizeof(epd_paint_t));

    //uint8_t *image = malloc(sizeof(uint8_t) * LCD_H_RES * LCD_V_RES / 8);
    uint8_t *image = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint8_t) / 8, MALLOC_CAP_DMA);
    if (!image) {
        ESP_LOGE(TAG, "no memory for display driver");
        return;
    }

    epd_paint_init(epd_paint, image, LCD_H_RES, LCD_V_RES);
    epd_paint_clear(epd_paint, 0);

    panel_ssd1680_init_full(&panel);
    if (boot_cnt - 1 % 10 == 0) {
        // every 10 boot clear one display
        panel_ssd1680_clear_display(&panel, 0xff);
    }

    static uint32_t loop_cnt = 1;
    uint32_t last_full_refresh_loop_cnt = loop_cnt;
    uint32_t last_full_refresh_tick = xTaskGetTickCount();
    static uint32_t current_tick;
    static uint32_t ulNotificationCount;
    static uint32_t continue_time_out_count = 0;

    //sleep wait for sensor init
    vTaskDelay(pdMS_TO_TICKS(50));

    while (1) {
        // not first loop
        if (loop_cnt > 1 && esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
            ulNotificationCount = ulTaskGenericNotifyTake(0, pdTRUE, pdMS_TO_TICKS(60000));
            ESP_LOGI(TAG, "ulTaskGenericNotifyTake %ld", ulNotificationCount);
            if (ulNotificationCount > 0) { // may > 1 more data ws send
                continue_time_out_count = 0;
            } else {
                /* The call to ulTaskNotifyTake() timed out. */
                continue_time_out_count += 1;
            }
        }

        ESP_LOGI(TAG, "draw loop: %ld, boot_cnt: %ld ", loop_cnt, boot_cnt);
        current_tick = xTaskGetTickCount();

        draw_page(epd_paint, loop_cnt);

        // use partial update mode
        // less continue 60 times partial refresh mode or last full update time less 30min and not first loop
        bool use_partial_update_mode = loop_cnt != 1
                                       && loop_cnt - last_full_refresh_loop_cnt < 60
                                       && current_tick - last_full_refresh_tick < configTICK_RATE_HZ * 1800;
        if (panel._using_partial_mode != use_partial_update_mode) {
            if (use_partial_update_mode) {
                panel_ssd1680_init_partial(&panel);
            } else {
                panel_ssd1680_init_full(&panel);
            }
        }

        panel_ssd1680_draw_bitmap(&panel, 0, 0, LCD_H_RES, LCD_V_RES, epd_paint->image);
        panel_ssd1680_refresh(&panel, use_partial_update_mode);
        if (!use_partial_update_mode) {
            last_full_refresh_tick = current_tick;
            last_full_refresh_loop_cnt = loop_cnt;
        }

        loop_cnt += 1;

        // enter deep sleep mode
        if (continue_time_out_count >= 2 || esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
            enter_deep_sleep(120, &panel);
        }
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
        //ESP_LOGI(TAG, "request for update...");
        int *full_update = (int *) event_data;
        xTaskGenericNotify(xTaskToNotify, 0, *full_update,
                           eIncrement, NULL);
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
    page_inst_t current_page = page_manager_get_current_page();
    if (current_page.key_click_handler) {
        if (current_page.key_click_handler(event_id)) {
            return;
        }
    }

    // if page not handle key click event here handle
    ESP_LOGI(TAG, "no page handler key click event %ld", event_id);
}

void display_init(uint32_t boot_count) {
    boot_cnt = boot_count;

    // uxPriority 0 最低
    xTaskCreatePinnedToCore(guiTask, "gui", 4096 * 2, NULL, 1, NULL, 1);

    // key click event
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_KEY_EVENT, ESP_EVENT_ANY_ID,
                                    key_click_event_handler, NULL);

    // update display event
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_REQUEST_UPDATE_DISPLAY_EVENT, ESP_EVENT_ANY_ID,
                                    request_display_update_handler, NULL);
}

