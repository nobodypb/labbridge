/*
 * temperature_channels.h
 *
 * Cached per-channel temperature values with independent availability and freshness.
 */

#ifndef _temperature_channels_H_
#define _temperature_channels_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct temperature_channels_t temperature_channels_t;

#include "temperature_channel.h"



typedef struct temperature_channels_t {
    list_t *channels; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} temperature_channels_t;

__attribute__((deprecated)) temperature_channels_t *temperature_channels_create(
    list_t *channels
);

void temperature_channels_free(temperature_channels_t *temperature_channels);

temperature_channels_t *temperature_channels_parseFromJSON(cJSON *temperature_channelsJSON);

cJSON *temperature_channels_convertToJSON(temperature_channels_t *temperature_channels);

#endif /* _temperature_channels_H_ */

