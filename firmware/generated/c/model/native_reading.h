/*
 * native_reading.h
 *
 * Native instrument reading with independent freshness, explicit unit and origin. Reception quality is not calibration accuracy. Keep device counters separate from browser/host session integrals; any uncommissioned counter scale remains raw.
 */

#ifndef _native_reading_H_
#define _native_reading_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct native_reading_t native_reading_t;


// Enum QUALITY for native_reading

typedef enum  { labbridge_api_native_reading_QUALITY_NULL = 0, labbridge_api_native_reading_QUALITY_valid, labbridge_api_native_reading_QUALITY_stale, labbridge_api_native_reading_QUALITY_unavailable } labbridge_api_native_reading_QUALITY_e;

char* native_reading_quality_ToString(labbridge_api_native_reading_QUALITY_e quality);

labbridge_api_native_reading_QUALITY_e native_reading_quality_FromString(char* quality);



typedef struct native_reading_t {
    char *id; // string
    double value; //numeric
    char *unit; // string
    char *origin; // string
    labbridge_api_native_reading_QUALITY_e quality; //enum
    int received_ms; //numeric
    int age_ms; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} native_reading_t;

__attribute__((deprecated)) native_reading_t *native_reading_create(
    char *id,
    double value,
    char *unit,
    char *origin,
    labbridge_api_native_reading_QUALITY_e quality,
    int received_ms,
    int age_ms
);

void native_reading_free(native_reading_t *native_reading);

native_reading_t *native_reading_parseFromJSON(cJSON *native_readingJSON);

cJSON *native_reading_convertToJSON(native_reading_t *native_reading);

#endif /* _native_reading_H_ */

