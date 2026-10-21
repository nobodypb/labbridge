#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stream_auth.h"


char* stream_auth_type_ToString(labbridge_api_stream_auth_TYPE_e type) {
    char* typeArray[] =  { "NULL", "auth" };
    return typeArray[type];
}

labbridge_api_stream_auth_TYPE_e stream_auth_type_FromString(char* type){
    int stringToReturn = 0;
    char *typeArray[] =  { "NULL", "auth" };
    size_t sizeofArray = sizeof(typeArray) / sizeof(typeArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(type, typeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static stream_auth_t *stream_auth_create_internal(
    labbridge_api_stream_auth_TYPE_e type,
    char *token,
    char *csrf_token
    ) {
    stream_auth_t *stream_auth_local_var = malloc(sizeof(stream_auth_t));
    if (!stream_auth_local_var) {
        return NULL;
    }
    stream_auth_local_var->type = type;
    stream_auth_local_var->token = token;
    stream_auth_local_var->csrf_token = csrf_token;

    stream_auth_local_var->_library_owned = 1;
    return stream_auth_local_var;
}

__attribute__((deprecated)) stream_auth_t *stream_auth_create(
    labbridge_api_stream_auth_TYPE_e type,
    char *token,
    char *csrf_token
    ) {
    return stream_auth_create_internal (
        type,
        token,
        csrf_token
        );
}

void stream_auth_free(stream_auth_t *stream_auth) {
    if(NULL == stream_auth){
        return ;
    }
    if(stream_auth->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "stream_auth_free");
        return ;
    }
    listEntry_t *listEntry;
    if (stream_auth->token) {
        free(stream_auth->token);
        stream_auth->token = NULL;
    }
    if (stream_auth->csrf_token) {
        free(stream_auth->csrf_token);
        stream_auth->csrf_token = NULL;
    }
    free(stream_auth);
}

cJSON *stream_auth_convertToJSON(stream_auth_t *stream_auth) {
    cJSON *item = cJSON_CreateObject();

    // stream_auth->type
    if (labbridge_api_stream_auth_TYPE_NULL == stream_auth->type) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "type", stream_auth_type_ToString(stream_auth->type)) == NULL)
    {
    goto fail; //Enum
    }


    // stream_auth->token
    if(stream_auth->token) {
    if(cJSON_AddStringToObject(item, "token", stream_auth->token) == NULL) {
    goto fail; //String
    }
    }


    // stream_auth->csrf_token
    if(stream_auth->csrf_token) {
    if(cJSON_AddStringToObject(item, "csrf_token", stream_auth->csrf_token) == NULL) {
    goto fail; //String
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

stream_auth_t *stream_auth_parseFromJSON(cJSON *stream_authJSON){

    stream_auth_t *stream_auth_local_var = NULL;

    // stream_auth->type
    cJSON *type = cJSON_GetObjectItemCaseSensitive(stream_authJSON, "type");
    if (cJSON_IsNull(type)) {
        type = NULL;
    }
    if (!type) {
        goto end;
    }

    labbridge_api_stream_auth_TYPE_e typeVariable;
    
    if(!cJSON_IsString(type))
    {
    goto end; //Enum
    }
    typeVariable = stream_auth_type_FromString(type->valuestring);

    // stream_auth->token
    cJSON *token = cJSON_GetObjectItemCaseSensitive(stream_authJSON, "token");
    if (cJSON_IsNull(token)) {
        token = NULL;
    }
    if (token) { 
    if(!cJSON_IsString(token) && !cJSON_IsNull(token))
    {
    goto end; //String
    }
    }

    // stream_auth->csrf_token
    cJSON *csrf_token = cJSON_GetObjectItemCaseSensitive(stream_authJSON, "csrf_token");
    if (cJSON_IsNull(csrf_token)) {
        csrf_token = NULL;
    }
    if (csrf_token) { 
    if(!cJSON_IsString(csrf_token) && !cJSON_IsNull(csrf_token))
    {
    goto end; //String
    }
    }


    stream_auth_local_var = stream_auth_create_internal (
        typeVariable,
        token && !cJSON_IsNull(token) ? strdup(token->valuestring) : NULL,
        csrf_token && !cJSON_IsNull(csrf_token) ? strdup(csrf_token->valuestring) : NULL
        );

    return stream_auth_local_var;
end:
    return NULL;

}
