/*
 * cell_temperature_guard.h
 *
 * 
 */

#ifndef _cell_temperature_guard_H_
#define _cell_temperature_guard_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct cell_temperature_guard_t cell_temperature_guard_t;




typedef struct cell_temperature_guard_t {
    char *sensor_device_id; // string
    char *channel_id; // string
    double minimum_c; //numeric
    double maximum_c; //numeric
    int max_age_ms; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} cell_temperature_guard_t;

__attribute__((deprecated)) cell_temperature_guard_t *cell_temperature_guard_create(
    char *sensor_device_id,
    char *channel_id,
    double minimum_c,
    double maximum_c,
    int max_age_ms
);

void cell_temperature_guard_free(cell_temperature_guard_t *cell_temperature_guard);

cell_temperature_guard_t *cell_temperature_guard_parseFromJSON(cJSON *cell_temperature_guardJSON);

cJSON *cell_temperature_guard_convertToJSON(cell_temperature_guard_t *cell_temperature_guard);

#endif /* _cell_temperature_guard_H_ */

