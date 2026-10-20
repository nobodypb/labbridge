#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "actuator_state.h"


char* actuator_state_state_ToString(labbridge_api_actuator_state_STATE_e state) {
    char* stateArray[] =  { "NULL", "unavailable", "ready", "busy", "fault" };
    return stateArray[state];
}

labbridge_api_actuator_state_STATE_e actuator_state_state_FromString(char* state){
    int stringToReturn = 0;
    char *stateArray[] =  { "NULL", "unavailable", "ready", "busy", "fault" };
    size_t sizeofArray = sizeof(stateArray) / sizeof(stateArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(state, stateArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}
char* actuator_state_output_ToString(labbridge_api_actuator_state_OUTPUT_e output) {
    char* outputArray[] =  { "NULL", "on", "off", "unknown" };
    return outputArray[output];
}

labbridge_api_actuator_state_OUTPUT_e actuator_state_output_FromString(char* output){
    int stringToReturn = 0;
    char *outputArray[] =  { "NULL", "on", "off", "unknown" };
    size_t sizeofArray = sizeof(outputArray) / sizeof(outputArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(output, outputArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static actuator_state_t *actuator_state_create_internal(
    char *device_id,
    char *component_id,
    labbridge_api_actuator_state_STATE_e state,
    labbridge_api_actuator_state_OUTPUT_e output,
    actuator_settings_t *settings
    ) {
    actuator_state_t *actuator_state_local_var = malloc(sizeof(actuator_state_t));
    if (!actuator_state_local_var) {
        return NULL;
    }
    actuator_state_local_var->device_id = device_id;
    actuator_state_local_var->component_id = component_id;
    actuator_state_local_var->state = state;
    actuator_state_local_var->output = output;
    actuator_state_local_var->settings = settings;

    actuator_state_local_var->_library_owned = 1;
    return actuator_state_local_var;
}

__attribute__((deprecated)) actuator_state_t *actuator_state_create(
    char *device_id,
    char *component_id,
    labbridge_api_actuator_state_STATE_e state,
    labbridge_api_actuator_state_OUTPUT_e output,
    actuator_settings_t *settings
    ) {
    return actuator_state_create_internal (
        device_id,
        component_id,
        state,
        output,
        settings
        );
}

void actuator_state_free(actuator_state_t *actuator_state) {
    if(NULL == actuator_state){
        return ;
    }
    if(actuator_state->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "actuator_state_free");
        return ;
    }
    listEntry_t *listEntry;
    if (actuator_state->device_id) {
        free(actuator_state->device_id);
        actuator_state->device_id = NULL;
    }
    if (actuator_state->component_id) {
        free(actuator_state->component_id);
        actuator_state->component_id = NULL;
    }
    if (actuator_state->settings) {
        actuator_settings_free(actuator_state->settings);
        actuator_state->settings = NULL;
    }
    free(actuator_state);
}

cJSON *actuator_state_convertToJSON(actuator_state_t *actuator_state) {
    cJSON *item = cJSON_CreateObject();

    // actuator_state->device_id
    if (!actuator_state->device_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "device_id", actuator_state->device_id) == NULL) {
    goto fail; //String
    }


    // actuator_state->component_id
    if (!actuator_state->component_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "component_id", actuator_state->component_id) == NULL) {
    goto fail; //String
    }


    // actuator_state->state
    if (labbridge_api_actuator_state_STATE_NULL == actuator_state->state) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "state", actuator_state_state_ToString(actuator_state->state)) == NULL)
    {
    goto fail; //Enum
    }


    // actuator_state->output
    if (labbridge_api_actuator_state_OUTPUT_NULL == actuator_state->output) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "output", actuator_state_output_ToString(actuator_state->output)) == NULL)
    {
    goto fail; //Enum
    }


    // actuator_state->settings
    if (!actuator_state->settings) {
        goto fail;
    }
    cJSON *settings_local_JSON = actuator_settings_convertToJSON(actuator_state->settings);
    if(settings_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "settings", settings_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

actuator_state_t *actuator_state_parseFromJSON(cJSON *actuator_stateJSON){

    actuator_state_t *actuator_state_local_var = NULL;

    // define the local variable for actuator_state->settings
    actuator_settings_t *settings_local_nonprim = NULL;

    // actuator_state->device_id
    cJSON *device_id = cJSON_GetObjectItemCaseSensitive(actuator_stateJSON, "device_id");
    if (cJSON_IsNull(device_id)) {
        device_id = NULL;
    }
    if (!device_id) {
        goto end;
    }

    
    if(!cJSON_IsString(device_id))
    {
    goto end; //String
    }

    // actuator_state->component_id
    cJSON *component_id = cJSON_GetObjectItemCaseSensitive(actuator_stateJSON, "component_id");
    if (cJSON_IsNull(component_id)) {
        component_id = NULL;
    }
    if (!component_id) {
        goto end;
    }

    
    if(!cJSON_IsString(component_id))
    {
    goto end; //String
    }

    // actuator_state->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(actuator_stateJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    if (!state) {
        goto end;
    }

    labbridge_api_actuator_state_STATE_e stateVariable;
    
    if(!cJSON_IsString(state))
    {
    goto end; //Enum
    }
    stateVariable = actuator_state_state_FromString(state->valuestring);

    // actuator_state->output
    cJSON *output = cJSON_GetObjectItemCaseSensitive(actuator_stateJSON, "output");
    if (cJSON_IsNull(output)) {
        output = NULL;
    }
    if (!output) {
        goto end;
    }

    labbridge_api_actuator_state_OUTPUT_e outputVariable;
    
    if(!cJSON_IsString(output))
    {
    goto end; //Enum
    }
    outputVariable = actuator_state_output_FromString(output->valuestring);

    // actuator_state->settings
    cJSON *settings = cJSON_GetObjectItemCaseSensitive(actuator_stateJSON, "settings");
    if (cJSON_IsNull(settings)) {
        settings = NULL;
    }
    if (!settings) {
        goto end;
    }

    
    settings_local_nonprim = actuator_settings_parseFromJSON(settings); //nonprimitive


    actuator_state_local_var = actuator_state_create_internal (
        strdup(device_id->valuestring),
        strdup(component_id->valuestring),
        stateVariable,
        outputVariable,
        settings_local_nonprim
        );

    return actuator_state_local_var;
end:
    if (settings_local_nonprim) {
        actuator_settings_free(settings_local_nonprim);
        settings_local_nonprim = NULL;
    }
    return NULL;

}
