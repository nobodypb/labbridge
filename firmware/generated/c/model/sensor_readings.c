#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sensor_readings.h"



static sensor_readings_t *sensor_readings_create_internal(
    char *device_id,
    char *boot_id,
    list_t *readings
    ) {
    sensor_readings_t *sensor_readings_local_var = malloc(sizeof(sensor_readings_t));
    if (!sensor_readings_local_var) {
        return NULL;
    }
    sensor_readings_local_var->device_id = device_id;
    sensor_readings_local_var->boot_id = boot_id;
    sensor_readings_local_var->readings = readings;

    sensor_readings_local_var->_library_owned = 1;
    return sensor_readings_local_var;
}

__attribute__((deprecated)) sensor_readings_t *sensor_readings_create(
    char *device_id,
    char *boot_id,
    list_t *readings
    ) {
    return sensor_readings_create_internal (
        device_id,
        boot_id,
        readings
        );
}

void sensor_readings_free(sensor_readings_t *sensor_readings) {
    if(NULL == sensor_readings){
        return ;
    }
    if(sensor_readings->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "sensor_readings_free");
        return ;
    }
    listEntry_t *listEntry;
    if (sensor_readings->device_id) {
        free(sensor_readings->device_id);
        sensor_readings->device_id = NULL;
    }
    if (sensor_readings->boot_id) {
        free(sensor_readings->boot_id);
        sensor_readings->boot_id = NULL;
    }
    if (sensor_readings->readings) {
        list_ForEach(listEntry, sensor_readings->readings) {
            sensor_reading_free(listEntry->data);
        }
        list_freeList(sensor_readings->readings);
        sensor_readings->readings = NULL;
    }
    free(sensor_readings);
}

cJSON *sensor_readings_convertToJSON(sensor_readings_t *sensor_readings) {
    cJSON *item = cJSON_CreateObject();

    // sensor_readings->device_id
    if (!sensor_readings->device_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "device_id", sensor_readings->device_id) == NULL) {
    goto fail; //String
    }


    // sensor_readings->boot_id
    if (!sensor_readings->boot_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "boot_id", sensor_readings->boot_id) == NULL) {
    goto fail; //String
    }


    // sensor_readings->readings
    if (!sensor_readings->readings) {
        goto fail;
    }
    cJSON *readings = cJSON_AddArrayToObject(item, "readings");
    if(readings == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *readingsListEntry;
    if (sensor_readings->readings) {
    list_ForEach(readingsListEntry, sensor_readings->readings) {
    cJSON *itemLocal = sensor_reading_convertToJSON(readingsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(readings, itemLocal);
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

sensor_readings_t *sensor_readings_parseFromJSON(cJSON *sensor_readingsJSON){

    sensor_readings_t *sensor_readings_local_var = NULL;

    // define the local list for sensor_readings->readings
    list_t *readingsList = NULL;

    // sensor_readings->device_id
    cJSON *device_id = cJSON_GetObjectItemCaseSensitive(sensor_readingsJSON, "device_id");
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

    // sensor_readings->boot_id
    cJSON *boot_id = cJSON_GetObjectItemCaseSensitive(sensor_readingsJSON, "boot_id");
    if (cJSON_IsNull(boot_id)) {
        boot_id = NULL;
    }
    if (!boot_id) {
        goto end;
    }

    
    if(!cJSON_IsString(boot_id))
    {
    goto end; //String
    }

    // sensor_readings->readings
    cJSON *readings = cJSON_GetObjectItemCaseSensitive(sensor_readingsJSON, "readings");
    if (cJSON_IsNull(readings)) {
        readings = NULL;
    }
    if (!readings) {
        goto end;
    }

    
    cJSON *readings_local_nonprimitive = NULL;
    if(!cJSON_IsArray(readings)){
        goto end; //nonprimitive container
    }

    readingsList = list_createList();

    cJSON_ArrayForEach(readings_local_nonprimitive,readings )
    {
        if(!cJSON_IsObject(readings_local_nonprimitive)){
            goto end;
        }
        sensor_reading_t *readingsItem = sensor_reading_parseFromJSON(readings_local_nonprimitive);

        list_addElement(readingsList, readingsItem);
    }


    sensor_readings_local_var = sensor_readings_create_internal (
        strdup(device_id->valuestring),
        strdup(boot_id->valuestring),
        readingsList
        );

    return sensor_readings_local_var;
end:
    if (readingsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, readingsList) {
            sensor_reading_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(readingsList);
        readingsList = NULL;
    }
    return NULL;

}
