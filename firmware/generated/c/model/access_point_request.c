#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "access_point_request.h"



static access_point_request_t *access_point_request_create_internal(
    char *psk
    ) {
    access_point_request_t *access_point_request_local_var = malloc(sizeof(access_point_request_t));
    if (!access_point_request_local_var) {
        return NULL;
    }
    access_point_request_local_var->psk = psk;

    access_point_request_local_var->_library_owned = 1;
    return access_point_request_local_var;
}

__attribute__((deprecated)) access_point_request_t *access_point_request_create(
    char *psk
    ) {
    return access_point_request_create_internal (
        psk
        );
}

void access_point_request_free(access_point_request_t *access_point_request) {
    if(NULL == access_point_request){
        return ;
    }
    if(access_point_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "access_point_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (access_point_request->psk) {
        free(access_point_request->psk);
        access_point_request->psk = NULL;
    }
    free(access_point_request);
}

cJSON *access_point_request_convertToJSON(access_point_request_t *access_point_request) {
    cJSON *item = cJSON_CreateObject();

    // access_point_request->psk
    if (!access_point_request->psk) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "psk", access_point_request->psk) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

access_point_request_t *access_point_request_parseFromJSON(cJSON *access_point_requestJSON){

    access_point_request_t *access_point_request_local_var = NULL;

    // access_point_request->psk
    cJSON *psk = cJSON_GetObjectItemCaseSensitive(access_point_requestJSON, "psk");
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


    access_point_request_local_var = access_point_request_create_internal (
        strdup(psk->valuestring)
        );

    return access_point_request_local_var;
end:
    return NULL;

}
