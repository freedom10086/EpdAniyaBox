#ifndef EVENT_COMMON_H
#define EVENT_COMMON_H

/**
 * hrm
 */
typedef enum {
    BLE_HRM_SENSOR_UPDATE,
} ble_hrm_event_id_t;

// for event
typedef struct {
    uint16_t heart_rate;
} ble_hrm_data_t;

/**
 * csc
 */
typedef enum {
    BLE_CSC_SENSOR_UPDATE,
} ble_csc_event_id_t;

// for event
typedef struct {
    float wheel_total_distance;
    float wheel_cadence; // 轮圈速
    float wheel_speed; //轮速 km/h
    float crank_cadence; // 踏频
} ble_csc_data_t;


#endif