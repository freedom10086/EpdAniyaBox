#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

typedef struct {
    float temperature;
    bool temperature_valid;

    float altitude;
    bool altitude_valid;

    float wheel_speed;
    bool wheel_speed_valid;

    float crank_cadence;
    float crank_cadence_valid;
} main_page_data_t;

void main_page_init();

void main_page_update_temperature(float temp);

void main_page_update_altitude(float altitude);

void main_page_update_speed(float speed);

void main_page_update_crank_cadence(float crank_cadence);

#endif