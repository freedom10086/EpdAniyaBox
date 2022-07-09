/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <stdlib.h>
#include <sys/cdefs.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_check.h"

#include "esp_lcd_panel_ssd1680.h"

static const char *TAG = "lcd_panel.ssd1680";

static const uint8_t lut_full_update[] = {
        0x50, 0xAA, 0x55, 0xAA, 0x11, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xFF, 0xFF, 0x1F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//static const uint8_t lut_partial_update[] = {
//        0x10, 0x18, 0x18, 0x08, 0x18, 0x18,
//        0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
//        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//        0x00, 0x00, 0x13, 0x14, 0x44, 0x12,
//        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
//};

// ssd1680 commands
#define SSD1680_CMD_DRIVER_OUTPUT_CONTROL 0x01
#define SSD1680_CMD_BOOSTER_SOFT_START_CONTROL  0x0C
#define SSD1680_CMD_DEEP_SLEEP_MODE       0x10
#define SSD1680_CMD_DATA_ENTRY_MODE_SETTING 0x11
#define SSD1680_CMD_SOFT_RESET            0x12
#define SSD1680_CMD_TEMPERATURE_SENSOR_CONTROL 0x18
#define SSD1680_CMD_MASTER_ACTIVATION  0x20
#define SSD1680_CMD_DISPLAY_UPDATE_CONTROL_1      0x21
#define SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2      0x22
#define SSD1680_CMD_WRITE_RAM              0x24
#define SSD1680_CMD_WRITE_VCOM_REGISTER              0x2C
#define SSD1680_CMD_BORDER_WAVEFORM_CONTROL      0x3C
#define SSD1680_CMD_SET_RAM_X_START_END      0x44
#define SSD1680_CMD_SET_RAM_Y_START_END      0x45
#define SSD1680_CMD_SET_RAM_X_ADDRESS_COUNTER      0x4E
#define SSD1680_CMD_SET_RAM_Y_ADDRESS_COUNTER      0x4F


void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc=(int)t->user;
    gpio_set_level(CONFIG_DISP_PIN_DC, dc);
}


static void lcd_spi_post_trans_callback(spi_transaction_t *trans)
{
}

esp_err_t new_panel_ssd1680(lcd_ssd1680_panel_t *panel,
                            spi_host_device_t bus,
                            const esp_lcd_panel_io_spi_config_t *io_config) {

    esp_err_t ret = ESP_OK;
    if (panel->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
                .mode = GPIO_MODE_OUTPUT,
                .pin_bit_mask = 1ULL << panel->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    if (panel -> busy_gpio_num >= 0) {
        // setup gpio
        ESP_LOGI(TAG, "busy pin is %d", panel -> busy_gpio_num);
        gpio_config_t io_config = {
                .pin_bit_mask = (1ull << panel -> busy_gpio_num),
                .mode = GPIO_MODE_INPUT,
        };
        ESP_ERROR_CHECK(gpio_config(&io_config));
    }

    // at the moment, the hardware doesn't support 9-bit SPI LCD, but in the future, there will be such a feature
    // so for now, we will force the user to use the GPIO to control the LCD's D/C line.
    ESP_GOTO_ON_FALSE(io_config->dc_gpio_num >= 0, ESP_ERR_INVALID_ARG, err, TAG, "invalid DC mode");

    spi_device_interface_config_t devcfg = {
            // currently the driver only supports TX path, so half duplex is enough
            .flags = SPI_DEVICE_HALFDUPLEX | (io_config->flags.lsb_first ? SPI_DEVICE_TXBIT_LSBFIRST : 0),
            .clock_speed_hz = io_config->pclk_hz,
            .mode = io_config->spi_mode,
            .spics_io_num = io_config->cs_gpio_num,
            .queue_size = io_config->trans_queue_depth,
            .pre_cb = lcd_spi_pre_transfer_callback, // pre-transaction callback, mainly control DC gpio level
            .post_cb = io_config->on_color_trans_done ? lcd_spi_post_trans_callback : NULL, // post-transaction, where we invoke user registered "on_color_trans_done()"
    };

    spi_device_handle_t spi_dev;
    ret = spi_bus_add_device((spi_host_device_t)bus, &devcfg, &spi_dev);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "adding spi device to bus failed");

    panel->spi_dev = spi_dev;

    // if the DC line is not encoded into any spi transaction phase or it's not controlled by SPI peripheral
    if (io_config->dc_gpio_num >= 0) {
        gpio_config_t io_conf = {
                .mode = GPIO_MODE_OUTPUT,
                .pin_bit_mask = 1ULL << io_config->dc_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for D/C line failed");
    }

    return ESP_OK;

    err:
    if (panel->reset_gpio_num >= 0) {
        gpio_reset_pin(panel->reset_gpio_num);
    }
    if (panel->busy_gpio_num >= 0) {
        gpio_reset_pin(panel->busy_gpio_num);
    }
    if (io_config->dc_gpio_num >= 0) {
        gpio_reset_pin(io_config->dc_gpio_num);
    }
    return ret;
}

static esp_err_t lcd_cmd(lcd_ssd1680_panel_t *panel, const uint8_t cmd, const void *param, size_t param_size)
{

    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_polling_transmit(panel->spi_dev, &t);  //Transmit!
    ESP_GOTO_ON_ERROR(ret, err, TAG, "spi transmit (polling) cmd failed");

    if (param && param_size) {
        memset(&t, 0, sizeof(t));       //Zero out the transaction
        t.length=param_size;                     //Command is 8 bits
        t.tx_buffer=&cmd;               //The data is the cmd itself
        t.user=(void*)1;                //D/C needs to be set to 0
        ret=spi_device_polling_transmit(panel->spi_dev, &t);  //Transmit!
        ESP_GOTO_ON_ERROR(ret, err, TAG, "spi transmit (polling) param failed");
    }

    err:
    return ret;
}

static esp_err_t lcd_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    ESP_GOTO_ON_ERROR(ret, err, TAG, "spi transmit (polling) data failed");

    err:
    return ret;
}

esp_err_t panel_ssd1680_del(lcd_ssd1680_panel_t *panel) {
    if (panel->reset_gpio_num >= 0) {
        gpio_reset_pin(panel->reset_gpio_num);
    }
    if (panel->busy_gpio_num >= 0) {
        gpio_reset_pin(panel->busy_gpio_num);
    }

    // enter deep sleep mode
    lcd_cmd(panel, SSD1680_CMD_DEEP_SLEEP_MODE, (uint8_t[]) {0x011}, 1);

    ESP_LOGD(TAG, "del ssd1680 panel @%p", panel);
    free(panel);
    return ESP_OK;
}

static void wait_for_busy(lcd_ssd1680_panel_t *ssd1680) {
    //• Wait BUSY Low
    ESP_LOGI(TAG, "ssd1680 wait for busy ...");
    if (ssd1680->busy_gpio_num >= 0) {
        while (1) {
            if (gpio_get_level(ssd1680->busy_gpio_num)) {
                vTaskDelay(pdMS_TO_TICKS(5));
            } else {
                break;
            }
        }
    } else {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t panel_ssd1680_reset(lcd_ssd1680_panel_t *panel) {
    ESP_LOGI(TAG, "lcd panel start reset.");

    // perform hardware reset
    if (panel->reset_gpio_num >= 0) {
        gpio_set_level(panel->reset_gpio_num, !panel->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(panel->reset_gpio_num, panel->reset_level);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_set_level(panel->reset_gpio_num, !panel->reset_level);
        vTaskDelay(pdMS_TO_TICKS(5));
        wait_for_busy(panel);
    }

    ESP_LOGI(TAG, "lcd panel hw reset.");

    return ESP_OK;
}

esp_err_t panel_ssd1680_sw_reset(lcd_ssd1680_panel_t *panel) {
    ESP_LOGI(TAG, "lcd panel start reset.");

    // sw reset
    lcd_cmd(panel, SSD1680_CMD_SOFT_RESET, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    //wait_for_busy(ssd1680);

    ESP_LOGI(TAG, "lcd panel sw reset OK.");

    return ESP_OK;
}

static esp_err_t
set_mem_area(lcd_ssd1680_panel_t *ssd1680, uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y) {
    //0x0F-->(15+1)*8=128
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    lcd_cmd(ssd1680, SSD1680_CMD_SET_RAM_X_START_END,
                              (uint8_t[]) {(start_x >> 3) & 0xff, (end_x >> 3) & 0xff}, 2);

    lcd_cmd(ssd1680, SSD1680_CMD_SET_RAM_Y_START_END,
                              (uint8_t[]) {start_y & 0xff, (start_y >> 8) & 0xff, end_y & 0xff, (end_y >> 8) & 0xff},
                              4);

    return ESP_OK;
}

static esp_err_t set_mem_pointer(lcd_ssd1680_panel_t *ssd1680, uint16_t x, uint16_t y) {
    lcd_cmd(ssd1680, SSD1680_CMD_SET_RAM_X_ADDRESS_COUNTER, (uint8_t[]) {(x >> 3) & 0xff}, 1);
    lcd_cmd(ssd1680, SSD1680_CMD_SET_RAM_Y_ADDRESS_COUNTER,
                              (uint8_t[]) {y & 0xff, (y >> 8) & 0xff}, 2);
    wait_for_busy(ssd1680);
    return ESP_OK;
}

static esp_err_t display_frame(lcd_ssd1680_panel_t *ssd1680) {
    lcd_cmd(ssd1680, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2, (uint8_t[]) {0xF7}, 1);
    // update
    lcd_cmd(ssd1680, SSD1680_CMD_MASTER_ACTIVATION, NULL, 0);
    wait_for_busy(ssd1680);
    return ESP_OK;
}

static esp_err_t display_frame_partial(lcd_ssd1680_panel_t *ssd1680) {
    lcd_cmd(ssd1680, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2, (uint8_t[]) {0x0F}, 1);
    // update
    lcd_cmd(ssd1680, SSD1680_CMD_MASTER_ACTIVATION, NULL, 0);
    wait_for_busy(ssd1680);
    return ESP_OK;
}

static esp_err_t clear_display(lcd_ssd1680_panel_t *panel, uint8_t color) {
    set_mem_area(panel, 0, 0, LCD_H_RES - 1, LCD_V_RES - 1);
    set_mem_pointer(panel, 0, 0);

    size_t len = LCD_H_RES * LCD_V_RES  / 8;
    for (int j = 0; j < LCD_V_RES; j++) {
        for (int i = 0; i < LCD_H_RES / 8; i++) {
            lcd_cmd(panel, SSD1680_CMD_WRITE_RAM, color, 1);
        }
    }

    display_frame(panel);
    return ESP_OK;
}

static void set_lut_by_host(lcd_ssd1680_panel_t *ssd1680, uint8_t *lut, uint8_t len) {
    lcd_cmd(ssd1680, 0x32, lut, len);
    wait_for_busy(ssd1680);
}

esp_err_t panel_ssd1680_init(lcd_ssd1680_panel_t *panel) {

    ESP_LOGI(TAG, "ssd1680 start for init ...");
    // HW Reset
    // panel_ssd1680_reset(panel);

    // SW Reset by Command 0x12
    panel_ssd1680_sw_reset(panel);

    // Wait 10ms busy is high when sw reset
    vTaskDelay(pdMS_TO_TICKS(5));
    wait_for_busy(panel);

    // 3. Send Initialization Code
    // Set gate driver output by Command 0x01
    lcd_cmd(panel, SSD1680_CMD_DRIVER_OUTPUT_CONTROL, (uint8_t[]) {(LCD_V_RES -1) & 0xff, ((LCD_V_RES - 1) >> 8) & 0xff, 0x00}, 3);
    wait_for_busy(panel);

    lcd_cmd(panel, SSD1680_CMD_BOOSTER_SOFT_START_CONTROL, (uint8_t[]) {0xD7, 0xD6, 0x9D}, 3);
    lcd_cmd(panel, SSD1680_CMD_WRITE_VCOM_REGISTER, (uint8_t[]) {0xA8}, 1);
    lcd_cmd(panel, 0x3A, (uint8_t[]) {0x01}, 1);
    lcd_cmd(panel, 0x3B, (uint8_t[]) {0x08}, 1);


    // Set display RAM size by Command 0x11, 0x44, 0x45
    // 00 –Y decrement, X decrement, //01 –Y decrement, X increment, //10 –Y increment, X decrement, //11 –Y increment, X increment [POR]
    lcd_cmd(panel, SSD1680_CMD_DATA_ENTRY_MODE_SETTING, (uint8_t[]) {0x03}, 1);


    // set lut
    set_lut_by_host(panel, lut_full_update, 30);


    set_mem_area(panel, 0, 0, LCD_H_RES - 1, LCD_V_RES - 1);

    // Set panel border by Command 0x3C
    // lcd_cmd(io, SSD1680_CMD_BORDER_WAVEFORM_CONTROL, (uint8_t[]) {0x80}, 1);

    //4. Load Waveform LUT
    //• Sense temperature by int/ext TS by Command 0x18
    //• Load waveform LUT from OTP by Command 0x22, 0x20 or by MCU
    lcd_cmd(panel, SSD1680_CMD_TEMPERATURE_SENSOR_CONTROL, (uint8_t[]) {0x80}, 1);

    lcd_cmd(panel, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_1, (uint8_t[]) {0x00, 0x80}, 2);

    //• Wait BUSY Low
    vTaskDelay(pdMS_TO_TICKS(10));

    // 5. Write Image and Drive Display Panel
    //• Write image data in RAM by Command 0x4E, 0x4F, 0x24, 0x26
    //• Set softstart setting by Command 0x0C
    //• Drive display panel by Command 0x22, 0x20
    set_mem_pointer(panel, 0, 0);

    //• Wait BUSY Low
    wait_for_busy(panel);


    clear_display(panel, 0xff);

    //set_lut_by_host(ssd1680, WS_20_30);
    ESP_LOGI(TAG, "ssd1680 panel init ok !");
    return ESP_OK;
}

/**
 * x_start, y_start include, x_end, y_end not include
 */
esp_err_t panel_ssd1680_draw_bitmap(lcd_ssd1680_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                           const void *color_data) {

    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");

    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    x_start &= 0xF8;
    uint16_t image_width = (x_end - x_start) & 0xF8;
    if (x_start + image_width > LCD_H_RES) {
        x_end = LCD_H_RES;
    }

    uint16_t image_height = y_end - y_start;
    if (y_start + image_height > LCD_V_RES) {
        y_end = LCD_V_RES;
    }

    set_mem_area(panel, x_start, y_start, x_end - 1, y_end - 1);
    set_mem_pointer(panel, x_start, y_start);

//    /* send the image data */
//    for (int j = 0; j < y_end - y_start; j++) {
//        for (int i = 0; i < (x_end - x_start) / 8; i++) {
//            //
//            //lcd_cmd(io, 0, color_data, len);
//            //SendData(image_buffer[i + j * (image_width / 8)]);
//            lcd_cmd(io, SSD1680_CMD_WRITE_RAM, color_data, 1);
//            color_data++;
//        }
//    }

    size_t len = (y_end - y_start) * (x_end - x_start) / 8;
    // transfer frame buffer
    lcd_cmd(panel, SSD1680_CMD_WRITE_RAM, color_data, len);

    display_frame(panel);

    return ESP_OK;
}