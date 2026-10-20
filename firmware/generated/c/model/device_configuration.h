/*
 * device_configuration.h
 *
 * 
 */

#ifndef _device_configuration_H_
#define _device_configuration_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct device_configuration_t device_configuration_t;

#include "cell_temperature_guard.h"
#include "interlock_configuration.h"
#include "safety_limits.h"



typedef struct device_configuration_t {
    char *id; // string
    char *driver; // string
    char *transport; // string
    char *endpoint; // string
    list_t *pins; //primitive container
    struct safety_limits_t *limits; //model
    struct cell_temperature_guard_t *cell_temperature; //model
    struct interlock_configuration_t *interlock; //model

    int _library_owned; // Is the library responsible for freeing this object?
} device_configuration_t;

__attribute__((deprecated)) device_configuration_t *device_configuration_create(
    char *id,
    char *driver,
    char *transport,
    char *endpoint,
    list_t *pins,
    safety_limits_t *limits,
    cell_temperature_guard_t *cell_temperature,
    interlock_configuration_t *interlock
);

void device_configuration_free(device_configuration_t *device_configuration);

device_configuration_t *device_configuration_parseFromJSON(cJSON *device_configurationJSON);

cJSON *device_configuration_convertToJSON(device_configuration_t *device_configuration);

#endif /* _device_configuration_H_ */

