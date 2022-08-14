#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "driver/i2c.h"

#include "sht31.h"

#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       500

#define SHT31_ADDR                 0x44
#define SHT31_MEAS_HIGHREP_STRETCH 0x2C06 /**< Measurement High Repeatability with Clock Stretch Enabled */
#define SHT31_MEAS_MEDREP_STRETCH   0x2C0D /**< Measurement Medium Repeatability with Clock Stretch Enabled */
#define SHT31_MEAS_LOWREP_STRETCH   0x2C10 /**< Measurement Low Repeatability with Clock Stretch Enabled*/
#define SHT31_MEAS_HIGHREP    0x2400 /**< Measurement High Repeatability with Clock Stretch Disabled */
#define SHT31_MEAS_MEDREP      0x240B /**< Measurement Medium Repeatability with Clock Stretch Disabled */
#define SHT31_MEAS_LOWREP   0x2416 /**< Measurement Low Repeatability with Clock Stretch Disabled */
#define SHT31_READSTATUS 0xF32D   /**< Read Out of Status Register */
#define SHT31_CLEARSTATUS 0x3041  /**< Clear Status */
#define SHT31_SOFTRESET 0x30A2    /**< Soft Reset */
#define SHT31_HEATEREN 0x306D     /**< Heater Enable */
#define SHT31_HEATERDIS 0x3066    /**< Heater Disable */
#define SHT31_FETCH_DATA 0xE000

#define SHT31_REG_HEATER_BIT 0x0d /**< Status Register Heater Bit */


static const char *TAG = "sht-31";

sht31_t sht31;

/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init(void) {
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = 3,
            .scl_io_num = 2,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * @brief Read a sequence of bytes from a MPU9250 sensor registers
 */
static esp_err_t i2c_read(uint16_t reg_addr, uint8_t *data, size_t len) {
    uint8_t u8_reg_addr[] = {reg_addr >> 8, reg_addr & 0xff};
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, SHT31_ADDR,
                                                 u8_reg_addr, 2, data, len,
                                                 I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read i2c failed, for dev:%x addr:%x,  %s", SHT31_ADDR, reg_addr, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t i2c_write_cmd(uint16_t reg_addr) {
    int ret;
    uint8_t u8_reg_addr[] = {reg_addr >> 8, reg_addr & 0xff};
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, SHT31_ADDR,
                                     u8_reg_addr, 2,
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write i2c cmd failed, for addr %x,  %s", reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t i2c_write_data(uint16_t reg_addr, uint8_t data) {
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, SHT31_ADDR,
                                     write_buf,
                                     sizeof(write_buf),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write i2c cmd failed, for addr %x,  %s", reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

static uint8_t crc8(const uint8_t *data, int len) {
    const uint8_t POLYNOMIAL = 0x31;
    uint8_t crc = 0xFF;

    for (int j = len; j; --j) {
        crc ^= *data++;

        for (int i = 8; i; --i) {
            crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
        }
    }
    return crc;
}

sht31_t *sht31_init() {
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    uint8_t data[3];
    i2c_read(SHT31_READSTATUS, data, 3);
    ESP_LOGI(TAG, "Status %X %X", data[0], data[1]);

    return &sht31;
}

bool sht31_read_temp_hum() {
    uint8_t readbuffer[6];
    i2c_write_cmd(SHT31_MEAS_HIGHREP);
    vTaskDelay(pdMS_TO_TICKS(20));

    i2c_read(SHT31_FETCH_DATA, readbuffer, 6);

    if (readbuffer[2] != crc8(readbuffer, 2) ||
        readbuffer[5] != crc8(readbuffer + 3, 2)) {
        ESP_LOGW(TAG, "read temp and hum crc check failed!");
        return false;
    }

    sht31.data_valid = true;

    int32_t stemp = (int32_t)(((uint32_t)readbuffer[0] << 8) | readbuffer[1]);
    // simplified (65536 instead of 65535) integer version of:
    // temp = (stemp * 175.0f) / 65535.0f - 45.0f;
    stemp = ((4375 * stemp) >> 14) - 4500;
    sht31.temp = (float)stemp / 100.0f;

    uint32_t shum = ((uint32_t)readbuffer[3] << 8) | readbuffer[4];
    // simplified (65536 instead of 65535) integer version of:
    // humidity = (shum * 100.0f) / 65535.0f;
    shum = (625 * shum) >> 12;
    sht31.hum = (float)shum / 100.0f;

    return true;
}

void sht31_deinit() {
    ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));
    ESP_LOGI(TAG, "I2C de-initialized successfully");
}
