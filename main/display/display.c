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
#include "epd_lcd_ssd1680.h"
#include "epdpaint.h"


/*********************
 *      DEFINES
 *********************/
#define TAG "DISPLAY"

#define TFT_SPI_HOST 1
#define DISP_SPI_MISO CONFIG_DISP_SPI_MISO
#define DISP_SPI_MOSI CONFIG_DISP_SPI_MOSI
#define DISP_SPI_CLK CONFIG_DISP_SPI_CLK
#define DISP_PIN_RST CONFIG_DISP_PIN_RST
#define DISP_SPI_CS CONFIG_DISP_SPI_CS
#define DISP_PIN_DC CONFIG_DISP_PIN_DC
#define DISP_PIN_BUSY CONFIG_DISP_PIN_BUSY

#define DISP_BUFF_SIZE LCD_H_RES * LCD_V_RES

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void guiTask(void *pvParameter);


/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;

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

static void guiTask(void *pvParameter) {

    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    spi_driver_init(TFT_SPI_HOST,
                    DISP_SPI_MISO, DISP_SPI_MOSI, DISP_SPI_CLK,
                    DISP_BUFF_SIZE * sizeof(uint16_t), SPI_DMA_CH_AUTO);

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
            .dc_gpio_num = DISP_PIN_DC,
            .cs_gpio_num = DISP_SPI_CS,
            .pclk_hz = 400000,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .spi_mode = 0,
            .trans_queue_depth = 10,
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

    epd_paint_init(epd_paint, image, LCD_H_RES, LCD_V_RES);

    epd_paint_clear(epd_paint, 1);
    panel_ssd1680_clear_display(&panel, 0xff);

    //panel_ssd1680_draw_bitmap(&panel, 0, 0, LCD_H_RES, LCD_V_RES, epd_paint->image);
    //panel_ssd1680_refresh(&panel, false);

    panel_ssd1680_init_partial(&panel);

    uint32_t loop_cnt = 0;
    while (1) {
        loop_cnt += 1;
        if (loop_cnt % 100 == 0) {
            // request full fresh
            panel_ssd1680_init_full(&panel);
            panel_ssd1680_clear_display(&panel, 0xff);

            panel_ssd1680_init_partial(&panel);
            loop_cnt = 0;
        }

        main_page_draw(epd_paint, loop_cnt);

        panel_ssd1680_draw_bitmap(&panel, 0, 0, LCD_H_RES, LCD_V_RES, epd_paint->image);
        panel_ssd1680_refresh(&panel, true);

        vTaskDelay(pdMS_TO_TICKS(5000));
        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    panel_ssd1680_sleep(&panel);
    epd_paint_deinit(epd_paint);
    free(image);
    free(epd_paint);

    vTaskDelete(NULL);
}

void display_init() {
    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
         * Otherwise there can be problem such as memory corruption and so on.
         * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
    xTaskCreatePinnedToCore(guiTask, "gui", 4096 * 2, NULL, 0, NULL, 1);
}

