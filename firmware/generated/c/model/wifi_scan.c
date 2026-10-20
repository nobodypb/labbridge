#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wifi_scan.h"


char* wifi_scan_state_ToString(labbridge_api_wifi_scan_STATE_e state) {
    char* stateArray[] =  { "NULL", "scanning", "complete", "failed" };
    return stateArray[state];
}

labbridge_api_wifi_scan_STATE_e wifi_scan_state_FromString(char* state){
    int stringToReturn = 0;
    char *stateArray[] =  { "NULL", "scanning", "complete", "failed" };
    size_t sizeofArray = sizeof(stateArray) / sizeof(stateArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(state, stateArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static wifi_scan_t *wifi_scan_create_internal(
    list_t *networks,
    labbridge_api_wifi_scan_STATE_e state
    ) {
    wifi_scan_t *wifi_scan_local_var = malloc(sizeof(wifi_scan_t));
    if (!wifi_scan_local_var) {
        return NULL;
    }
    wifi_scan_local_var->networks = networks;
    wifi_scan_local_var->state = state;

    wifi_scan_local_var->_library_owned = 1;
    return wifi_scan_local_var;
}

__attribute__((deprecated)) wifi_scan_t *wifi_scan_create(
    list_t *networks,
    labbridge_api_wifi_scan_STATE_e state
    ) {
    return wifi_scan_create_internal (
        networks,
        state
        );
}

void wifi_scan_free(wifi_scan_t *wifi_scan) {
    if(NULL == wifi_scan){
        return ;
    }
    if(wifi_scan->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "wifi_scan_free");
        return ;
    }
    listEntry_t *listEntry;
    if (wifi_scan->networks) {
        list_ForEach(listEntry, wifi_scan->networks) {
            wifi_network_free(listEntry->data);
        }
        list_freeList(wifi_scan->networks);
        wifi_scan->networks = NULL;
    }
    free(wifi_scan);
}

cJSON *wifi_scan_convertToJSON(wifi_scan_t *wifi_scan) {
    cJSON *item = cJSON_CreateObject();

    // wifi_scan->networks
    if (!wifi_scan->networks) {
        goto fail;
    }
    cJSON *networks = cJSON_AddArrayToObject(item, "networks");
    if(networks == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *networksListEntry;
    if (wifi_scan->networks) {
    list_ForEach(networksListEntry, wifi_scan->networks) {
    cJSON *itemLocal = wifi_network_convertToJSON(networksListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(networks, itemLocal);
    }
    }


    // wifi_scan->state
    if (labbridge_api_wifi_scan_STATE_NULL == wifi_scan->state) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "state", wifi_scan_state_ToString(wifi_scan->state)) == NULL)
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

wifi_scan_t *wifi_scan_parseFromJSON(cJSON *wifi_scanJSON){

    wifi_scan_t *wifi_scan_local_var = NULL;

    // define the local list for wifi_scan->networks
    list_t *networksList = NULL;

    // wifi_scan->networks
    cJSON *networks = cJSON_GetObjectItemCaseSensitive(wifi_scanJSON, "networks");
    if (cJSON_IsNull(networks)) {
        networks = NULL;
    }
    if (!networks) {
        goto end;
    }

    
    cJSON *networks_local_nonprimitive = NULL;
    if(!cJSON_IsArray(networks)){
        goto end; //nonprimitive container
    }

    networksList = list_createList();

    cJSON_ArrayForEach(networks_local_nonprimitive,networks )
    {
        if(!cJSON_IsObject(networks_local_nonprimitive)){
            goto end;
        }
        wifi_network_t *networksItem = wifi_network_parseFromJSON(networks_local_nonprimitive);

        list_addElement(networksList, networksItem);
    }

    // wifi_scan->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(wifi_scanJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    if (!state) {
        goto end;
    }

    labbridge_api_wifi_scan_STATE_e stateVariable;
    
    if(!cJSON_IsString(state))
    {
    goto end; //Enum
    }
    stateVariable = wifi_scan_state_FromString(state->valuestring);


    wifi_scan_local_var = wifi_scan_create_internal (
        networksList,
        stateVariable
        );

    return wifi_scan_local_var;
end:
    if (networksList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, networksList) {
            wifi_network_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(networksList);
        networksList = NULL;
    }
    return NULL;

}
