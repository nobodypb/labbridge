/*
 * sensor_readings.h
 *
 * Cached readings for one physical device. Individual channels may have different validity/age; use sequence/epoch to avoid combining unrelated samples.
 */

#ifndef _sensor_readings_H_
#define _sensor_readings_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct sensor_readings_t sensor_readings_t;

#include "sensor_reading.h"



typedef struct sensor_readings_t {
    char *device_id; // string
    char *boot_id; // string
    list_t *readings; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} sensor_readings_t;

__attribute__((deprecated)) sensor_readings_t *sensor_readings_create(
    char *device_id,
    char *boot_id,
    list_t *readings
);

void sensor_readings_free(sensor_readings_t *sensor_readings);

sensor_readings_t *sensor_readings_parseFromJSON(cJSON *sensor_readingsJSON);

cJSON *sensor_readings_convertToJSON(sensor_readings_t *sensor_readings);

#endif /* _sensor_readings_H_ */

