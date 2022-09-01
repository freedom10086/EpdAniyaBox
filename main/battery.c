#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "soc/soc_caps.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "bike_common.h"
#include "battery.h"

#define TAG "battery"
#define STORAGE_NAMESPACE "battery"

//ADC1 Channels
#if CONFIG_IDF_TARGET_ESP32
#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_4
#define EXAMPLE_ADC1_CHAN1          ADC_CHANNEL_5
#else
#define ADC1_CHAN1          ADC_CHANNEL_1
#endif

static int adc_raw;
static int _voltage;

/**
 * 4208 充电
 * 4184 100%
 * 16:24 开始充电
 */

static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle);

static void adc_calibration_deinit(adc_cali_handle_t handle);

esp_err_t get_battery_curve_status(int32_t *status) {
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) return err;

    // Read
    err = nvs_get_i32(my_handle, "curve_status", status);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t set_battery_curve_status(int32_t status) {
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Write
    err = nvs_set_i32(my_handle, "curve_status", status);
    if (err != ESP_OK) return err;

    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t clear_battery_curve() {
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(my_handle, "curve", NULL, 0);
    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t add_battery_curve(int v) {
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "curve", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    // Read previously saved blob if available
    uint32_t * votages = malloc(required_size + sizeof(uint32_t));
    if (required_size > 0) {
        err = nvs_get_blob(my_handle, "curve", votages, &required_size);
        if (err != ESP_OK) {
            free(votages);
            return err;
        }
    }

    // Write value including previously saved blob if available
    required_size += sizeof(uint32_t);
    votages[required_size / sizeof(uint32_t) - 1] = v;
    err = nvs_set_blob(my_handle, "curve", votages, required_size);
    free(votages);

    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t print_battery_curve(void) {
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    // obtain required memory space to store blob being read from NVS
    err = nvs_get_blob(my_handle, "curve", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    printf("battery curve:\n");
    if (required_size == 0) {
        printf("Nothing saved yet!\n");
    } else {
        uint32_t * battery_curve = malloc(required_size);
        err = nvs_get_blob(my_handle, "curve", battery_curve, &required_size);

        if (err != ESP_OK) {
            free(battery_curve);
            return err;
        }
        for (int i = 0; i < required_size / sizeof(uint32_t); i++) {
            printf("%d: %ld\n", i + 1, battery_curve[i]);
        }
        free(battery_curve);
    }

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

static void battery_task_entry(void *arg) {
    ESP_ERROR_CHECK(common_init_nvs());

    esp_err_t err;
    int32_t battery_curve_status;
    bool start_battery_curve = false;

    get_battery_curve_status(&battery_curve_status);
    ESP_LOGI(TAG, "battery curve status %ld", battery_curve_status);
    if (battery_curve_status > 0) {
        print_battery_curve();
    } else {
        ESP_LOGI(TAG, "start battery curve");
        err = set_battery_curve_status(1);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "set battery curve status failed %d %s", err, esp_err_to_name(err));
        } else {
            start_battery_curve = true;
        }
    }

    //-------------ADC1 Init---------------//
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
            .bitwidth = ADC_BITWIDTH_DEFAULT,
            .atten = ADC_ATTEN_DB_11,
    };

    // esp32c3 gpio1
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_CHAN1, &config));
    //ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN1, &config));


    //-------------ADC1 Calibration Init---------------//
    adc_cali_handle_t adc1_cali_handle = NULL;
    bool do_calibration1 = adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_11, &adc1_cali_handle);

    while (1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN1, &adc_raw));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC1_CHAN1, adc_raw);
        if (do_calibration1) {
            int voltage;
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage));
            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC1_CHAN1, voltage);
            _voltage = voltage;

            if (start_battery_curve) {
                err = add_battery_curve(voltage);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "add battery curve failed %d %s", err, esp_err_to_name(err));
                } else {
                    ESP_LOGI(TAG, "add battery curve %d", voltage);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(90000));
    }

    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    if (do_calibration1) {
        adc_calibration_deinit(adc1_cali_handle);
    }
}

void battery_init(void) {
    TaskHandle_t tsk_hdl;
    /* Create NMEA Parser task */
    BaseType_t err = xTaskCreate(
            battery_task_entry,
            "battery",
            2048,
            NULL,
            uxTaskPriorityGet(NULL),
            &tsk_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "create battery detect task failed");
    }

    ESP_LOGI(TAG, "battery task init OK");
}


/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = unit,
                .atten = atten,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void adc_calibration_deinit(adc_cali_handle_t handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

int battery_get_voltage() {
    return _voltage;
}

void battery_deinit() {

}
