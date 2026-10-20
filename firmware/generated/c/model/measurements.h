/*
 * measurements.h
 *
 * Compatibility device-level snapshot. Optional fields are present only when supported/available; absence must never be converted into a measured zero. Prefer SensorReading for new meter/composite integrations.
 */

#ifndef _measurements_H_
#define _measurements_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct measurements_t measurements_t;

#include "temperature_channel.h"

// Enum QUALITY for measurements

typedef enum  { labbridge_api_measurements_QUALITY_NULL = 0, labbridge_api_measurements_QUALITY_valid, labbridge_api_measurements_QUALITY_stale, labbridge_api_measurements_QUALITY_unavailable } labbridge_api_measurements_QUALITY_e;

char* measurements_quality_ToString(labbridge_api_measurements_QUALITY_e quality);

labbridge_api_measurements_QUALITY_e measurements_quality_FromString(char* quality);

// Enum REGULATIONMODE for measurements

typedef enum  { labbridge_api_measurements_REGULATIONMODE_NULL = 0, labbridge_api_measurements_REGULATIONMODE_cc, labbridge_api_measurements_REGULATIONMODE_cv, labbridge_api_measurements_REGULATIONMODE_unknown } labbridge_api_measurements_REGULATIONMODE_e;

char* measurements_regulation_mode_ToString(labbridge_api_measurements_REGULATIONMODE_e regulation_mode);

labbridge_api_measurements_REGULATIONMODE_e measurements_regulation_mode_FromString(char* regulation_mode);



typedef struct measurements_t {
    char *device_id; // string
    long sequence; //numeric
    long received_ms; //numeric
    long age_ms; //numeric
    labbridge_api_measurements_QUALITY_e quality; //enum
    double voltage_v; //numeric
    double current_a; //numeric
    double power_w; //numeric
    double capacity_ah; //numeric
    double energy_wh; //numeric
    list_t *temperature_channels; //nonprimitive container
    double set_voltage_v; //numeric
    double set_current_a; //numeric
    labbridge_api_measurements_REGULATIONMODE_e regulation_mode; //enum
    int protection_code; //numeric
    char *boot_id; // string
    double input_voltage_v; //numeric
    double battery_voltage_v; //numeric
    int battery_detected; //boolean

    int _library_owned; // Is the library responsible for freeing this object?
} measurements_t;

__attribute__((deprecated)) measurements_t *measurements_create(
    char *device_id,
    long sequence,
    long received_ms,
    long age_ms,
    labbridge_api_measurements_QUALITY_e quality,
    double voltage_v,
    double current_a,
    double power_w,
    double capacity_ah,
    double energy_wh,
    list_t *temperature_channels,
    double set_voltage_v,
    double set_current_a,
    labbridge_api_measurements_REGULATIONMODE_e regulation_mode,
    int protection_code,
    char *boot_id,
    double input_voltage_v,
    double battery_voltage_v,
    int battery_detected
);

void measurements_free(measurements_t *measurements);

measurements_t *measurements_parseFromJSON(cJSON *measurementsJSON);

cJSON *measurements_convertToJSON(measurements_t *measurements);

#endif /* _measurements_H_ */

