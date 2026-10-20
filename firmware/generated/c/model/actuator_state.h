/*
 * actuator_state.h
 *
 * Current actor-specific state, output confirmation and cached settings. Unknown output must not be treated as OFF.
 */

#ifndef _actuator_state_H_
#define _actuator_state_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct actuator_state_t actuator_state_t;

#include "actuator_settings.h"

// Enum STATE for actuator_state

typedef enum  { labbridge_api_actuator_state_STATE_NULL = 0, labbridge_api_actuator_state_STATE_unavailable, labbridge_api_actuator_state_STATE_ready, labbridge_api_actuator_state_STATE_busy, labbridge_api_actuator_state_STATE_fault } labbridge_api_actuator_state_STATE_e;

char* actuator_state_state_ToString(labbridge_api_actuator_state_STATE_e state);

labbridge_api_actuator_state_STATE_e actuator_state_state_FromString(char* state);

// Enum OUTPUT for actuator_state

typedef enum  { labbridge_api_actuator_state_OUTPUT_NULL = 0, labbridge_api_actuator_state_OUTPUT_on, labbridge_api_actuator_state_OUTPUT_off, labbridge_api_actuator_state_OUTPUT_unknown } labbridge_api_actuator_state_OUTPUT_e;

char* actuator_state_output_ToString(labbridge_api_actuator_state_OUTPUT_e output);

labbridge_api_actuator_state_OUTPUT_e actuator_state_output_FromString(char* output);



typedef struct actuator_state_t {
    char *device_id; // string
    char *component_id; // string
    labbridge_api_actuator_state_STATE_e state; //enum
    labbridge_api_actuator_state_OUTPUT_e output; //enum
    struct actuator_settings_t *settings; //model

    int _library_owned; // Is the library responsible for freeing this object?
} actuator_state_t;

__attribute__((deprecated)) actuator_state_t *actuator_state_create(
    char *device_id,
    char *component_id,
    labbridge_api_actuator_state_STATE_e state,
    labbridge_api_actuator_state_OUTPUT_e output,
    actuator_settings_t *settings
);

void actuator_state_free(actuator_state_t *actuator_state);

actuator_state_t *actuator_state_parseFromJSON(cJSON *actuator_stateJSON);

cJSON *actuator_state_convertToJSON(actuator_state_t *actuator_state);

#endif /* _actuator_state_H_ */

