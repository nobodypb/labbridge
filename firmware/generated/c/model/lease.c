#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lease.h"


char* lease_state_ToString(labbridge_api_lease_STATE_e state) {
    char* stateArray[] =  { "NULL", "active", "stopped", "expired" };
    return stateArray[state];
}

labbridge_api_lease_STATE_e lease_state_FromString(char* state){
    int stringToReturn = 0;
    char *stateArray[] =  { "NULL", "active", "stopped", "expired" };
    size_t sizeofArray = sizeof(stateArray) / sizeof(stateArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(state, stateArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static lease_t *lease_create_internal(
    char *id,
    list_t *device_ids,
    long expires_at_ms,
    labbridge_api_lease_STATE_e state
    ) {
    lease_t *lease_local_var = malloc(sizeof(lease_t));
    if (!lease_local_var) {
        return NULL;
    }
    lease_local_var->id = id;
    lease_local_var->device_ids = device_ids;
    lease_local_var->expires_at_ms = expires_at_ms;
    lease_local_var->state = state;

    lease_local_var->_library_owned = 1;
    return lease_local_var;
}

__attribute__((deprecated)) lease_t *lease_create(
    char *id,
    list_t *device_ids,
    long expires_at_ms,
    labbridge_api_lease_STATE_e state
    ) {
    return lease_create_internal (
        id,
        device_ids,
        expires_at_ms,
        state
        );
}

void lease_free(lease_t *lease) {
    if(NULL == lease){
        return ;
    }
    if(lease->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "lease_free");
        return ;
    }
    listEntry_t *listEntry;
    if (lease->id) {
        free(lease->id);
        lease->id = NULL;
    }
    if (lease->device_ids) {
        list_ForEach(listEntry, lease->device_ids) {
            free(listEntry->data);
        }
        list_freeList(lease->device_ids);
        lease->device_ids = NULL;
    }
    free(lease);
}

cJSON *lease_convertToJSON(lease_t *lease) {
    cJSON *item = cJSON_CreateObject();

    // lease->id
    if (!lease->id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "id", lease->id) == NULL) {
    goto fail; //String
    }


    // lease->device_ids
    if (!lease->device_ids) {
        goto fail;
    }
    cJSON *device_ids = cJSON_AddArrayToObject(item, "device_ids");
    if(device_ids == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *device_idsListEntry;
    list_ForEach(device_idsListEntry, lease->device_ids) {
    if(cJSON_AddStringToObject(device_ids, "", device_idsListEntry->data) == NULL)
    {
        goto fail;
    }
    }


    // lease->expires_at_ms
    if (!lease->expires_at_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "expires_at_ms", lease->expires_at_ms) == NULL) {
    goto fail; //Numeric
    }


    // lease->state
    if (labbridge_api_lease_STATE_NULL == lease->state) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "state", lease_state_ToString(lease->state)) == NULL)
    {
    goto fail; //Enum
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

lease_t *lease_parseFromJSON(cJSON *leaseJSON){

    lease_t *lease_local_var = NULL;

    // define the local list for lease->device_ids
    list_t *device_idsList = NULL;

    // lease->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(leaseJSON, "id");
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

    // lease->device_ids
    cJSON *device_ids = cJSON_GetObjectItemCaseSensitive(leaseJSON, "device_ids");
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

    // lease->expires_at_ms
    cJSON *expires_at_ms = cJSON_GetObjectItemCaseSensitive(leaseJSON, "expires_at_ms");
    if (cJSON_IsNull(expires_at_ms)) {
        expires_at_ms = NULL;
    }
    if (!expires_at_ms) {
        goto end;
    }

    
    if(!cJSON_IsNumber(expires_at_ms))
    {
    goto end; //Numeric
    }

    // lease->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(leaseJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    if (!state) {
        goto end;
    }

    labbridge_api_lease_STATE_e stateVariable;
    
    if(!cJSON_IsString(state))
    {
    goto end; //Enum
    }
    stateVariable = lease_state_FromString(state->valuestring);


    lease_local_var = lease_create_internal (
        strdup(id->valuestring),
        device_idsList,
        expires_at_ms->valuedouble,
        stateVariable
        );

    return lease_local_var;
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
