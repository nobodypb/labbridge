/*
 * device_configuration.h
 *
 * One configured physical endpoint. Select a driver-supported transport, stable identity and explicitly approved pins. Resource conflicts and invalid driver calibration are rejected before allocation. Read-only sensors need no actuator limits.
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
#include "ina228_configuration.h"
#include "interlock_configuration.h"
#include "safety_limits.h"
#include "yocto_watt_configuration.h"



typedef struct device_configuration_t {
    char *id; // string
    char *driver; // string
    char *transport; // string
    char *endpoint; // string
    list_t *pins; //primitive container
    struct safety_limits_t *limits; //model
    struct cell_temperature_guard_t *cell_temperature; //model
    struct interlock_configuration_t *interlock; //model
    struct ina228_configuration_t *ina228; //model
    struct yocto_watt_configuration_t *yocto_watt; //model
    int sensor_max_age_ms; //numeric

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
    interlock_configuration_t *interlock,
    ina228_configuration_t *ina228,
    yocto_watt_configuration_t *yocto_watt,
    int sensor_max_age_ms
);

void device_configuration_free(device_configuration_t *device_configuration);

device_configuration_t *device_configuration_parseFromJSON(cJSON *device_configurationJSON);

cJSON *device_configuration_convertToJSON(device_configuration_t *device_configuration);

#endif /* _device_configuration_H_ */

