/*
 * temperature_channel.h
 *
 * One typed temperature channel. Only fresh external_probe/cell channels qualify for cell guards; internal temperature cannot substitute for a cell probe.
 */

#ifndef _temperature_channel_H_
#define _temperature_channel_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct temperature_channel_t temperature_channel_t;


// Enum KIND for temperature_channel

typedef enum  { labbridge_api_temperature_channel_KIND_NULL = 0, labbridge_api_temperature_channel_KIND_internal, labbridge_api_temperature_channel_KIND_external_probe, labbridge_api_temperature_channel_KIND_cell } labbridge_api_temperature_channel_KIND_e;

char* temperature_channel_kind_ToString(labbridge_api_temperature_channel_KIND_e kind);

labbridge_api_temperature_channel_KIND_e temperature_channel_kind_FromString(char* kind);

// Enum QUALITY for temperature_channel

typedef enum  { labbridge_api_temperature_channel_QUALITY_NULL = 0, labbridge_api_temperature_channel_QUALITY_valid, labbridge_api_temperature_channel_QUALITY_stale, labbridge_api_temperature_channel_QUALITY_unavailable } labbridge_api_temperature_channel_QUALITY_e;

char* temperature_channel_quality_ToString(labbridge_api_temperature_channel_QUALITY_e quality);

labbridge_api_temperature_channel_QUALITY_e temperature_channel_quality_FromString(char* quality);



typedef struct temperature_channel_t {
    char *id; // string
    labbridge_api_temperature_channel_KIND_e kind; //enum
    labbridge_api_temperature_channel_QUALITY_e quality; //enum
    double value_c; //numeric
    long received_ms; //numeric
    long age_ms; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} temperature_channel_t;

__attribute__((deprecated)) temperature_channel_t *temperature_channel_create(
    char *id,
    labbridge_api_temperature_channel_KIND_e kind,
    labbridge_api_temperature_channel_QUALITY_e quality,
    double value_c,
    long received_ms,
    long age_ms
);

void temperature_channel_free(temperature_channel_t *temperature_channel);

temperature_channel_t *temperature_channel_parseFromJSON(cJSON *temperature_channelJSON);

cJSON *temperature_channel_convertToJSON(temperature_channel_t *temperature_channel);

#endif /* _temperature_channel_H_ */

