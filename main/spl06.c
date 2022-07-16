#include <math.h>
#include <driver/i2c.h>
#include <esp_log.h>

#include "spl06.h"


// or 0x76 if SDO is low
#define SPL06_ADDR 0x77

#define PSR_B2 0x00
#define PSR_B1 0x01
#define PSR_B0 0x02

#define TMP_B2 0x03
#define TMP_B1 0x04
#define TMP_B0 0x05

/**
 * 每秒采样次数  -- 只在background模式生效
 * bit [6:4] PM_RATE rw
 *      000 1/s 100 16
 *      001 2/s 101 32
 *      010 4   110 64
 *      011 8   111 128
 *
 * 超采样次数
 * bit [3:0] PM_PRC rw
 *      0000 1  0100 16   -- bit shift. CFG_REG
 *      0001 2  0101 32     -- 53.2ms
 *      0010 4  0110 64     -- 104.4ms
 *      0011 8  0111 128    -- 206.8ms
 */
#define PRS_CFG 0x06

/**
 * bit 7 TMP_ EXT rw
 * bit [6:4] TMP_RATE rw
 * bit [2:0] TM_PRC rw
 *      000 1       100 16        1次采样 3.6ms
 *      001 2       101 32
 *      010 4       110 64
 *      011 8       111 128
 */
#define TMP_CFG 0x07

/**
 * bit 7 COEF_RDY   1- ready
 * bit 6 SENSOR _RDY 1 - Sensor initialization complete
 * bit 5 TMP _RDY 1 - New temperature measurement is ready.
 * bit 4 PRS_ RDY 1 - New pressure measurement is ready.
 * bit [2:0] MEAS_CRTL rw
 *      000 - Idle / Stop background measurement
 *      001 - Pressure measurement
 *      010 - Temperature measurement
 *
 *      Background Mode:
 *      101 - Continuous pressure measurement
 *      110 - Continuous temperature measurement
 *      111 - Continuous pressure and temperature measurement
 */
#define MEAS_CFG 0x08

/**
 * bit 7 INT_ HL rw
 * bit [6:4] INT_ SEL rw
 * bit 3 TMP_ SHIFT_ EN (rw)  Temperature result bit-shift, Note: Must be set to '1' when the oversampling rate is >8 times.
 * bit 2 PRS_ SHIFT_ EN (rw) Pressure result bit-shift, Must be set to '1' when the oversampling rate is >8 times.
 * bit 1 FIFO_ EN (rw)  1 - Enable the FIFO
 */
#define CFG_REG 0x09

/**
 * bit 2 INT_ FIFO_ FULL
 * bit 1 INT_ TMP
 * bit 0 INT_ PRS
 */
#define INT_STS 0x0A

/**
 * bit 1 FIFO_ FULL
 * bit 0 FIFO_ EMPTY
 */
#define FIFO_STS 0x0B

/**
 * bit 7 FIFO_ FLUSH (w)    1-Empty FIFO After reading out all data from the FIFO, write '1' to clear all old data.
 * bit [3:0] SOFT_RST [3:0] (w)  1001 for reset
 */
#define RESET 0x0C

/**
 * [7:4] PROD_ID
 * [3:0] REV_ID
 */
#define SPL06_ID  0x0D

/**
 * from 0x10 to 0x21
 */
#define COEF 0x10

#define I2C_MASTER_NUM 0
#define I2C_MASTER_TIMEOUT_MS 10
#define I2C_MASTER_FREQ_HZ 30000

#define I2C_MASTER_SDA_IO 18
#define I2C_MASTER_SCL_IO 19

#define TAG "spl06"

uint16_t c0 = 0;
uint16_t c1 = 0;
uint32_t c00 = 0;
uint32_t c10 = 0;
uint16_t c01 = 0;
uint16_t c11 = 0;
uint16_t c20 = 0;
uint16_t c21 = 0;
uint16_t c30 = 0;


/**
 * @brief Read a sequence of bytes from a MPU9250 sensor registers
 */
static esp_err_t i2c_read(uint8_t reg_addr, uint8_t *data, size_t len) {
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, SPL06_ADDR,
                                                 &reg_addr, 1, data, len,
                                                 I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read i2c failed, for addr %x,  %s", reg_addr, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t i2c_write_cmd(uint8_t reg_addr) {
    int ret;
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, SPL06_ADDR,
                                     &reg_addr, 1,
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write i2c cmd failed, for addr %x,  %s", reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t i2c_write_data(uint8_t reg_addr, uint8_t data) {
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, SPL06_ADDR,
                                     write_buf,
                                     sizeof(write_buf),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write i2c cmd failed, for addr %x,  %s", reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

void spl06_init() {
    i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_MASTER_SDA_IO,         // select GPIO specific to your project
            //.sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_io_num = I2C_MASTER_SCL_IO,         // select GPIO specific to your project
            //.scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_MASTER_FREQ_HZ,  // select frequency specific to your project
            // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);

    ESP_LOGI(TAG, "ms5611 start for init scl:%d sda:%d", I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);

    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
    ESP_LOGI(TAG, "ms5611 I2C initialized successfully sda: %d, scl:%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

    spl06_reset();

    // wait for COEF READY
    while (1) {
        uint8_t meas_cfg;
        i2c_read(MEAS_CFG, &meas_cfg, 1);
        if ((meas_cfg >> 7) & 0x01) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(3));
    }

    // load COEF from 0x10 - 0x21
    for (int i = MEAS_CFG; i <= 0x21; ++i) {
        uint8_t cofe_buf = 0;
        i2c_read(MEAS_CFG + i, &cofe_buf, 1);
        if (i == 0x10) {
            c0 = (cofe_buf << 4);
        } else if (i == 0x11) {
            c0 = c0 | (cofe_buf >> 4);
            c1 = ((cofe_buf & 0x0f) << 8);
        } else if (i == 0x12) {
            c1 = c1 | cofe_buf;
        } else if (i == 0x13) {
            c00 = (cofe_buf << 12);
        } else if (i == 0x14) {
            c00 = c00 | (cofe_buf << 4);
        } else if (i == 0x15) {
            c00 = c00 | (cofe_buf >> 4);
            c10 = (cofe_buf & 0x0f) << 16;
        } else if (i == 0x16) {
            c10 = c10 | (cofe_buf << 8);
        } else if (i == 0x17) {
            c10 = c10 | cofe_buf;
        } else if (i == 0x18) {
            c01 = cofe_buf << 8;
        } else if (i == 0x19) {
            c01 = c01 | cofe_buf;
        } else if (i == 0x1a) {
            c11 = cofe_buf << 8;
        } else if (i == 0x1b) {
            c11 = c11 | cofe_buf;
        } else if (i == 0x1c) {
            c20 = cofe_buf << 8;
        } else if (i == 0x1d) {
            c20 = c20 | cofe_buf;
        } else if (i == 0x1e) {
            c21 = cofe_buf << 8;
        } else if (i == 0x1f) {
            c21 = c21 | cofe_buf;
        } else if (i == 0x20) {
            c30 = cofe_buf << 8;
        } else if (i == 0x21) {
            c30 = c30 | cofe_buf;
        }
    }

    ESP_LOGI(TAG, "spl06 read COEF c0:%x c1:%x c00:%x c10:%x c01:%x c11:%x c20:%x c21:%x c30:%x",
             c0, c1, c00, c10, c01, c11, c20, c21, c30);

    // read device id
    uint8_t device_id;
    i2c_read(SPL06_ID, &device_id, 1);
    ESP_LOGI(TAG, "PROD_ID:%x, REV_ID:%x", device_id >> 4, device_id & 0x0f);
}

void spl06_reset() {
    i2c_write_data(RESET, 0b10001001);
    ESP_LOGI(TAG, "reset...");
}