#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_event.h"
#include "driver/i2c.h"

#include "bike_common.h"
#include "sht31.h"

#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       200

#define SHT31_ADDR                 0x44
#define SHT31_MEAS_HIGHREP_STRETCH 0x2C06 /**< Measurement High Repeatability with Clock Stretch Enabled */
#define SHT31_MEAS_MEDREP_STRETCH   0x2C0D /**< Measurement Medium Repeatability with Clock Stretch Enabled */
#define SHT31_MEAS_LOWREP_STRETCH   0x2C10 /**< Measurement Low Repeatability with Clock Stretch Enabled*/
#define SHT31_MEAS_HIGHREP    0x2400 /**< Measurement High Repeatability with Clock Stretch Disabled */
#define SHT31_MEAS_MEDREP      0x240B /**< Measurement Medium Repeatability with Clock Stretch Disabled */
#define SHT31_MEAS_LOWREP   0x2416 /**< Measurement Low Repeatability with Clock Stretch Disabled */

/**
 * 16bit
 * [0] - Write data checksum status
        '0': checksum of last write transfer was correct
 * [1] - Command status '0': last command executed successfully
 * [3:2] - Reserved
 * [4] System reset detected, '0': no reset detected since last ‘clear status register’ command
 * [9:5] Reserved
 * [10] T tracking alert, ‘0’ : no alert
 * [11] RH tracking alert
 * [12] Reserved
 * [13] Heater status ‘0’ : Heater OFF
 * [14] Reserved
 * [15] Alert pending status, '0': no pending alerts
  */
#define SHT31_READSTATUS 0xF32D   /**< Read Out of Status Register */
#define SHT31_CLEARSTATUS 0x3041  /**< Clear Status */

#define SHT31_SOFTRESET 0x30A2    /**< Soft Reset */
#define SHT31_HEATEREN 0x306D     /**< Heater Enable */
#define SHT31_HEATERDIS 0x3066    /**< Heater Disable */
#define SHT31_FETCH_DATA 0xE000

// Periodic Data Acquisition Mode
// 0.5mps
#define SHT31_PERIOD_MEAS_HALF_HIGHREP 0x2032
#define SHT31_PERIOD_MEAS_HALF_MEDREP 0x2024
#define SHT31_PERIOD_MEAS_HALF_LOWREP 0x202F

// 1mps (1 measurements per second)
#define SHT31_PERIOD_MEAS_1_HIGHREP 0x2130
#define SHT31_PERIOD_MEAS_1_MEDREP 0x2126
#define SHT31_PERIOD_MEAS_1_LOWREP 0x212D

// 2mps (2 measurements per second)
#define SHT31_PERIOD_MEAS_2_HIGHREP 0x2236
#define SHT31_PERIOD_MEAS_2_MEDREP 0x2220
#define SHT31_PERIOD_MEAS_2_LOWREP 0x222B

// 4mps
#define SHT31_PERIOD_MEAS_4_HIGHREP 0x2334
#define SHT31_PERIOD_MEAS_4_MEDREP 0x2322
#define SHT31_PERIOD_MEAS_4_LOWREP 0x2329

// 10mps
#define SHT31_PERIOD_MEAS_10_HIGHREP 0x2737
#define SHT31_PERIOD_MEAS_10_MEDREP 0x2721
#define SHT31_PERIOD_MEAS_10_LOWREP 0x272A

#define SHT31_PEROID_MEAS_WITH_ART 0x2B32
#define SHT31_STOP_PERIOD 0x3093
/**
 * No Clock Stretching
 * When a command without clock stretching has been
 * issued, the sensor responds to a read header with a not
 * acknowledge (NACK), if no data is present.

 * Clock Stretching
 * When a command with clock stretching has been issued,
 * the sensor responds to a read header with an ACK and
 * subsequently pulls down the SCL line. The SCL line is
 * pulled down until the measurement is complete. As soon
 * as the measurement is complete, the sensor releases
 * the SCL line and sends the measurement results
 *
 *
 * Measurement duration
 *                          Typ.    Max.
 * Low repeatability        2.5     4ms
 * Medium repeatability     4.5     6ms
 * High repeatability       12.5    15ms
 */

static const char *TAG = "sht-31";

ESP_EVENT_DEFINE_BASE(BIKE_TEMP_HUM_SENSOR_EVENT);

static bool sht31_inited = false;
static sht31_t sht31;

/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init(void) {
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_SDA_IO,
            .scl_io_num = I2C_SCL_IO,
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

static uint8_t crc8(const uint8_t *data, uint8_t len) {
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

static void sht31_task_entry(void *arg) {
    uint8_t failed_cnt = 0;
    uint8_t data[3];

    sht31_reset();
    sht31_start:
    i2c_read(SHT31_READSTATUS, data, 3);
    if (data[2] != crc8(data, 2)) {
        ESP_LOGW(TAG, "read status crc check failed!");
        failed_cnt++;
        if (failed_cnt >= 3) {
            ESP_LOGE(TAG, "read sht31 status failed for 3 times...");
            common_post_event(BIKE_TEMP_HUM_SENSOR_EVENT, SHT31_SENSOR_INIT_FAILED);
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(3));
        goto sht31_start;
    } else {
        ESP_LOGI(TAG, "Status %X %X", data[0], data[1]);
        ESP_LOGI(TAG, "HEAT Status %d", (data[0] >> 5) & 0x01);
    }

    i2c_write_cmd(SHT31_CLEARSTATUS);
    vTaskDelay(pdMS_TO_TICKS(10));
    while (1) {
        if (sht31_read_temp_hum()) {
            //ESP_LOGI(TAG, "temp %f, hum:%f", sht31.data.temp, sht31.data.hum);
            common_post_event_data(BIKE_TEMP_HUM_SENSOR_EVENT, SHT31_SENSOR_UPDATE, &sht31.data, sizeof(sht31_data_t));
        } else {
            common_post_event(BIKE_TEMP_HUM_SENSOR_EVENT, SHT31_SENSOR_READ_FAILED);
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void sht31_reset() {
    ESP_LOGI(TAG, "Reset Sht31...");
    i2c_write_cmd(SHT31_SOFTRESET);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void sht31_init() {
    if (sht31_inited) {
        return;
    }
    esp_err_t iic_err = i2c_master_init();
    if (iic_err != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialized failed %d %s", iic_err, esp_err_to_name(iic_err));
        return;
    }
    ESP_LOGI(TAG, "I2C initialized successfully");

    /* Create NMEA Parser task */
    BaseType_t err = xTaskCreate(
            sht31_task_entry,
            "sht31",
            2048,
            &sht31,
            5,
            NULL);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "create sht31 task failed");
        return;
    }
    sht31_inited = true;
}

bool sht31_read_temp_hum() {
    uint8_t readbuffer[6];

    i2c_write_cmd(SHT31_MEAS_MEDREP); // med 10, high 20
    vTaskDelay(pdMS_TO_TICKS(10));

    i2c_read(SHT31_FETCH_DATA, readbuffer, 6);

    if (readbuffer[2] != crc8(readbuffer, 2) ||
        readbuffer[5] != crc8(readbuffer + 3, 2)) {
        ESP_LOGW(TAG, "read temp and hum crc check failed!");
        return false;
    }

    sht31.data_valid = true;

    int32_t stemp = (int32_t) (((uint32_t) readbuffer[0] << 8) | readbuffer[1]);
    if (stemp != sht31.raw_temp) {
        sht31.raw_temp = stemp;
    }

    // simplified (65536 instead of 65535) integer version of:
    // temp = (stemp * 175.0f) / 65535.0f - 45.0f;
    // stemp = ((4375 * stemp) >> 14) - 4500;
    sht31.data.temp = ((float) stemp * 175.0f) / 65535.0f - 45.0f;

    uint32_t shum = ((uint32_t) readbuffer[3] << 8) | readbuffer[4];
    if (shum != sht31.raw_hum) {
        sht31.raw_hum = shum;
    }

    // simplified (65536 instead of 65535) integer version of:
    // humidity = (shum * 100.0f) / 65535.0f;
    // shum = (625 * shum) >> 12;
    sht31.data.hum = ((float) shum * 100.0f) / (65536 - 1);

    return true;
}

bool sht31_get_temp_hum(float *temp, float *hum) {
    if (!sht31_inited) {
        return false;
    }

    if (!sht31.data_valid) {
        sht31_read_temp_hum();
    }

    if (!sht31.data_valid) {
        return false;
    }

    *temp = sht31.data.temp;
    *hum = sht31.data.hum;

    return true;
}

void sht31_deinit() {
    ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));
    ESP_LOGI(TAG, "I2C de-initialized successfully");
    sht31_inited = false;
}
