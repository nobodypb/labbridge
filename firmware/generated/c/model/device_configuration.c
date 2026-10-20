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
    safety_limits_t *limits,
    cell_temperature_guard_t *cell_temperature,
    interlock_configuration_t *interlock,
    ina228_configuration_t *ina228,
    yocto_watt_configuration_t *yocto_watt,
    int sensor_max_age_ms,
    rd60xx_configuration_t *rd60xx
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
    device_configuration_local_var->cell_temperature = cell_temperature;
    device_configuration_local_var->interlock = interlock;
    device_configuration_local_var->ina228 = ina228;
    device_configuration_local_var->yocto_watt = yocto_watt;
    device_configuration_local_var->sensor_max_age_ms = sensor_max_age_ms;
    device_configuration_local_var->rd60xx = rd60xx;

    device_configuration_local_var->_library_owned = 1;
    return device_configuration_local_var;
}

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
    int sensor_max_age_ms,
    rd60xx_configuration_t *rd60xx
    ) {
    return device_configuration_create_internal (
        id,
        driver,
        transport,
        endpoint,
        pins,
        limits,
        cell_temperature,
        interlock,
        ina228,
        yocto_watt,
        sensor_max_age_ms,
        rd60xx
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
    if (device_configuration->cell_temperature) {
        cell_temperature_guard_free(device_configuration->cell_temperature);
        device_configuration->cell_temperature = NULL;
    }
    if (device_configuration->interlock) {
        interlock_configuration_free(device_configuration->interlock);
        device_configuration->interlock = NULL;
    }
    if (device_configuration->ina228) {
        ina228_configuration_free(device_configuration->ina228);
        device_configuration->ina228 = NULL;
    }
    if (device_configuration->yocto_watt) {
        yocto_watt_configuration_free(device_configuration->yocto_watt);
        device_configuration->yocto_watt = NULL;
    }
    if (device_configuration->rd60xx) {
        rd60xx_configuration_free(device_configuration->rd60xx);
        device_configuration->rd60xx = NULL;
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
    if(device_configuration->limits) {
    cJSON *limits_local_JSON = safety_limits_convertToJSON(device_configuration->limits);
    if(limits_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "limits", limits_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // device_configuration->cell_temperature
    if(device_configuration->cell_temperature) {
    cJSON *cell_temperature_local_JSON = cell_temperature_guard_convertToJSON(device_configuration->cell_temperature);
    if(cell_temperature_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "cell_temperature", cell_temperature_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // device_configuration->interlock
    if(device_configuration->interlock) {
    cJSON *interlock_local_JSON = interlock_configuration_convertToJSON(device_configuration->interlock);
    if(interlock_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "interlock", interlock_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // device_configuration->ina228
    if(device_configuration->ina228) {
    cJSON *ina228_local_JSON = ina228_configuration_convertToJSON(device_configuration->ina228);
    if(ina228_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "ina228", ina228_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // device_configuration->yocto_watt
    if(device_configuration->yocto_watt) {
    cJSON *yocto_watt_local_JSON = yocto_watt_configuration_convertToJSON(device_configuration->yocto_watt);
    if(yocto_watt_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "yocto_watt", yocto_watt_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // device_configuration->sensor_max_age_ms
    if(device_configuration->sensor_max_age_ms) {
    if(cJSON_AddNumberToObject(item, "sensor_max_age_ms", device_configuration->sensor_max_age_ms) == NULL) {
    goto fail; //Numeric
    }
    }


    // device_configuration->rd60xx
    if(device_configuration->rd60xx) {
    cJSON *rd60xx_local_JSON = rd60xx_configuration_convertToJSON(device_configuration->rd60xx);
    if(rd60xx_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "rd60xx", rd60xx_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
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

    // define the local variable for device_configuration->cell_temperature
    cell_temperature_guard_t *cell_temperature_local_nonprim = NULL;

    // define the local variable for device_configuration->interlock
    interlock_configuration_t *interlock_local_nonprim = NULL;

    // define the local variable for device_configuration->ina228
    ina228_configuration_t *ina228_local_nonprim = NULL;

    // define the local variable for device_configuration->yocto_watt
    yocto_watt_configuration_t *yocto_watt_local_nonprim = NULL;

    // define the local variable for device_configuration->rd60xx
    rd60xx_configuration_t *rd60xx_local_nonprim = NULL;

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
    if (limits) { 
    limits_local_nonprim = safety_limits_parseFromJSON(limits); //nonprimitive
    }

    // device_configuration->cell_temperature
    cJSON *cell_temperature = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "cell_temperature");
    if (cJSON_IsNull(cell_temperature)) {
        cell_temperature = NULL;
    }
    if (cell_temperature) { 
    cell_temperature_local_nonprim = cell_temperature_guard_parseFromJSON(cell_temperature); //nonprimitive
    }

    // device_configuration->interlock
    cJSON *interlock = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "interlock");
    if (cJSON_IsNull(interlock)) {
        interlock = NULL;
    }
    if (interlock) { 
    interlock_local_nonprim = interlock_configuration_parseFromJSON(interlock); //nonprimitive
    }

    // device_configuration->ina228
    cJSON *ina228 = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "ina228");
    if (cJSON_IsNull(ina228)) {
        ina228 = NULL;
    }
    if (ina228) { 
    ina228_local_nonprim = ina228_configuration_parseFromJSON(ina228); //nonprimitive
    }

    // device_configuration->yocto_watt
    cJSON *yocto_watt = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "yocto_watt");
    if (cJSON_IsNull(yocto_watt)) {
        yocto_watt = NULL;
    }
    if (yocto_watt) { 
    yocto_watt_local_nonprim = yocto_watt_configuration_parseFromJSON(yocto_watt); //nonprimitive
    }

    // device_configuration->sensor_max_age_ms
    cJSON *sensor_max_age_ms = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "sensor_max_age_ms");
    if (cJSON_IsNull(sensor_max_age_ms)) {
        sensor_max_age_ms = NULL;
    }
    if (sensor_max_age_ms) { 
    if(!cJSON_IsNumber(sensor_max_age_ms))
    {
    goto end; //Numeric
    }
    }

    // device_configuration->rd60xx
    cJSON *rd60xx = cJSON_GetObjectItemCaseSensitive(device_configurationJSON, "rd60xx");
    if (cJSON_IsNull(rd60xx)) {
        rd60xx = NULL;
    }
    if (rd60xx) { 
    rd60xx_local_nonprim = rd60xx_configuration_parseFromJSON(rd60xx); //nonprimitive
    }


    device_configuration_local_var = device_configuration_create_internal (
        strdup(id->valuestring),
        strdup(driver->valuestring),
        strdup(transport->valuestring),
        strdup(endpoint->valuestring),
        pins ? pinsList : NULL,
        limits ? limits_local_nonprim : NULL,
        cell_temperature ? cell_temperature_local_nonprim : NULL,
        interlock ? interlock_local_nonprim : NULL,
        ina228 ? ina228_local_nonprim : NULL,
        yocto_watt ? yocto_watt_local_nonprim : NULL,
        sensor_max_age_ms ? sensor_max_age_ms->valuedouble : 0,
        rd60xx ? rd60xx_local_nonprim : NULL
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
    if (cell_temperature_local_nonprim) {
        cell_temperature_guard_free(cell_temperature_local_nonprim);
        cell_temperature_local_nonprim = NULL;
    }
    if (interlock_local_nonprim) {
        interlock_configuration_free(interlock_local_nonprim);
        interlock_local_nonprim = NULL;
    }
    if (ina228_local_nonprim) {
        ina228_configuration_free(ina228_local_nonprim);
        ina228_local_nonprim = NULL;
    }
    if (yocto_watt_local_nonprim) {
        yocto_watt_configuration_free(yocto_watt_local_nonprim);
        yocto_watt_local_nonprim = NULL;
    }
    if (rd60xx_local_nonprim) {
        rd60xx_configuration_free(rd60xx_local_nonprim);
        rd60xx_local_nonprim = NULL;
    }
    return NULL;

}
