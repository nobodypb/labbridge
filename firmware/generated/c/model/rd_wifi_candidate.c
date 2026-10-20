#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rd_wifi_candidate.h"



static rd_wifi_candidate_t *rd_wifi_candidate_create_internal(
    char *serial,
    char *model,
    long model_id,
    long firmware_version,
    int connected,
    int configured
    ) {
    rd_wifi_candidate_t *rd_wifi_candidate_local_var = malloc(sizeof(rd_wifi_candidate_t));
    if (!rd_wifi_candidate_local_var) {
        return NULL;
    }
    rd_wifi_candidate_local_var->serial = serial;
    rd_wifi_candidate_local_var->model = model;
    rd_wifi_candidate_local_var->model_id = model_id;
    rd_wifi_candidate_local_var->firmware_version = firmware_version;
    rd_wifi_candidate_local_var->connected = connected;
    rd_wifi_candidate_local_var->configured = configured;

    rd_wifi_candidate_local_var->_library_owned = 1;
    return rd_wifi_candidate_local_var;
}

__attribute__((deprecated)) rd_wifi_candidate_t *rd_wifi_candidate_create(
    char *serial,
    char *model,
    long model_id,
    long firmware_version,
    int connected,
    int configured
    ) {
    return rd_wifi_candidate_create_internal (
        serial,
        model,
        model_id,
        firmware_version,
        connected,
        configured
        );
}

void rd_wifi_candidate_free(rd_wifi_candidate_t *rd_wifi_candidate) {
    if(NULL == rd_wifi_candidate){
        return ;
    }
    if(rd_wifi_candidate->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "rd_wifi_candidate_free");
        return ;
    }
    listEntry_t *listEntry;
    if (rd_wifi_candidate->serial) {
        free(rd_wifi_candidate->serial);
        rd_wifi_candidate->serial = NULL;
    }
    if (rd_wifi_candidate->model) {
        free(rd_wifi_candidate->model);
        rd_wifi_candidate->model = NULL;
    }
    free(rd_wifi_candidate);
}

cJSON *rd_wifi_candidate_convertToJSON(rd_wifi_candidate_t *rd_wifi_candidate) {
    cJSON *item = cJSON_CreateObject();

    // rd_wifi_candidate->serial
    if (!rd_wifi_candidate->serial) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "serial", rd_wifi_candidate->serial) == NULL) {
    goto fail; //String
    }


    // rd_wifi_candidate->model
    if (!rd_wifi_candidate->model) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "model", rd_wifi_candidate->model) == NULL) {
    goto fail; //String
    }


    // rd_wifi_candidate->model_id
    if (!rd_wifi_candidate->model_id) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "model_id", rd_wifi_candidate->model_id) == NULL) {
    goto fail; //Numeric
    }


    // rd_wifi_candidate->firmware_version
    if (!rd_wifi_candidate->firmware_version) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "firmware_version", rd_wifi_candidate->firmware_version) == NULL) {
    goto fail; //Numeric
    }


    // rd_wifi_candidate->connected
    if (!rd_wifi_candidate->connected) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "connected", rd_wifi_candidate->connected) == NULL) {
    goto fail; //Bool
    }


    // rd_wifi_candidate->configured
    if (!rd_wifi_candidate->configured) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "configured", rd_wifi_candidate->configured) == NULL) {
    goto fail; //Bool
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

rd_wifi_candidate_t *rd_wifi_candidate_parseFromJSON(cJSON *rd_wifi_candidateJSON){

    rd_wifi_candidate_t *rd_wifi_candidate_local_var = NULL;

    // rd_wifi_candidate->serial
    cJSON *serial = cJSON_GetObjectItemCaseSensitive(rd_wifi_candidateJSON, "serial");
    if (cJSON_IsNull(serial)) {
        serial = NULL;
    }
    if (!serial) {
        goto end;
    }

    
    if(!cJSON_IsString(serial))
    {
    goto end; //String
    }

    // rd_wifi_candidate->model
    cJSON *model = cJSON_GetObjectItemCaseSensitive(rd_wifi_candidateJSON, "model");
    if (cJSON_IsNull(model)) {
        model = NULL;
    }
    if (!model) {
        goto end;
    }

    
    if(!cJSON_IsString(model))
    {
    goto end; //String
    }

    // rd_wifi_candidate->model_id
    cJSON *model_id = cJSON_GetObjectItemCaseSensitive(rd_wifi_candidateJSON, "model_id");
    if (cJSON_IsNull(model_id)) {
        model_id = NULL;
    }
    if (!model_id) {
        goto end;
    }

    
    if(!cJSON_IsNumber(model_id))
    {
    goto end; //Numeric
    }

    // rd_wifi_candidate->firmware_version
    cJSON *firmware_version = cJSON_GetObjectItemCaseSensitive(rd_wifi_candidateJSON, "firmware_version");
    if (cJSON_IsNull(firmware_version)) {
        firmware_version = NULL;
    }
    if (!firmware_version) {
        goto end;
    }

    
    if(!cJSON_IsNumber(firmware_version))
    {
    goto end; //Numeric
    }

    // rd_wifi_candidate->connected
    cJSON *connected = cJSON_GetObjectItemCaseSensitive(rd_wifi_candidateJSON, "connected");
    if (cJSON_IsNull(connected)) {
        connected = NULL;
    }
    if (!connected) {
        goto end;
    }

    
    if(!cJSON_IsBool(connected))
    {
    goto end; //Bool
    }

    // rd_wifi_candidate->configured
    cJSON *configured = cJSON_GetObjectItemCaseSensitive(rd_wifi_candidateJSON, "configured");
    if (cJSON_IsNull(configured)) {
        configured = NULL;
    }
    if (!configured) {
        goto end;
    }

    
    if(!cJSON_IsBool(configured))
    {
    goto end; //Bool
    }


    rd_wifi_candidate_local_var = rd_wifi_candidate_create_internal (
        strdup(serial->valuestring),
        strdup(model->valuestring),
        model_id->valuedouble,
        firmware_version->valuedouble,
        connected->valueint,
        configured->valueint
        );

    return rd_wifi_candidate_local_var;
end:
    return NULL;

}
