#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

typedef struct {
    float temperature;
    bool temperature_valid;

    float altitude;
    bool altitude_valid;
} main_page_data_t;

void main_page_init();

void main_page_update_temperature(float temp);

void main_page_update_altitude(float altitude);

#endif