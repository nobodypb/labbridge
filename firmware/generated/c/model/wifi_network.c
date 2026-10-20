#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wifi_network.h"



static wifi_network_t *wifi_network_create_internal(
    char *ssid,
    long rssi_dbm,
    int secured
    ) {
    wifi_network_t *wifi_network_local_var = malloc(sizeof(wifi_network_t));
    if (!wifi_network_local_var) {
        return NULL;
    }
    wifi_network_local_var->ssid = ssid;
    wifi_network_local_var->rssi_dbm = rssi_dbm;
    wifi_network_local_var->secured = secured;

    wifi_network_local_var->_library_owned = 1;
    return wifi_network_local_var;
}

__attribute__((deprecated)) wifi_network_t *wifi_network_create(
    char *ssid,
    long rssi_dbm,
    int secured
    ) {
    return wifi_network_create_internal (
        ssid,
        rssi_dbm,
        secured
        );
}

void wifi_network_free(wifi_network_t *wifi_network) {
    if(NULL == wifi_network){
        return ;
    }
    if(wifi_network->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "wifi_network_free");
        return ;
    }
    listEntry_t *listEntry;
    if (wifi_network->ssid) {
        free(wifi_network->ssid);
        wifi_network->ssid = NULL;
    }
    free(wifi_network);
}

cJSON *wifi_network_convertToJSON(wifi_network_t *wifi_network) {
    cJSON *item = cJSON_CreateObject();

    // wifi_network->ssid
    if (!wifi_network->ssid) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "ssid", wifi_network->ssid) == NULL) {
    goto fail; //String
    }


    // wifi_network->rssi_dbm
    if (!wifi_network->rssi_dbm) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "rssi_dbm", wifi_network->rssi_dbm) == NULL) {
    goto fail; //Numeric
    }


    // wifi_network->secured
    if (!wifi_network->secured) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "secured", wifi_network->secured) == NULL) {
    goto fail; //Bool
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

wifi_network_t *wifi_network_parseFromJSON(cJSON *wifi_networkJSON){

    wifi_network_t *wifi_network_local_var = NULL;

    // wifi_network->ssid
    cJSON *ssid = cJSON_GetObjectItemCaseSensitive(wifi_networkJSON, "ssid");
    if (cJSON_IsNull(ssid)) {
        ssid = NULL;
    }
    if (!ssid) {
        goto end;
    }

    
    if(!cJSON_IsString(ssid))
    {
    goto end; //String
    }

    // wifi_network->rssi_dbm
    cJSON *rssi_dbm = cJSON_GetObjectItemCaseSensitive(wifi_networkJSON, "rssi_dbm");
    if (cJSON_IsNull(rssi_dbm)) {
        rssi_dbm = NULL;
    }
    if (!rssi_dbm) {
        goto end;
    }

    
    if(!cJSON_IsNumber(rssi_dbm))
    {
    goto end; //Numeric
    }

    // wifi_network->secured
    cJSON *secured = cJSON_GetObjectItemCaseSensitive(wifi_networkJSON, "secured");
    if (cJSON_IsNull(secured)) {
        secured = NULL;
    }
    if (!secured) {
        goto end;
    }

    
    if(!cJSON_IsBool(secured))
    {
    goto end; //Bool
    }


    wifi_network_local_var = wifi_network_create_internal (
        strdup(ssid->valuestring),
        rssi_dbm->valuedouble,
        secured->valueint
        );

    return wifi_network_local_var;
end:
    return NULL;

}
