/*
 * device.h
 *
 * One physical instrument and transport/recovery identity. All its components share physical-device ownership, fault handling and lease expiry behavior.
 */

#ifndef _device_H_
#define _device_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct device_t device_t;

#include "component.h"
#include "feature_status.h"
#include "safety_limits.h"

// Enum STATE for device

typedef enum  { labbridge_api_device_STATE_NULL = 0, labbridge_api_device_STATE_unavailable, labbridge_api_device_STATE_ready, labbridge_api_device_STATE_busy, labbridge_api_device_STATE_fault } labbridge_api_device_STATE_e;

char* device_state_ToString(labbridge_api_device_STATE_e state);

labbridge_api_device_STATE_e device_state_FromString(char* state);

// Enum OUTPUT for device

typedef enum  { labbridge_api_device_OUTPUT_NULL = 0, labbridge_api_device_OUTPUT_on, labbridge_api_device_OUTPUT_off, labbridge_api_device_OUTPUT_unknown } labbridge_api_device_OUTPUT_e;

char* device_output_ToString(labbridge_api_device_OUTPUT_e output);

labbridge_api_device_OUTPUT_e device_output_FromString(char* output);



typedef struct device_t {
    char *id; // string
    char *driver; // string
    char *identity; // string
    char *transport; // string
    list_t *capabilities; //primitive container
    labbridge_api_device_STATE_e state; //enum
    labbridge_api_device_OUTPUT_e output; //enum
    struct safety_limits_t *limits; //model
    list_t *components; //nonprimitive container
    list_t *features; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} device_t;

__attribute__((deprecated)) device_t *device_create(
    char *id,
    char *driver,
    char *identity,
    char *transport,
    list_t *capabilities,
    labbridge_api_device_STATE_e state,
    labbridge_api_device_OUTPUT_e output,
    safety_limits_t *limits,
    list_t *components,
    list_t *features
);

void device_free(device_t *device);

device_t *device_parseFromJSON(cJSON *deviceJSON);

cJSON *device_convertToJSON(device_t *device);

#endif /* _device_H_ */

