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

#include "lcd/epdpaint.h"
#include "epd_lcd_ssd1680.h"

static const char *TAG = "lcd_panel.ssd1680";

#define TRANSFER_QUEUE_SIZE 10

#ifdef CONFIG_SPI_DISPLAY_SSD1680_1IN54_V1
unsigned char WF_Full_1IN54[30] = {
        0x02, 0x02, 0x01, 0x11, 0x12, 0x12, 0x22, 0x22,
        0x66, 0x69, 0x69, 0x59, 0x58, 0x99, 0x99, 0x88,
        0x00, 0x00, 0x00, 0x00, 0xF8, 0xB4, 0x13, 0x51,
        0x35, 0x51, 0x51, 0x19, 0x01, 0x00};

unsigned char WF_PARTIAL_1IN54[30] = {
        0x10, 0x18, 0x18, 0x08, 0x18, 0x18, 0x08, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x13, 0x14, 0x44, 0x12,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#else
unsigned char WF_Full_1IN54[159] =
        {
                0x80, 0x48, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x40, 0x48, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x80, 0x48, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x40, 0x48, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0xA, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x8, 0x1, 0x0, 0x8, 0x1, 0x0, 0x2,
                0xA, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x0, 0x0, 0x0,
                0x22, 0x17, 0x41, 0x0, 0x32, 0x20
        };
unsigned char WF_PARTIAL_1IN54[159] =
        {
                0x0, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x80, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x40, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x0, 0x0, 0x0,
                0x02, 0x17, 0x41, 0xB0, 0x32, 0x28,
        };
#endif

// ssd1680 commands
#define SSD1680_CMD_DRIVER_OUTPUT_CONTROL           0x01
#define SSD1680_CMD_GATE_DRIVING_VOLTAGE_CONTROL    0x03
#define SSD1680_CMD_SOURCE_DRIVING_VOLTAGE_CONTROL  0x04
#define SSD1680_CMD_BOOSTER_SOFT_START_CONTROL      0x0C
#define SSD1680_CMD_DEEP_SLEEP_MODE                 0x10
#define SSD1680_CMD_DATA_ENTRY_MODE_SETTING         0x11
#define SSD1680_CMD_SOFT_RESET                      0x12
#define SSD1680_CMD_TEMPERATURE_SENSOR_CONTROL      0x18
#define SSD1680_CMD_MASTER_ACTIVATION               0x20
#define SSD1680_CMD_DISPLAY_UPDATE_CONTROL_1        0x21
#define SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2        0x22
#define SSD1680_CMD_WRITE_RAM                       0x24
#define SSD1680_CMD_WRITE_VCOM_REGISTER             0x2C
#define SSD1680_CMD_WRITE_LUT_REGISTER              0x32
#define SSD1680_CMD_SET_DUMMY_LINE_PERIOD           0x3A
#define SSD1680_CMD_SET_GATE_TIME                   0x3B
#define SSD1680_CMD_BORDER_WAVEFORM_CONTROL         0x3C
#define SSD1680_CMD_SET_RAM_X_START_END             0x44
#define SSD1680_CMD_SET_RAM_Y_START_END             0x45
#define SSD1680_CMD_SET_RAM_X_ADDRESS_COUNTER       0x4E
#define SSD1680_CMD_SET_RAM_Y_ADDRESS_COUNTER       0x4F

static int dc_gpio = -1;
static TaskHandle_t xTaskToNotify = NULL;

void lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
    if (dc_gpio > 0) {
        int dc = (int) t->user;
        gpio_set_level(dc_gpio, dc);
    }
}

static void lcd_spi_post_trans_callback(spi_transaction_t *trans) {
}

static void IRAM_ATTR busy_gpio_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//    /* Notify the task that the transmission is complete. */
    vTaskNotifyGiveIndexedFromISR(xTaskToNotify, 0, &xHigherPriorityTaskWoken);
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

    if (panel->busy_gpio_num >= 0) {
        xTaskToNotify = xTaskGetCurrentTaskHandle();

        // setup gpio
        ESP_LOGI(TAG, "busy pin is %d", panel->busy_gpio_num);
        gpio_config_t io_config = {
                .pin_bit_mask = (1ull << panel->busy_gpio_num),
                .mode = GPIO_MODE_INPUT,
                .intr_type = GPIO_INTR_NEGEDGE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_config));

        //install gpio isr service
        gpio_install_isr_service(0);
        //hook isr handler for specific gpio pin
        gpio_isr_handler_add(panel->busy_gpio_num, busy_gpio_isr_handler, NULL);
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
            .queue_size = TRANSFER_QUEUE_SIZE, // able to queue 10 transactions at a time
            .pre_cb = lcd_spi_pre_transfer_callback, // pre-transaction callback, mainly control DC gpio level
            .post_cb = io_config->on_color_trans_done ? lcd_spi_post_trans_callback
                                                      : NULL, // post-transaction, where we invoke user registered "on_color_trans_done()"
    };

    spi_device_handle_t spi_dev;
    ret = spi_bus_add_device((spi_host_device_t) bus, &devcfg, &spi_dev);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "adding spi device to bus failed");

    panel->spi_dev = spi_dev;

    // if the DC line is not encoded into any spi transaction phase or it's not controlled by SPI peripheral
    if (io_config->dc_gpio_num >= 0) {
        gpio_config_t io_conf = {
                .mode = GPIO_MODE_OUTPUT,
                .pin_bit_mask = 1ULL << io_config->dc_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for D/C line failed");
        dc_gpio = io_config->dc_gpio_num;
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

static esp_err_t lcd_data(lcd_ssd1680_panel_t *panel, const uint8_t *data, size_t len) {
    // ESP_LOGI(TAG, "lcd data 0x%02x len:%d", *data, len);
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = len * 8;             //Len is in bytes, transaction length is in bits.
    t.tx_buffer = data;             //Data
    t.user = (void *) 1;            //D/C needs to be set to 1
    ret = spi_device_polling_transmit(panel->spi_dev, &t);  //Transmit!
    ESP_GOTO_ON_ERROR(ret, err, TAG, "spi transmit (polling) data failed");

    err:
    return ret;
}

static esp_err_t lcd_cmd(lcd_ssd1680_panel_t *panel, const uint8_t cmd, const void *param, size_t param_size) {
    // ESP_LOGI(TAG, "lcd cmd 0x%02x", cmd);
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = 8;                     //Command is 8 bits
    t.tx_buffer = &cmd;               //The data is the cmd itself
    t.user = (void *) 0;                //D/C needs to be set to 0
    ret = spi_device_polling_transmit(panel->spi_dev, &t);  //Transmit!
    ESP_GOTO_ON_ERROR(ret, err, TAG, "spi transmit (polling) cmd failed");

    if (param && param_size) {
        ret = lcd_data(panel, param, param_size);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "spi transmit (polling) data failed");
    }

    err:
    return ret;
}

static void wait_for_busy(lcd_ssd1680_panel_t *ssd1680) {
    //• Wait BUSY Low
    // ESP_LOGI(TAG, "ssd1680 wait for busy ...");
    if (ssd1680->busy_gpio_num >= 0) {
        while (true) {
            if (!gpio_get_level(ssd1680->busy_gpio_num)) {
                break;
            }
            uint32_t ulNotificationValueCount = ulTaskNotifyTakeIndexed(0, pdTRUE, pdMS_TO_TICKS(500));
            if (ulNotificationValueCount > 0) {
                //ESP_LOGI(TAG, "get busy isr result... exit wait");
            } else {
                ESP_LOGI(TAG, "still wait for busy ...");
            }
        }
    } else {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void set_lut_by_host(lcd_ssd1680_panel_t *ssd1680, uint8_t *lut, uint8_t len) {
    lcd_cmd(ssd1680, SSD1680_CMD_WRITE_LUT_REGISTER, lut, len);
    wait_for_busy(ssd1680);
}

void reset_panel_state(lcd_ssd1680_panel_t *panel) {
    panel->_current_mem_area_start_x = -1;
    panel->_current_mem_area_start_y = -1;
    panel->_current_mem_pointer_x = -1;
    panel->_current_mem_pointer_y = -1;

    panel->_using_partial_mode = false;
}

esp_err_t panel_ssd1680_reset(lcd_ssd1680_panel_t *panel) {
    //ESP_LOGI(TAG, "lcd panel start hardware reset.");

    // perform hardware reset
    if (panel->reset_gpio_num >= 0) {
        reset_panel_state(panel);

//        gpio_set_level(panel->reset_gpio_num, !panel->reset_level);
//        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(panel->reset_gpio_num, panel->reset_level);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_set_level(panel->reset_gpio_num, !panel->reset_level);
        vTaskDelay(pdMS_TO_TICKS(5));
        wait_for_busy(panel);
    }

    ESP_LOGI(TAG, "lcd panel hardware reset OK.");
    return ESP_OK;
}

esp_err_t panel_ssd1680_sw_reset(lcd_ssd1680_panel_t *panel) {
    //ESP_LOGI(TAG, "lcd panel start software reset.");

    reset_panel_state(panel);
    // sw reset
    lcd_cmd(panel, SSD1680_CMD_SOFT_RESET, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    //wait_for_busy(ssd1680);

    ESP_LOGI(TAG, "lcd panel software reset OK.");
    return ESP_OK;
}

/**
 * 左闭右开
 */
static esp_err_t
set_mem_area(lcd_ssd1680_panel_t *ssd1680, uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y) {
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    end_x -= 1;
    end_y -= 1;

    if (start_x != ssd1680->_current_mem_area_start_x
        || end_x != ssd1680->_current_mem_area_end_x) {
        lcd_cmd(ssd1680, SSD1680_CMD_SET_RAM_X_START_END,
                (uint8_t[]) {(start_x >> 3) & 0xff, (end_x >> 3) & 0xff}, 2);
    }

    if (start_y != ssd1680->_current_mem_area_start_y
        || end_y != ssd1680->_current_mem_area_end_y) {
        lcd_cmd(ssd1680, SSD1680_CMD_SET_RAM_Y_START_END,
                (uint8_t[]) {start_y & 0xff, (start_y >> 8) & 0xff, end_y & 0xff, (end_y >> 8) & 0xff},
                4);
    }

    ssd1680->_current_mem_area_start_x = start_x;
    ssd1680->_current_mem_area_end_x = end_x;
    ssd1680->_current_mem_area_start_y = start_y;
    ssd1680->_current_mem_area_end_y = end_y;
    return ESP_OK;
}

static esp_err_t set_mem_pointer(lcd_ssd1680_panel_t *ssd1680, uint16_t x, uint16_t y) {
    if (x != ssd1680->_current_mem_pointer_x) {
        lcd_cmd(ssd1680, SSD1680_CMD_SET_RAM_X_ADDRESS_COUNTER, (uint8_t[]) {(x >> 3) & 0xff}, 1);
    }
    if (y != ssd1680->_current_mem_pointer_y) {
        lcd_cmd(ssd1680, SSD1680_CMD_SET_RAM_Y_ADDRESS_COUNTER,
                (uint8_t[]) {y & 0xff, (y >> 8) & 0xff}, 2);
    }
    ssd1680->_current_mem_pointer_x = x;
    ssd1680->_current_mem_pointer_y = y;
    return ESP_OK;
}

/**
 *  @brief: update the display
 *          there are 2 memory areas embedded in the e-paper display
 *          but once this function is called,
 *          the the next action of SetFrameMemory or ClearFrame will
 *          set the other memory area.
 */
esp_err_t update_full(lcd_ssd1680_panel_t *ssd1680) {
#ifdef CONFIG_SPI_DISPLAY_SSD1680_1IN54_V1
    lcd_cmd(ssd1680, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2, (uint8_t[]) {0xC4}, 1);
    lcd_cmd(ssd1680, SSD1680_CMD_MASTER_ACTIVATION, NULL, 0);
    lcd_cmd(ssd1680, 0xFF, NULL, 0);
#else
    lcd_cmd(ssd1680, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2, (uint8_t[]) {0xC7}, 1);
    lcd_cmd(ssd1680, SSD1680_CMD_MASTER_ACTIVATION, NULL, 0);
#endif
    wait_for_busy(ssd1680);
    ESP_LOGI(TAG, "ssd1680 full refresh success..");
    return ESP_OK;
}

esp_err_t update_part(lcd_ssd1680_panel_t *ssd1680) {
#ifdef CONFIG_SPI_DISPLAY_SSD1680_1IN54_V1
    lcd_cmd(ssd1680, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2, (uint8_t[]) {0xC4}, 1);
    lcd_cmd(ssd1680, SSD1680_CMD_MASTER_ACTIVATION, NULL, 0);
    lcd_cmd(ssd1680, 0xFF, NULL, 0);
#else
    lcd_cmd(ssd1680, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2, (uint8_t[]) {0xCF}, 1);
    lcd_cmd(ssd1680, SSD1680_CMD_MASTER_ACTIVATION, NULL, 0);
#endif
    wait_for_busy(ssd1680);
    ESP_LOGI(TAG, "ssd1680 part refresh success..");
    return ESP_OK;
}

esp_err_t panel_ssd1680_clear_display(lcd_ssd1680_panel_t *panel, uint8_t color) {
    set_mem_area(panel, 0, 0, LCD_H_RES, LCD_V_RES);
    set_mem_pointer(panel, 0, 0);

    // size_t len = LCD_H_RES * LCD_V_RES / 8;
    lcd_cmd(panel, SSD1680_CMD_WRITE_RAM, NULL, 0);

    for (int j = 0; j < LCD_V_RES; j++) {
        for (int i = 0; i < LCD_H_RES / 8; i++) {
            lcd_data(panel, &color, 1);
        }
    }

//    lcd_cmd(panel, 0x26, NULL, 0);
//    for (int j = 0; j < LCD_V_RES; j++) {
//        for (int i = 0; i < LCD_H_RES / 8; i++) {
//            lcd_data(panel, &color, 1);
//        }
//    }

    panel_ssd1680_refresh(panel, false);
    ESP_LOGI(TAG, "ssd1680 clear display ...");
    return ESP_OK;
}

esp_err_t pre_init(lcd_ssd1680_panel_t *panel) {
    ESP_LOGI(TAG, "ssd1680 start for init ...");

    // SW Reset by Command 0x12
    wait_for_busy(panel);
    panel_ssd1680_sw_reset(panel);

    // Wait 10ms busy is high when sw reset
    vTaskDelay(pdMS_TO_TICKS(5));
    wait_for_busy(panel);

    // 3. Send Initialization Code
    // Set gate driver output by Command 0x01
    lcd_cmd(panel, SSD1680_CMD_DRIVER_OUTPUT_CONTROL,
            (uint8_t[]) {(LCD_V_RES - 1) & 0xff, ((LCD_V_RES - 1) >> 8) & 0xff, 0x00}, 3);

#ifdef CONFIG_SPI_DISPLAY_SSD1680_1IN54_V1
    lcd_cmd(panel, SSD1680_CMD_BOOSTER_SOFT_START_CONTROL, (uint8_t[]) {0xd7, 0xd6, 0x9d}, 3);
    lcd_cmd(panel, SSD1680_CMD_WRITE_VCOM_REGISTER, (uint8_t[]) {0xa8, 0xd6, 0x9d}, 3);
    lcd_cmd(panel, SSD1680_CMD_SET_DUMMY_LINE_PERIOD, (uint8_t[]) {0x1a}, 1);
    lcd_cmd(panel, SSD1680_CMD_SET_GATE_TIME, (uint8_t[]) {0x08}, 1);
#endif

    // Set lcd RAM size by Command 0x11, 0x44, 0x45
    // 00 –Y decrement, X decrement, //01 –Y decrement, X increment, //10 –Y increment, X decrement, //11 –Y increment, X increment [POR]
    lcd_cmd(panel, SSD1680_CMD_DATA_ENTRY_MODE_SETTING, (uint8_t[]) {0x03}, 1);

    // Set panel border by Command 0x3C
    lcd_cmd(panel, SSD1680_CMD_BORDER_WAVEFORM_CONTROL, (uint8_t[]) {0x01}, 1);

    lcd_cmd(panel, SSD1680_CMD_TEMPERATURE_SENSOR_CONTROL, (uint8_t[]) {0x80}, 1);

//    //Load Temperature and waveform setting.
//    lcd_cmd(panel, 0x22, (uint8_t[]) {0XB1}, 1);
//    lcd_cmd(panel, 0x20, NULL, 1);
//    wait_for_busy(panel);

//    //  Display update control
//    lcd_cmd(panel, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_1, (uint8_t[]) {0x00, 0x80}, 2);

    panel->_current_mem_area_start_x = -1;
    panel->_current_mem_area_start_y = -1;
    panel->_current_mem_pointer_x = -1;
    panel->_current_mem_pointer_y = -1;

    set_mem_area(panel, 0, 0, LCD_H_RES, LCD_V_RES);
    set_mem_pointer(panel, 0, 0);

    ESP_LOGI(TAG, "ssd1680 panel pre init ok !");
    return ESP_OK;
}

esp_err_t panel_ssd1680_init_full(lcd_ssd1680_panel_t *panel) {
    pre_init(panel);
    panel->_using_partial_mode = false;

#ifdef CONFIG_SPI_DISPLAY_SSD1680_1IN54_V1
    set_lut_by_host(panel, WF_Full_1IN54, 30);
#else
    set_lut_by_host(panel, WF_Full_1IN54, 153);
    lcd_cmd(panel, 0x3f, &WF_Full_1IN54[153], 1);
    lcd_cmd(panel, 0x03, &WF_Full_1IN54[154], 1);
    lcd_cmd(panel, 0x04, &WF_Full_1IN54[155], 3);
    lcd_cmd(panel, 0x2C, &WF_Full_1IN54[158], 1);
#endif
    ESP_LOGI(TAG, "ssd1680 init full success!");
    return ESP_OK;
}

esp_err_t panel_ssd1680_init_partial(lcd_ssd1680_panel_t *panel) {
    if (panel->_using_partial_mode) {
        return ESP_OK;
    }

    pre_init(panel);

#ifdef CONFIG_SPI_DISPLAY_SSD1680_1IN54_V1
    set_lut_by_host(panel, WF_PARTIAL_1IN54, 30);
#else
    set_lut_by_host(panel, WF_PARTIAL_1IN54, 153);
    lcd_cmd(panel, 0x3f, &WF_PARTIAL_1IN54[153], 1);
    lcd_cmd(panel, 0x03, &WF_PARTIAL_1IN54[154], 1);
    lcd_cmd(panel, 0x04, &WF_PARTIAL_1IN54[155], 3);
    lcd_cmd(panel, 0x2C, &WF_PARTIAL_1IN54[158], 1);

    lcd_cmd(panel, 0x37, (uint8_t[]) {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00}, 10);

    //c0 cf
    lcd_cmd(panel, SSD1680_CMD_DISPLAY_UPDATE_CONTROL_2, (uint8_t[]) {0xCF}, 1);
    lcd_cmd(panel, SSD1680_CMD_MASTER_ACTIVATION, NULL, 0);
    wait_for_busy(panel);
#endif
    panel->_using_partial_mode = true;
    ESP_LOGI(TAG, "ssd1680 init partial success!");
    return ESP_OK;
}

/**
 * x_start, y_start include, x_end, y_end not include
 */
esp_err_t panel_ssd1680_draw_bitmap(lcd_ssd1680_panel_t *panel, int16_t x_start, int16_t y_start, int16_t x_end, int16_t y_end,
                                    const void *color_data) {
    assert(((x_start < x_end) && (y_start < y_end)) && "start position must be smaller than end position");
    int w = x_end - x_start;
    int h = y_end - y_start;

    int wb = (w + 7) / 8; // width bytes, bitmaps are padded

    x_start -= x_start % 8; // byte boundary
    w = wb * 8; // byte boundary

    int x1 = x_start < 0 ? 0 : x_start;
    int y1 = y_start < 0 ? 0 : y_start;
    int w1 = x_start + w < LCD_H_RES ? w : LCD_H_RES - x_start; // limit
    int h1 = y_start + h < LCD_V_RES ? h : LCD_V_RES - y_start; // limit

    int dx = x1 - x_start;
    int dy = y1 - y_start;

    w1 -= dx;
    h1 -= dy;

    if ((w1 <= 0) || (h1 <= 0)) return ESP_OK;

    set_mem_area(panel, x1, y1, x1 + w, y1 + h);
    set_mem_pointer(panel, x1, y1);

    size_t len = h1 * w1 / 8;
    if (len == LCD_H_RES * LCD_V_RES / 8) {
        // transfer frame buffer
        lcd_cmd(panel, SSD1680_CMD_WRITE_RAM, color_data, len);
    } else {
        lcd_cmd(panel, SSD1680_CMD_WRITE_RAM, NULL, 0);
        for (int16_t i = 0; i < h1; i++) {
            // method1 one byte by one byte
//            for (int16_t j = 0; j < w1 / 8; j++) {
//                uint8_t data;
//                // use wb, h of bitmap for index!
//                int16_t idx = j + dx / 8 + (i + dy) * wb;
//                data = ((uint8_t *) color_data)[idx];
//                lcd_data(panel, &data, 1);
//            }

            // method 2 full line (w1 / 8) byte
            int16_t idx = dx / 8 + (i + dy) * wb;
            uint8_t data = ((uint8_t *) color_data)[idx];
            lcd_data(panel, &data, w1 / 8);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return ESP_OK;
}

esp_err_t panel_ssd1680_refresh(lcd_ssd1680_panel_t *panel, bool partial_update_mode) {
    ESP_LOGI(TAG, "ssd1680 request for refresh mode %s", partial_update_mode ? "partial" : "full");
    if (partial_update_mode) {
        if (!panel->_using_partial_mode) {
            panel_ssd1680_init_partial(panel);
        }
        panel_ssd1680_refresh_area(panel, 0, 0, LCD_H_RES, LCD_V_RES);
    } else {
        if (panel->_using_partial_mode) {
            panel_ssd1680_init_full(panel);
        }
        update_full(panel);
    }
    return ESP_OK;
}

esp_err_t
panel_ssd1680_refresh_area(lcd_ssd1680_panel_t *panel, int16_t x, int16_t y, int16_t end_x, int16_t end_y) {
    if (!panel->_using_partial_mode) {
        panel_ssd1680_init_partial(panel);
    }

    if (x < 0) x = 0;
    if (end_x > LCD_H_RES) end_x = LCD_H_RES;
    if (y < 0) y = 0;
    if (end_y > LCD_V_RES) end_y = LCD_V_RES;

    // make x1, w1 multiple of 8
    if (end_x % 8 > 0) end_x += 8 - end_x % 8;
    x -= x % 8;
    set_mem_area(panel, x, y, end_x, end_y);
    update_part(panel);
    return ESP_OK;
}

esp_err_t panel_ssd1680_sleep(lcd_ssd1680_panel_t *panel) {
    // enter deep sleep mode
    // 0x01 deep sleep mode 1 ram保存
    // 0x11 deep sleep mode 2 ram不保存
    // deep sleep mode 需要HWRESET唤醒
    lcd_cmd(panel, SSD1680_CMD_DEEP_SLEEP_MODE, (uint8_t[]) {0x11}, 1);
    ESP_LOGI(TAG, "ssd1680 enter sleep mode");
    return ESP_OK;
}

esp_err_t panel_ssd1680_del(lcd_ssd1680_panel_t *panel) {
    if (panel->reset_gpio_num >= 0) {
        gpio_reset_pin(panel->reset_gpio_num);
    }
    if (panel->busy_gpio_num >= 0) {
        gpio_reset_pin(panel->busy_gpio_num);
    }
    ESP_LOGD(TAG, "del ssd1680 panel @%p", panel);
    return ESP_OK;
}