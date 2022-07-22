#include "pressure_common.h"

float calc_altitude(float pressure) {
    // calc height
    const float T1 = 15.0f + 273.15f;       // temperature at base height in Kelvin
    const float a = -6.5f / 1000.0f;       // temperature gradient in degrees per metre
    const float g = 9.80665f;              // gravity constant in m/s/s
    const float R = 287.05f;               // ideal gas constant in J/kg/K
    float pK = pressure / SEA_LEVEL_PRESSURE;
    float altitude = (((powf(pK, (-(a * R) / g))) * T1) - T1) / a;
    return altitude;
}

float calc_altitude_v2(float pressure) {
    float altitude = 44330 * (1 - pow(pressure / SEA_LEVEL_PRESSURE, 1 / 5.225));
    return altitude;
}