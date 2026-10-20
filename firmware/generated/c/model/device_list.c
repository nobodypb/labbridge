#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "device_list.h"



static device_list_t *device_list_create_internal(
    list_t *devices
    ) {
    device_list_t *device_list_local_var = malloc(sizeof(device_list_t));
    if (!device_list_local_var) {
        return NULL;
    }
    device_list_local_var->devices = devices;

    device_list_local_var->_library_owned = 1;
    return device_list_local_var;
}

__attribute__((deprecated)) device_list_t *device_list_create(
    list_t *devices
    ) {
    return device_list_create_internal (
        devices
        );
}

void device_list_free(device_list_t *device_list) {
    if(NULL == device_list){
        return ;
    }
    if(device_list->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "device_list_free");
        return ;
    }
    listEntry_t *listEntry;
    if (device_list->devices) {
        list_ForEach(listEntry, device_list->devices) {
            device_free(listEntry->data);
        }
        list_freeList(device_list->devices);
        device_list->devices = NULL;
    }
    free(device_list);
}

cJSON *device_list_convertToJSON(device_list_t *device_list) {
    cJSON *item = cJSON_CreateObject();

    // device_list->devices
    if (!device_list->devices) {
        goto fail;
    }
    cJSON *devices = cJSON_AddArrayToObject(item, "devices");
    if(devices == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *devicesListEntry;
    if (device_list->devices) {
    list_ForEach(devicesListEntry, device_list->devices) {
    cJSON *itemLocal = device_convertToJSON(devicesListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(devices, itemLocal);
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

device_list_t *device_list_parseFromJSON(cJSON *device_listJSON){

    device_list_t *device_list_local_var = NULL;

    // define the local list for device_list->devices
    list_t *devicesList = NULL;

    // device_list->devices
    cJSON *devices = cJSON_GetObjectItemCaseSensitive(device_listJSON, "devices");
    if (cJSON_IsNull(devices)) {
        devices = NULL;
    }
    if (!devices) {
        goto end;
    }

    
    cJSON *devices_local_nonprimitive = NULL;
    if(!cJSON_IsArray(devices)){
        goto end; //nonprimitive container
    }

    devicesList = list_createList();

    cJSON_ArrayForEach(devices_local_nonprimitive,devices )
    {
        if(!cJSON_IsObject(devices_local_nonprimitive)){
            goto end;
        }
        device_t *devicesItem = device_parseFromJSON(devices_local_nonprimitive);

        list_addElement(devicesList, devicesItem);
    }


    device_list_local_var = device_list_create_internal (
        devicesList
        );

    return device_list_local_var;
end:
    if (devicesList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, devicesList) {
            device_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(devicesList);
        devicesList = NULL;
    }
    return NULL;

}
