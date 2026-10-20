#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wifi_scan.h"



static wifi_scan_t *wifi_scan_create_internal(
    list_t *networks
    ) {
    wifi_scan_t *wifi_scan_local_var = malloc(sizeof(wifi_scan_t));
    if (!wifi_scan_local_var) {
        return NULL;
    }
    wifi_scan_local_var->networks = networks;

    wifi_scan_local_var->_library_owned = 1;
    return wifi_scan_local_var;
}

__attribute__((deprecated)) wifi_scan_t *wifi_scan_create(
    list_t *networks
    ) {
    return wifi_scan_create_internal (
        networks
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


    wifi_scan_local_var = wifi_scan_create_internal (
        networksList
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
