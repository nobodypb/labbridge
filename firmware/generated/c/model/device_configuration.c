#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "device_configuration.h"



static device_configuration_t *device_configuration_create_internal(
    char *id,
    char *driver,
    char *transport,
    char *endpoint,
    list_t *pins,
    safety_limits_t *limits
    ) {
    device_configuration_t *device_configuration_local_var = malloc(sizeof(device_configuration_t));
    if (!device_configuration_local_var) {
        return NULL;
    }
    device_configuration_local_var->id = id;
    device_configuration_local_var->driver = driver;
    device_configuration_local_var->transport = transport;
    device_configuration_local_var->endpoint = endpoint;
    device_configuration_local_var->pins = pins;
    device_configuration_local_var->limits = limits;

    device_configuration_local_var->_library_owned = 1;
    return device_configuration_local_var;
}

__attribute__((deprecated)) device_configuration_t *device_configuration_create(
    char *id,
    char *driver,
    char *transport,
    char *endpoint,
    list_t *pins,
    safety_limits_t *limits
    ) {
    return device_configuration_create_internal (
        id,
        driver,
        transport,
        endpoint,
        pins,
        limits
        );
}

void device_configuration_free(device_configuration_t *device_configuration) {
    if(NULL == device_configuration){
        return ;
    }
    if(device_configuration->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "device_configuration_free");
        return ;
    }
    listEntry_t *listEntry;
    if (device_configuration->id) {
        free(device_configuration->id);
        device_configuration->id = NULL;
    }
    if (device_configuration->driver) {
        free(device_configuration->driver);
        device_configuration->driver = NULL;
    }
    if (device_configuration->transport) {
        free(device_configuration->transport);
        device_configuration->transport = NULL;
    }
    if (device_configuration->endpoint) {
        free(device_configuration->endpoint);
        device_configuration->endpoint = NULL;
    }
    if (device_configuration->pins) {
        list_ForEach(listEntry, device_configuration->pins) {
            free(listEntry->data);
        }
        list_freeList(device_configuration->pins);
        device_configuration->pins = NULL;
    }
    if (device_configuration->limits) {
        safety_limits_free(device_configuration->limits);
        device_configuration->limits = NULL;
    }
    free(device_configuration);
}

cJSON *device_configuration_convertToJSON(device_configuration_t *device_configuration) {
    cJSON *item = cJSON_CreateObject();

    // device_configuration->id
    if (!device_configuration->id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "id", device_configuration->id) == NULL) {
    goto fail; //String
    }


    // device_configuration->driver
    if (!device_configuration->driver) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "driver", device_configuration->driver) == NULL) {
    goto fail; //String
    }


    // device_configuration->transport
    if (!device_configuration->transport) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "transport", device_configuration->transport) == NULL) {
    goto fail; //String
    }


    // device_configuration->endpoint
    if (!device_configuration->endpoint) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "endpoint", device_configuration->endpoint) == NULL) {
    goto fail; //String
    }


    // device_configuration->pins
    if(device_configuration->pins) {
    cJSON *pins = cJSON_AddArrayToObject(item, "pins");
    if(pins == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *pinsListEntry;
    list_ForEach(pinsListEntry, device_configuration->pins) {
    if(cJSON_AddNumberToObject(pins, "", *(double *)pinsListEntry->data) == NULL)
    {
        goto fail;
    }
    }
    }


    // device_configuration->limits
    if (!device_configuration->limits) {
        goto fail;
    }
    cJSON *limits_local_JSON = safety_limits_convertToJSON(device_configuration->limits);
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

device_configuration_t *device_configuration_parseFromJSON(cJSON *device_configurationJSON){

    device_configuration_t *device_configuration_local_var = NULL;

    // define the local list for device_configuration->pins
    list_t *pinsList = NULL;

    // define the local variable for device_configuration->limits
    safety_limits_t *limits_local_nonprim = NULL;

    // device_configuration->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "id");
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

    // device_configuration->driver
    cJSON *driver = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "driver");
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

    // device_configuration->transport
    cJSON *transport = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "transport");
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

    // device_configuration->endpoint
    cJSON *endpoint = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "endpoint");
    if (cJSON_IsNull(endpoint)) {
        endpoint = NULL;
    }
    if (!endpoint) {
        goto end;
    }

    
    if(!cJSON_IsString(endpoint))
    {
    goto end; //String
    }

    // device_configuration->pins
    cJSON *pins = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "pins");
    if (cJSON_IsNull(pins)) {
        pins = NULL;
    }
    if (pins) { 
    cJSON *pins_local = NULL;
    if(!cJSON_IsArray(pins)) {
        goto end;//primitive container
    }
    pinsList = list_createList();

    cJSON_ArrayForEach(pins_local, pins)
    {
        if(!cJSON_IsNumber(pins_local))
        {
            goto end;
        }
        double *pins_local_value = calloc(1, sizeof(double));
        if(!pins_local_value)
        {
            goto end;
        }
        *pins_local_value = pins_local->valuedouble;
        list_addElement(pinsList , pins_local_value);
    }
    }

    // device_configuration->limits
    cJSON *limits = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "limits");
    if (cJSON_IsNull(limits)) {
        limits = NULL;
    }
    if (!limits) {
        goto end;
    }

    
    limits_local_nonprim = safety_limits_parseFromJSON(limits); //nonprimitive


    device_configuration_local_var = device_configuration_create_internal (
        strdup(id->valuestring),
        strdup(driver->valuestring),
        strdup(transport->valuestring),
        strdup(endpoint->valuestring),
        pins ? pinsList : NULL,
        limits_local_nonprim
        );

    return device_configuration_local_var;
end:
    if (pinsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, pinsList) {
            free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(pinsList);
        pinsList = NULL;
    }
    if (limits_local_nonprim) {
        safety_limits_free(limits_local_nonprim);
        limits_local_nonprim = NULL;
    }
    return NULL;

}
