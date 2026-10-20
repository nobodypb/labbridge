/*
 * sensor_reading.h
 *
 * 
 */

#ifndef _sensor_reading_H_
#define _sensor_reading_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct sensor_reading_t sensor_reading_t;


// Enum QUALITY for sensor_reading

typedef enum  { labbridge_api_sensor_reading_QUALITY_NULL = 0, labbridge_api_sensor_reading_QUALITY_valid, labbridge_api_sensor_reading_QUALITY_stale, labbridge_api_sensor_reading_QUALITY_unavailable } labbridge_api_sensor_reading_QUALITY_e;

char* sensor_reading_quality_ToString(labbridge_api_sensor_reading_QUALITY_e quality);

labbridge_api_sensor_reading_QUALITY_e sensor_reading_quality_FromString(char* quality);



typedef struct sensor_reading_t {
    char *device_id; // string
    char *component_id; // string
    char *boot_id; // string
    char *quantity; // string
    char *unit; // string
    labbridge_api_sensor_reading_QUALITY_e quality; //enum
    double value; //numeric
    long sequence; //numeric
    long received_ms; //numeric
    long age_ms; //numeric
    char *epoch; // string

    int _library_owned; // Is the library responsible for freeing this object?
} sensor_reading_t;

__attribute__((deprecated)) sensor_reading_t *sensor_reading_create(
    char *device_id,
    char *component_id,
    char *boot_id,
    char *quantity,
    char *unit,
    labbridge_api_sensor_reading_QUALITY_e quality,
    double value,
    long sequence,
    long received_ms,
    long age_ms,
    char *epoch
);

void sensor_reading_free(sensor_reading_t *sensor_reading);

sensor_reading_t *sensor_reading_parseFromJSON(cJSON *sensor_readingJSON);

cJSON *sensor_reading_convertToJSON(sensor_reading_t *sensor_reading);

#endif /* _sensor_reading_H_ */

