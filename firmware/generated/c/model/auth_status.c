#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "auth_status.h"



static auth_status_t *auth_status_create_internal(
    int password_configured,
    int setup_allowed,
    int authenticated,
    char *csrf_token,
    int expires_in_ms
    ) {
    auth_status_t *auth_status_local_var = malloc(sizeof(auth_status_t));
    if (!auth_status_local_var) {
        return NULL;
    }
    auth_status_local_var->password_configured = password_configured;
    auth_status_local_var->setup_allowed = setup_allowed;
    auth_status_local_var->authenticated = authenticated;
    auth_status_local_var->csrf_token = csrf_token;
    auth_status_local_var->expires_in_ms = expires_in_ms;

    auth_status_local_var->_library_owned = 1;
    return auth_status_local_var;
}

__attribute__((deprecated)) auth_status_t *auth_status_create(
    int password_configured,
    int setup_allowed,
    int authenticated,
    char *csrf_token,
    int expires_in_ms
    ) {
    return auth_status_create_internal (
        password_configured,
        setup_allowed,
        authenticated,
        csrf_token,
        expires_in_ms
        );
}

void auth_status_free(auth_status_t *auth_status) {
    if(NULL == auth_status){
        return ;
    }
    if(auth_status->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "auth_status_free");
        return ;
    }
    listEntry_t *listEntry;
    if (auth_status->csrf_token) {
        free(auth_status->csrf_token);
        auth_status->csrf_token = NULL;
    }
    free(auth_status);
}

cJSON *auth_status_convertToJSON(auth_status_t *auth_status) {
    cJSON *item = cJSON_CreateObject();

    // auth_status->password_configured
    if (!auth_status->password_configured) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "password_configured", auth_status->password_configured) == NULL) {
    goto fail; //Bool
    }


    // auth_status->setup_allowed
    if (!auth_status->setup_allowed) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "setup_allowed", auth_status->setup_allowed) == NULL) {
    goto fail; //Bool
    }


    // auth_status->authenticated
    if (!auth_status->authenticated) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "authenticated", auth_status->authenticated) == NULL) {
    goto fail; //Bool
    }


    // auth_status->csrf_token
    if(auth_status->csrf_token) {
    if(cJSON_AddStringToObject(item, "csrf_token", auth_status->csrf_token) == NULL) {
    goto fail; //String
    }
    }


    // auth_status->expires_in_ms
    if(auth_status->expires_in_ms) {
    if(cJSON_AddNumberToObject(item, "expires_in_ms", auth_status->expires_in_ms) == NULL) {
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

auth_status_t *auth_status_parseFromJSON(cJSON *auth_statusJSON){

    auth_status_t *auth_status_local_var = NULL;

    // auth_status->password_configured
    cJSON *password_configured = cJSON_GetObjectItemCaseSensitive(auth_statusJSON, "password_configured");
    if (cJSON_IsNull(password_configured)) {
        password_configured = NULL;
    }
    if (!password_configured) {
        goto end;
    }

    
    if(!cJSON_IsBool(password_configured))
    {
    goto end; //Bool
    }

    // auth_status->setup_allowed
    cJSON *setup_allowed = cJSON_GetObjectItemCaseSensitive(auth_statusJSON, "setup_allowed");
    if (cJSON_IsNull(setup_allowed)) {
        setup_allowed = NULL;
    }
    if (!setup_allowed) {
        goto end;
    }

    
    if(!cJSON_IsBool(setup_allowed))
    {
    goto end; //Bool
    }

    // auth_status->authenticated
    cJSON *authenticated = cJSON_GetObjectItemCaseSensitive(auth_statusJSON, "authenticated");
    if (cJSON_IsNull(authenticated)) {
        authenticated = NULL;
    }
    if (!authenticated) {
        goto end;
    }

    
    if(!cJSON_IsBool(authenticated))
    {
    goto end; //Bool
    }

    // auth_status->csrf_token
    cJSON *csrf_token = cJSON_GetObjectItemCaseSensitive(auth_statusJSON, "csrf_token");
    if (cJSON_IsNull(csrf_token)) {
        csrf_token = NULL;
    }
    if (csrf_token) { 
    if(!cJSON_IsString(csrf_token) && !cJSON_IsNull(csrf_token))
    {
    goto end; //String
    }
    }

    // auth_status->expires_in_ms
    cJSON *expires_in_ms = cJSON_GetObjectItemCaseSensitive(auth_statusJSON, "expires_in_ms");
    if (cJSON_IsNull(expires_in_ms)) {
        expires_in_ms = NULL;
    }
    if (expires_in_ms) { 
    if(!cJSON_IsNumber(expires_in_ms))
    {
    goto end; //Numeric
    }
    }


    auth_status_local_var = auth_status_create_internal (
        password_configured->valueint,
        setup_allowed->valueint,
        authenticated->valueint,
        csrf_token && !cJSON_IsNull(csrf_token) ? strdup(csrf_token->valuestring) : NULL,
        expires_in_ms ? expires_in_ms->valuedouble : 0
        );

    return auth_status_local_var;
end:
    return NULL;

}
