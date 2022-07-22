#ifndef SPL06_H
#define SPL06_H

#include "pressure_common.h"

typedef struct {
    bool en_fifo;

    // TMP_PRC[2:0]        : 0      | 1 | 2 | 3 | 4  | 5  | 6  | 7
    // oversampling (times): single | 2 | 4 | 8 | 16 | 32 | 64 | 128
    uint8_t oversampling_t;

    // PM_PRC[3:0]         : 0      | 1   | 2   | 3    | 4    | 5    | 6     | 7
    // oversampling (times): single | 2   | 4   | 8    | 16   | 32   | 64    | 128
    uint8_t oversampling_p;

    int32_t raw_temp;

    bool raw_temp_valid;

    int32_t raw_pressure;

    float altitude;

    // measurement mode and type
    /**
     * Standby Mode
        000 - Idle / Stop background measurement
       Command Mode
        001 - Pressure measurement
        010 - Temperature measurement
        011 - na.
        100 - na.
       Background Mode
        101 - Continuous pressure measurement
        110 - Continuous temperature measurement
        111 - Continuous pressure and temperature measurement
     */
    uint8_t meas_ctrl;

    bool coef_ready;

    bool sensor_ready;

    bool temp_ready;

    bool pressure_ready;

    bool fifo_full;

    bool fifo_empty;

    int32_t fifo[32];

    uint8_t fifo_len;
} spl06_t;

spl06_t* spl06_init();
void spl06_fifo_state(spl06_t *spl06);
void spl06_meassure_state(spl06_t *spl06);
void spl06_reset(spl06_t *spl06);
void spl06_start(spl06_t *spl06, bool en_fifo);
void spl06_stop(spl06_t *spl06);
uint32_t spl06_read_raw_temp(spl06_t *spl06);
uint32_t spl06_read_raw_pressure(spl06_t *spl06);
float spl06_get_temperature(spl06_t *spl06);
float spl06_get_pressure(spl06_t *spl06);
void spl06_read_raw_fifo(spl06_t *spl06);

#endif