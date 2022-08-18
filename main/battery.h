#ifndef BATTERY_H
#define BATTERY_H

void battery_init(void);

int battery_get_voltage();

void battery_deinit(void);

#endif