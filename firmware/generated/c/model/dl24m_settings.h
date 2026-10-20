/*
 * dl24m_settings.h
 *
 * 
 */

#ifndef _dl24m_settings_H_
#define _dl24m_settings_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct dl24m_settings_t dl24m_settings_t;




typedef struct dl24m_settings_t {
    double cutoff_voltage_v; //numeric
    long time_limit_s; //numeric
    long working_brightness; //numeric
    long standby_brightness; //numeric
    long current_resolution; //numeric
    double overvoltage_v; //numeric
    double overcurrent_a; //numeric
    double overpower_w; //numeric
    long overtemperature_c; //numeric
    long fan_temperature_c; //numeric
    long battery_overtemperature_c; //numeric
    long language; //numeric
    int mode; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} dl24m_settings_t;

__attribute__((deprecated)) dl24m_settings_t *dl24m_settings_create(
    double cutoff_voltage_v,
    long time_limit_s,
    long working_brightness,
    long standby_brightness,
    long current_resolution,
    double overvoltage_v,
    double overcurrent_a,
    double overpower_w,
    long overtemperature_c,
    long fan_temperature_c,
    long battery_overtemperature_c,
    long language,
    int mode
);

void dl24m_settings_free(dl24m_settings_t *dl24m_settings);

dl24m_settings_t *dl24m_settings_parseFromJSON(cJSON *dl24m_settingsJSON);

cJSON *dl24m_settings_convertToJSON(dl24m_settings_t *dl24m_settings);

#endif /* _dl24m_settings_H_ */

