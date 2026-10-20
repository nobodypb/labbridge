/*
 * measurements.h
 *
 * 
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
    list_t *temperature_channels
);

void measurements_free(measurements_t *measurements);

measurements_t *measurements_parseFromJSON(cJSON *measurementsJSON);

cJSON *measurements_convertToJSON(measurements_t *measurements);

#endif /* _measurements_H_ */

