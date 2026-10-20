#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wifi_state.h"


char* wifi_state_state_ToString(labbridge_api_wifi_state_STATE_e state) {
    char* stateArray[] =  { "NULL", "setup", "connecting", "connected", "failed" };
    return stateArray[state];
}

labbridge_api_wifi_state_STATE_e wifi_state_state_FromString(char* state){
    int stringToReturn = 0;
    char *stateArray[] =  { "NULL", "setup", "connecting", "connected", "failed" };
    size_t sizeofArray = sizeof(stateArray) / sizeof(stateArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(state, stateArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static wifi_state_t *wifi_state_create_internal(
    labbridge_api_wifi_state_STATE_e state,
    char *hostname,
    char *address,
    int ap_secured
    ) {
    wifi_state_t *wifi_state_local_var = malloc(sizeof(wifi_state_t));
    if (!wifi_state_local_var) {
        return NULL;
    }
    wifi_state_local_var->state = state;
    wifi_state_local_var->hostname = hostname;
    wifi_state_local_var->address = address;
    wifi_state_local_var->ap_secured = ap_secured;

    wifi_state_local_var->_library_owned = 1;
    return wifi_state_local_var;
}

__attribute__((deprecated)) wifi_state_t *wifi_state_create(
    labbridge_api_wifi_state_STATE_e state,
    char *hostname,
    char *address,
    int ap_secured
    ) {
    return wifi_state_create_internal (
        state,
        hostname,
        address,
        ap_secured
        );
}

void wifi_state_free(wifi_state_t *wifi_state) {
    if(NULL == wifi_state){
        return ;
    }
    if(wifi_state->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "wifi_state_free");
        return ;
    }
    listEntry_t *listEntry;
    if (wifi_state->hostname) {
        free(wifi_state->hostname);
        wifi_state->hostname = NULL;
    }
    if (wifi_state->address) {
        free(wifi_state->address);
        wifi_state->address = NULL;
    }
    free(wifi_state);
}

cJSON *wifi_state_convertToJSON(wifi_state_t *wifi_state) {
    cJSON *item = cJSON_CreateObject();

    // wifi_state->state
    if (labbridge_api_wifi_state_STATE_NULL == wifi_state->state) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "state", wifi_state_state_ToString(wifi_state->state)) == NULL)
    {
    goto fail; //Enum
    }


    // wifi_state->hostname
    if (!wifi_state->hostname) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "hostname", wifi_state->hostname) == NULL) {
    goto fail; //String
    }


    // wifi_state->address
    if(wifi_state->address) {
    if(cJSON_AddStringToObject(item, "address", wifi_state->address) == NULL) {
    goto fail; //String
    }
    }


    // wifi_state->ap_secured
    if(wifi_state->ap_secured) {
    if(cJSON_AddBoolToObject(item, "ap_secured", wifi_state->ap_secured) == NULL) {
    goto fail; //Bool
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

wifi_state_t *wifi_state_parseFromJSON(cJSON *wifi_stateJSON){

    wifi_state_t *wifi_state_local_var = NULL;

    // wifi_state->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(wifi_stateJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    if (!state) {
        goto end;
    }

    labbridge_api_wifi_state_STATE_e stateVariable;
    
    if(!cJSON_IsString(state))
    {
    goto end; //Enum
    }
    stateVariable = wifi_state_state_FromString(state->valuestring);

    // wifi_state->hostname
    cJSON *hostname = cJSON_GetObjectItemCaseSensitive(wifi_stateJSON, "hostname");
    if (cJSON_IsNull(hostname)) {
        hostname = NULL;
    }
    if (!hostname) {
        goto end;
    }

    
    if(!cJSON_IsString(hostname))
    {
    goto end; //String
    }

    // wifi_state->address
    cJSON *address = cJSON_GetObjectItemCaseSensitive(wifi_stateJSON, "address");
    if (cJSON_IsNull(address)) {
        address = NULL;
    }
    if (address) { 
    if(!cJSON_IsString(address) && !cJSON_IsNull(address))
    {
    goto end; //String
    }
    }

    // wifi_state->ap_secured
    cJSON *ap_secured = cJSON_GetObjectItemCaseSensitive(wifi_stateJSON, "ap_secured");
    if (cJSON_IsNull(ap_secured)) {
        ap_secured = NULL;
    }
    if (ap_secured) { 
    if(!cJSON_IsBool(ap_secured))
    {
    goto end; //Bool
    }
    }


    wifi_state_local_var = wifi_state_create_internal (
        stateVariable,
        strdup(hostname->valuestring),
        address && !cJSON_IsNull(address) ? strdup(address->valuestring) : NULL,
        ap_secured ? ap_secured->valueint : 0
        );

    return wifi_state_local_var;
end:
    return NULL;

}
