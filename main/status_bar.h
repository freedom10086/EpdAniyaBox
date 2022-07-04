#ifndef __STATUS_BAR_H
#define __STATUS_BAR_H

#include "lvgl.h"
#include "esp_types.h"

#include "common.h"

ESP_EVENT_DECLARE_BASE(STATUS_BAR_UPDATE_EVENT);

typedef struct {
    uint8_t hour;      /*!< Hour */
    uint8_t minute;    /*!< Minute */
    uint8_t second;    /*!< Second */
    uint16_t thousand; /*!< Thousand */
} my_time_t;

/**
 * @brief GPS date
 *
 */
typedef struct {
    uint8_t day;   /*!< Day (start from 1) */
    uint8_t month; /*!< Month (start from 1) */
    uint16_t year; /*!< Year (start from 2000) */
} my_date_t;

typedef struct {
    float latitude;                                                /*!< Latitude (degrees) */
    float longitude;                                               /*!< Longitude (degrees) */
    float altitude;                                                /*!< Altitude (meters) */
    my_time_t tim;                                                /*!< time in UTC */
    my_date_t date;                                               /*!< Fix date */
    bool valid;                                                    /*!< GPS validity */
    float speed;                                                   /*!< Ground speed, unit: m/s */
    float cog;                                                     /*!< Course over ground */
    float variation;                                               /*!< Magnetic variation */
    uint8_t sats_in_use;                                           /*!< Number of satellites in use */
    uint8_t battery_level; // from 0 - 100
} status_bar_t;

esp_event_loop_handle_t init_status_bar(lv_obj_t * parent);

void update_status_bar(esp_event_loop_handle_t loop_handle);

esp_err_t deinit_status_bar(esp_event_loop_handle_t loop_handle);

#endif