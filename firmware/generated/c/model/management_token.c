#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "management_token.h"



static management_token_t *management_token_create_internal(
    char *token
    ) {
    management_token_t *management_token_local_var = malloc(sizeof(management_token_t));
    if (!management_token_local_var) {
        return NULL;
    }
    management_token_local_var->token = token;

    management_token_local_var->_library_owned = 1;
    return management_token_local_var;
}

__attribute__((deprecated)) management_token_t *management_token_create(
    char *token
    ) {
    return management_token_create_internal (
        token
        );
}

void management_token_free(management_token_t *management_token) {
    if(NULL == management_token){
        return ;
    }
    if(management_token->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "management_token_free");
        return ;
    }
    listEntry_t *listEntry;
    if (management_token->token) {
        free(management_token->token);
        management_token->token = NULL;
    }
    free(management_token);
}

cJSON *management_token_convertToJSON(management_token_t *management_token) {
    cJSON *item = cJSON_CreateObject();

    // management_token->token
    if (!management_token->token) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "token", management_token->token) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

management_token_t *management_token_parseFromJSON(cJSON *management_tokenJSON){

    management_token_t *management_token_local_var = NULL;

    // management_token->token
    cJSON *token = cJSON_GetObjectItemCaseSensitive(management_tokenJSON, "token");
    if (cJSON_IsNull(token)) {
        token = NULL;
    }
    if (!token) {
        goto end;
    }

    
    if(!cJSON_IsString(token))
    {
    goto end; //String
    }


    management_token_local_var = management_token_create_internal (
        strdup(token->valuestring)
        );

    return management_token_local_var;
end:
    return NULL;

}
