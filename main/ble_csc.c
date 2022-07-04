#include "esp_log.h"
#include "esp_gattc_api.h"

#include "ble_csc.h"

#define TAG "BLE_CSC"

// csc 计算的临时变量
static csc_measure_sensor_t csc_measure_sensor = {
        .first_wheel_data_get = false,
        .first_wheel_revolutions = 0,
        .last_wheel_revolutions = 0,
        .last_wheel_event_time = 0,

        .first_crank_data_get = false,
        .last_crank_revolutions = 0,
        .last_crank_event_time = 0,
};

// 轮速传感器
void on_wheel_measurement_received(uint32_t wheel_revolutions, uint16_t last_wheel_event_time) {
    // TODO 轮子周长 [mm]
    uint32_t cirreplacedference = 2200;

    if (!csc_measure_sensor.first_wheel_data_get /*csc_measure_sensor.first_wheel_revolutions < 0*/) {
        csc_measure_sensor.first_wheel_revolutions = wheel_revolutions;
    }

    // data repeat
    if (csc_measure_sensor.first_wheel_data_get && csc_measure_sensor.last_wheel_event_time == last_wheel_event_time ) {
        return;
    }

    if (csc_measure_sensor.first_wheel_data_get) {
        float timeDifference;
        if (last_wheel_event_time < csc_measure_sensor.last_wheel_event_time)
            timeDifference =
                    (float) (65535 + last_wheel_event_time - csc_measure_sensor.last_wheel_event_time) / 1024.0f; // [s]
        else
            timeDifference =
                    (float) (last_wheel_event_time - csc_measure_sensor.last_wheel_event_time) / 1024.0f; // [s]
        float distanceDifference =
                (float) (wheel_revolutions - csc_measure_sensor.last_wheel_revolutions) * (float) cirreplacedference /
                1000.0f; // [m]
        float sensor_total_distance = (float) wheel_revolutions * (float) cirreplacedference / 1000.0f; // [m]
        float total_distance =
                (float) (wheel_revolutions - csc_measure_sensor.first_wheel_revolutions) * (float) cirreplacedference /
                1000.0f; // [m]
        float speed = distanceDifference / timeDifference;
        float wheel_dance =
                (float) (wheel_revolutions - csc_measure_sensor.last_wheel_revolutions) * 60.0f / timeDifference;

        ESP_LOGI(TAG, "speed: %fm/s %fkm/h, sensor_distance:%f, total_distance: %f, wheel_dance: %f",
                 speed, speed * 3.6f, sensor_total_distance, total_distance, wheel_dance);
    }

    if (!csc_measure_sensor.first_wheel_data_get) {
        csc_measure_sensor.first_wheel_data_get = true;
    }
    csc_measure_sensor.last_wheel_revolutions = wheel_revolutions;
    csc_measure_sensor.last_wheel_event_time = last_wheel_event_time;
}

// 曲柄传感器
void on_crank_measurement_received(uint16_t crank_revolutions, uint16_t last_crank_event_time) {
    // data repeat
    if (csc_measure_sensor.first_crank_data_get && csc_measure_sensor.last_crank_event_time == last_crank_event_time)
        return;

    if (csc_measure_sensor.first_crank_data_get) {
        float timeDifference;
        if (last_crank_event_time < csc_measure_sensor.last_crank_event_time)
            timeDifference =
                    (float) (65535 + last_crank_event_time - csc_measure_sensor.last_crank_event_time) / 1024.0f; // [s]
        else
            timeDifference =
                    (float) (last_crank_event_time - csc_measure_sensor.last_crank_event_time) / 1024.0f; // [s]

        float crankCadence =
                (float) (crank_revolutions - csc_measure_sensor.last_crank_revolutions) * 60.0f / timeDifference;
        if (crankCadence > 0) {
            //final float gearRatio = mWheelCadence / crankCadence;

            ESP_LOGI(TAG, "crank_cadence: %f", crankCadence);
        }
    }
    if (!csc_measure_sensor.first_crank_data_get) {
        csc_measure_sensor.first_crank_data_get = true;
    }
    csc_measure_sensor.last_crank_revolutions = crank_revolutions;
    csc_measure_sensor.last_crank_event_time = last_crank_event_time;
}

// 解析csc数据
void ble_parse_csc_data(esp_ble_gattc_cb_param_t *p_data) {
    uint8_t flag = *p_data->notify.value;
    bool has_wheel_revolutions = flag & 0x01;
    bool has_crank_revolutions = flag & 0x02;

    p_data->notify.value += 1;
    if (has_wheel_revolutions) {
        uint32_t wheel_revolutions = *((uint32_t *) p_data->notify.value);
        p_data->notify.value += 4;

        // 1/1024s
        uint16_t last_wheel_revolutions_time = *((uint16_t *) p_data->notify.value);
        p_data->notify.value += 2;

        //ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, wheel revolutions : %d, last time %d", wheel_revolutions, last_wheel_revolutions_time);
        on_wheel_measurement_received(wheel_revolutions, last_wheel_revolutions_time);
    }

    if (has_crank_revolutions) {
        uint16_t crank_revolutions = *((uint16_t *) p_data->notify.value);
        p_data->notify.value += 2;

        uint16_t last_crank_revolutions_time = *((uint16_t *) p_data->notify.value);
        p_data->notify.value += 2;

        ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, crank revolutions : %d, last time %d", crank_revolutions, last_crank_revolutions_time);

        on_crank_measurement_received(crank_revolutions, last_crank_revolutions_time);
    }
}