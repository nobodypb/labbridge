#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wifi_request.h"



static wifi_request_t *wifi_request_create_internal(
    char *ssid,
    char *psk
    ) {
    wifi_request_t *wifi_request_local_var = malloc(sizeof(wifi_request_t));
    if (!wifi_request_local_var) {
        return NULL;
    }
    wifi_request_local_var->ssid = ssid;
    wifi_request_local_var->psk = psk;

    wifi_request_local_var->_library_owned = 1;
    return wifi_request_local_var;
}

__attribute__((deprecated)) wifi_request_t *wifi_request_create(
    char *ssid,
    char *psk
    ) {
    return wifi_request_create_internal (
        ssid,
        psk
        );
}

void wifi_request_free(wifi_request_t *wifi_request) {
    if(NULL == wifi_request){
        return ;
    }
    if(wifi_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "wifi_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (wifi_request->ssid) {
        free(wifi_request->ssid);
        wifi_request->ssid = NULL;
    }
    if (wifi_request->psk) {
        free(wifi_request->psk);
        wifi_request->psk = NULL;
    }
    free(wifi_request);
}

cJSON *wifi_request_convertToJSON(wifi_request_t *wifi_request) {
    cJSON *item = cJSON_CreateObject();

    // wifi_request->ssid
    if (!wifi_request->ssid) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "ssid", wifi_request->ssid) == NULL) {
    goto fail; //String
    }


    // wifi_request->psk
    if (!wifi_request->psk) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "psk", wifi_request->psk) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

wifi_request_t *wifi_request_parseFromJSON(cJSON *wifi_requestJSON){

    wifi_request_t *wifi_request_local_var = NULL;

    // wifi_request->ssid
    cJSON *ssid = cJSON_GetObjectItemCaseSensitive(wifi_requestJSON, "ssid");
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

    // wifi_request->psk
    cJSON *psk = cJSON_GetObjectItemCaseSensitive(wifi_requestJSON, "psk");
    if (cJSON_IsNull(psk)) {
        psk = NULL;
    }
    if (!psk) {
        goto end;
    }

    
    if(!cJSON_IsString(psk))
    {
    goto end; //String
    }


    wifi_request_local_var = wifi_request_create_internal (
        strdup(ssid->valuestring),
        strdup(psk->valuestring)
        );

    return wifi_request_local_var;
end:
    return NULL;

}
