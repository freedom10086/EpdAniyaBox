#ifndef SHT31_H
#define SHT31_H

typedef struct {
    bool data_valid;
    float temp;
    float hum;
} sht31_t;

sht31_t * sht31_init();
bool sht31_read_temp_hum();
void sht31_deinit();
#endif