#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "node_configuration.h"



static node_configuration_t *node_configuration_create_internal(
    long revision,
    char *node_id,
    char *board,
    list_t *devices
    ) {
    node_configuration_t *node_configuration_local_var = malloc(sizeof(node_configuration_t));
    if (!node_configuration_local_var) {
        return NULL;
    }
    node_configuration_local_var->revision = revision;
    node_configuration_local_var->node_id = node_id;
    node_configuration_local_var->board = board;
    node_configuration_local_var->devices = devices;

    node_configuration_local_var->_library_owned = 1;
    return node_configuration_local_var;
}

__attribute__((deprecated)) node_configuration_t *node_configuration_create(
    long revision,
    char *node_id,
    char *board,
    list_t *devices
    ) {
    return node_configuration_create_internal (
        revision,
        node_id,
        board,
        devices
        );
}

void node_configuration_free(node_configuration_t *node_configuration) {
    if(NULL == node_configuration){
        return ;
    }
    if(node_configuration->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "node_configuration_free");
        return ;
    }
    listEntry_t *listEntry;
    if (node_configuration->node_id) {
        free(node_configuration->node_id);
        node_configuration->node_id = NULL;
    }
    if (node_configuration->board) {
        free(node_configuration->board);
        node_configuration->board = NULL;
    }
    if (node_configuration->devices) {
        list_ForEach(listEntry, node_configuration->devices) {
            device_configuration_free(listEntry->data);
        }
        list_freeList(node_configuration->devices);
        node_configuration->devices = NULL;
    }
    free(node_configuration);
}

cJSON *node_configuration_convertToJSON(node_configuration_t *node_configuration) {
    cJSON *item = cJSON_CreateObject();

    // node_configuration->revision
    if (!node_configuration->revision) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "revision", node_configuration->revision) == NULL) {
    goto fail; //Numeric
    }


    // node_configuration->node_id
    if (!node_configuration->node_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "node_id", node_configuration->node_id) == NULL) {
    goto fail; //String
    }


    // node_configuration->board
    if (!node_configuration->board) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "board", node_configuration->board) == NULL) {
    goto fail; //String
    }


    // node_configuration->devices
    if (!node_configuration->devices) {
        goto fail;
    }
    cJSON *devices = cJSON_AddArrayToObject(item, "devices");
    if(devices == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *devicesListEntry;
    if (node_configuration->devices) {
    list_ForEach(devicesListEntry, node_configuration->devices) {
    cJSON *itemLocal = device_configuration_convertToJSON(devicesListEntry->data);
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

node_configuration_t *node_configuration_parseFromJSON(cJSON *node_configurationJSON){

    node_configuration_t *node_configuration_local_var = NULL;

    // define the local list for node_configuration->devices
    list_t *devicesList = NULL;

    // node_configuration->revision
    cJSON *revision = cJSON_GetObjectItemCaseSensitive(node_configurationJSON, "revision");
    if (cJSON_IsNull(revision)) {
        revision = NULL;
    }
    if (!revision) {
        goto end;
    }

    
    if(!cJSON_IsNumber(revision))
    {
    goto end; //Numeric
    }

    // node_configuration->node_id
    cJSON *node_id = cJSON_GetObjectItemCaseSensitive(node_configurationJSON, "node_id");
    if (cJSON_IsNull(node_id)) {
        node_id = NULL;
    }
    if (!node_id) {
        goto end;
    }

    
    if(!cJSON_IsString(node_id))
    {
    goto end; //String
    }

    // node_configuration->board
    cJSON *board = cJSON_GetObjectItemCaseSensitive(node_configurationJSON, "board");
    if (cJSON_IsNull(board)) {
        board = NULL;
    }
    if (!board) {
        goto end;
    }

    
    if(!cJSON_IsString(board))
    {
    goto end; //String
    }

    // node_configuration->devices
    cJSON *devices = cJSON_GetObjectItemCaseSensitive(node_configurationJSON, "devices");
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
        device_configuration_t *devicesItem = device_configuration_parseFromJSON(devices_local_nonprimitive);

        list_addElement(devicesList, devicesItem);
    }


    node_configuration_local_var = node_configuration_create_internal (
        revision->valuedouble,
        strdup(node_id->valuestring),
        strdup(board->valuestring),
        devicesList
        );

    return node_configuration_local_var;
end:
    if (devicesList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, devicesList) {
            device_configuration_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(devicesList);
        devicesList = NULL;
    }
    return NULL;

}
