#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "device.h"


char* device_state_ToString(labbridge_api_device_STATE_e state) {
    char* stateArray[] =  { "NULL", "unavailable", "ready", "busy", "fault" };
    return stateArray[state];
}

labbridge_api_device_STATE_e device_state_FromString(char* state){
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
char* device_output_ToString(labbridge_api_device_OUTPUT_e output) {
    char* outputArray[] =  { "NULL", "on", "off", "unknown" };
    return outputArray[output];
}

labbridge_api_device_OUTPUT_e device_output_FromString(char* output){
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

static device_t *device_create_internal(
    char *id,
    char *driver,
    char *identity,
    char *transport,
    list_t *capabilities,
    labbridge_api_device_STATE_e state,
    labbridge_api_device_OUTPUT_e output,
    safety_limits_t *limits
    ) {
    device_t *device_local_var = malloc(sizeof(device_t));
    if (!device_local_var) {
        return NULL;
    }
    device_local_var->id = id;
    device_local_var->driver = driver;
    device_local_var->identity = identity;
    device_local_var->transport = transport;
    device_local_var->capabilities = capabilities;
    device_local_var->state = state;
    device_local_var->output = output;
    device_local_var->limits = limits;

    device_local_var->_library_owned = 1;
    return device_local_var;
}

__attribute__((deprecated)) device_t *device_create(
    char *id,
    char *driver,
    char *identity,
    char *transport,
    list_t *capabilities,
    labbridge_api_device_STATE_e state,
    labbridge_api_device_OUTPUT_e output,
    safety_limits_t *limits
    ) {
    return device_create_internal (
        id,
        driver,
        identity,
        transport,
        capabilities,
        state,
        output,
        limits
        );
}

void device_free(device_t *device) {
    if(NULL == device){
        return ;
    }
    if(device->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "device_free");
        return ;
    }
    listEntry_t *listEntry;
    if (device->id) {
        free(device->id);
        device->id = NULL;
    }
    if (device->driver) {
        free(device->driver);
        device->driver = NULL;
    }
    if (device->identity) {
        free(device->identity);
        device->identity = NULL;
    }
    if (device->transport) {
        free(device->transport);
        device->transport = NULL;
    }
    if (device->capabilities) {
        list_ForEach(listEntry, device->capabilities) {
            free(listEntry->data);
        }
        list_freeList(device->capabilities);
        device->capabilities = NULL;
    }
    if (device->limits) {
        safety_limits_free(device->limits);
        device->limits = NULL;
    }
    free(device);
}

cJSON *device_convertToJSON(device_t *device) {
    cJSON *item = cJSON_CreateObject();

    // device->id
    if (!device->id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "id", device->id) == NULL) {
    goto fail; //String
    }


    // device->driver
    if (!device->driver) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "driver", device->driver) == NULL) {
    goto fail; //String
    }


    // device->identity
    if (!device->identity) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "identity", device->identity) == NULL) {
    goto fail; //String
    }


    // device->transport
    if (!device->transport) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "transport", device->transport) == NULL) {
    goto fail; //String
    }


    // device->capabilities
    if (!device->capabilities) {
        goto fail;
    }
    cJSON *capabilities = cJSON_AddArrayToObject(item, "capabilities");
    if(capabilities == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *capabilitiesListEntry;
    list_ForEach(capabilitiesListEntry, device->capabilities) {
    if(cJSON_AddStringToObject(capabilities, "", capabilitiesListEntry->data) == NULL)
    {
        goto fail;
    }
    }


    // device->state
    if (labbridge_api_device_STATE_NULL == device->state) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "state", device_state_ToString(device->state)) == NULL)
    {
    goto fail; //Enum
    }


    // device->output
    if (labbridge_api_device_OUTPUT_NULL == device->output) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "output", device_output_ToString(device->output)) == NULL)
    {
    goto fail; //Enum
    }


    // device->limits
    if (!device->limits) {
        goto fail;
    }
    cJSON *limits_local_JSON = safety_limits_convertToJSON(device->limits);
    if(limits_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "limits", limits_local_JSON);
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

device_t *device_parseFromJSON(cJSON *deviceJSON){

    device_t *device_local_var = NULL;

    // define the local list for device->capabilities
    list_t *capabilitiesList = NULL;

    // define the local variable for device->limits
    safety_limits_t *limits_local_nonprim = NULL;

    // device->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(deviceJSON, "id");
    if (cJSON_IsNull(id)) {
        id = NULL;
    }
    if (!id) {
        goto end;
    }

    
    if(!cJSON_IsString(id))
    {
    goto end; //String
    }

    // device->driver
    cJSON *driver = cJSON_GetObjectItemCaseSensitive(deviceJSON, "driver");
    if (cJSON_IsNull(driver)) {
        driver = NULL;
    }
    if (!driver) {
        goto end;
    }

    
    if(!cJSON_IsString(driver))
    {
    goto end; //String
    }

    // device->identity
    cJSON *identity = cJSON_GetObjectItemCaseSensitive(deviceJSON, "identity");
    if (cJSON_IsNull(identity)) {
        identity = NULL;
    }
    if (!identity) {
        goto end;
    }

    
    if(!cJSON_IsString(identity))
    {
    goto end; //String
    }

    // device->transport
    cJSON *transport = cJSON_GetObjectItemCaseSensitive(deviceJSON, "transport");
    if (cJSON_IsNull(transport)) {
        transport = NULL;
    }
    if (!transport) {
        goto end;
    }

    
    if(!cJSON_IsString(transport))
    {
    goto end; //String
    }

    // device->capabilities
    cJSON *capabilities = cJSON_GetObjectItemCaseSensitive(deviceJSON, "capabilities");
    if (cJSON_IsNull(capabilities)) {
        capabilities = NULL;
    }
    if (!capabilities) {
        goto end;
    }

    
    cJSON *capabilities_local = NULL;
    if(!cJSON_IsArray(capabilities)) {
        goto end;//primitive container
    }
    capabilitiesList = list_createList();

    cJSON_ArrayForEach(capabilities_local, capabilities)
    {
        if(!cJSON_IsString(capabilities_local))
        {
            goto end;
        }
        list_addElement(capabilitiesList , strdup(capabilities_local->valuestring));
    }

    // device->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(deviceJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    if (!state) {
        goto end;
    }

    labbridge_api_device_STATE_e stateVariable;
    
    if(!cJSON_IsString(state))
    {
    goto end; //Enum
    }
    stateVariable = device_state_FromString(state->valuestring);

    // device->output
    cJSON *output = cJSON_GetObjectItemCaseSensitive(deviceJSON, "output");
    if (cJSON_IsNull(output)) {
        output = NULL;
    }
    if (!output) {
        goto end;
    }

    labbridge_api_device_OUTPUT_e outputVariable;
    
    if(!cJSON_IsString(output))
    {
    goto end; //Enum
    }
    outputVariable = device_output_FromString(output->valuestring);

    // device->limits
    cJSON *limits = cJSON_GetObjectItemCaseSensitive(deviceJSON, "limits");
    if (cJSON_IsNull(limits)) {
        limits = NULL;
    }
    if (!limits) {
        goto end;
    }

    
    limits_local_nonprim = safety_limits_parseFromJSON(limits); //nonprimitive


    device_local_var = device_create_internal (
        strdup(id->valuestring),
        strdup(driver->valuestring),
        strdup(identity->valuestring),
        strdup(transport->valuestring),
        capabilitiesList,
        stateVariable,
        outputVariable,
        limits_local_nonprim
        );

    return device_local_var;
end:
    if (capabilitiesList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, capabilitiesList) {
            free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(capabilitiesList);
        capabilitiesList = NULL;
    }
    if (limits_local_nonprim) {
        safety_limits_free(limits_local_nonprim);
        limits_local_nonprim = NULL;
    }
    return NULL;

}
