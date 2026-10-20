#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lease_request.h"



static lease_request_t *lease_request_create_internal(
    list_t *device_ids,
    long ttl_ms
    ) {
    lease_request_t *lease_request_local_var = malloc(sizeof(lease_request_t));
    if (!lease_request_local_var) {
        return NULL;
    }
    lease_request_local_var->device_ids = device_ids;
    lease_request_local_var->ttl_ms = ttl_ms;

    lease_request_local_var->_library_owned = 1;
    return lease_request_local_var;
}

__attribute__((deprecated)) lease_request_t *lease_request_create(
    list_t *device_ids,
    long ttl_ms
    ) {
    return lease_request_create_internal (
        device_ids,
        ttl_ms
        );
}

void lease_request_free(lease_request_t *lease_request) {
    if(NULL == lease_request){
        return ;
    }
    if(lease_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "lease_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (lease_request->device_ids) {
        list_ForEach(listEntry, lease_request->device_ids) {
            free(listEntry->data);
        }
        list_freeList(lease_request->device_ids);
        lease_request->device_ids = NULL;
    }
    free(lease_request);
}

cJSON *lease_request_convertToJSON(lease_request_t *lease_request) {
    cJSON *item = cJSON_CreateObject();

    // lease_request->device_ids
    if (!lease_request->device_ids) {
        goto fail;
    }
    cJSON *device_ids = cJSON_AddArrayToObject(item, "device_ids");
    if(device_ids == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *device_idsListEntry;
    list_ForEach(device_idsListEntry, lease_request->device_ids) {
    if(cJSON_AddStringToObject(device_ids, "", device_idsListEntry->data) == NULL)
    {
        goto fail;
    }
    }


    // lease_request->ttl_ms
    if(lease_request->ttl_ms) {
    if(cJSON_AddNumberToObject(item, "ttl_ms", lease_request->ttl_ms) == NULL) {
    goto fail; //Numeric
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

lease_request_t *lease_request_parseFromJSON(cJSON *lease_requestJSON){

    lease_request_t *lease_request_local_var = NULL;

    // define the local list for lease_request->device_ids
    list_t *device_idsList = NULL;

    // lease_request->device_ids
    cJSON *device_ids = cJSON_GetObjectItemCaseSensitive(lease_requestJSON, "device_ids");
    if (cJSON_IsNull(device_ids)) {
        device_ids = NULL;
    }
    if (!device_ids) {
        goto end;
    }

    
    cJSON *device_ids_local = NULL;
    if(!cJSON_IsArray(device_ids)) {
        goto end;//primitive container
    }
    device_idsList = list_createList();

    cJSON_ArrayForEach(device_ids_local, device_ids)
    {
        if(!cJSON_IsString(device_ids_local))
        {
            goto end;
        }
        list_addElement(device_idsList , strdup(device_ids_local->valuestring));
    }

    // lease_request->ttl_ms
    cJSON *ttl_ms = cJSON_GetObjectItemCaseSensitive(lease_requestJSON, "ttl_ms");
    if (cJSON_IsNull(ttl_ms)) {
        ttl_ms = NULL;
    }
    if (ttl_ms) { 
    if(!cJSON_IsNumber(ttl_ms))
    {
    goto end; //Numeric
    }
    }


    lease_request_local_var = lease_request_create_internal (
        device_idsList,
        ttl_ms ? ttl_ms->valuedouble : 0
        );

    return lease_request_local_var;
end:
    if (device_idsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, device_idsList) {
            free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(device_idsList);
        device_idsList = NULL;
    }
    return NULL;

}
