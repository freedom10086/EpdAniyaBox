#ifndef MS5611
#define MS5611

void ms5611_init();
void ms5611_reset();
void ms5611_read_pressure_pre(void);
void ms5611_read_pressure(void);
void ms5611_read_temp_pre(void);
void ms5611_read_temp(void);

float ms5611_pressure_caculate(void);

#endif