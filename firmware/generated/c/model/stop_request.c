#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stop_request.h"



static stop_request_t *stop_request_create_internal(
    char *reason
    ) {
    stop_request_t *stop_request_local_var = malloc(sizeof(stop_request_t));
    if (!stop_request_local_var) {
        return NULL;
    }
    stop_request_local_var->reason = reason;

    stop_request_local_var->_library_owned = 1;
    return stop_request_local_var;
}

__attribute__((deprecated)) stop_request_t *stop_request_create(
    char *reason
    ) {
    return stop_request_create_internal (
        reason
        );
}

void stop_request_free(stop_request_t *stop_request) {
    if(NULL == stop_request){
        return ;
    }
    if(stop_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "stop_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (stop_request->reason) {
        free(stop_request->reason);
        stop_request->reason = NULL;
    }
    free(stop_request);
}

cJSON *stop_request_convertToJSON(stop_request_t *stop_request) {
    cJSON *item = cJSON_CreateObject();

    // stop_request->reason
    if(stop_request->reason) {
    if(cJSON_AddStringToObject(item, "reason", stop_request->reason) == NULL) {
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

stop_request_t *stop_request_parseFromJSON(cJSON *stop_requestJSON){

    stop_request_t *stop_request_local_var = NULL;

    // stop_request->reason
    cJSON *reason = cJSON_GetObjectItemCaseSensitive(stop_requestJSON, "reason");
    if (cJSON_IsNull(reason)) {
        reason = NULL;
    }
    if (reason) { 
    if(!cJSON_IsString(reason) && !cJSON_IsNull(reason))
    {
    goto end; //String
    }
    }


    stop_request_local_var = stop_request_create_internal (
        reason && !cJSON_IsNull(reason) ? strdup(reason->valuestring) : NULL
        );

    return stop_request_local_var;
end:
    return NULL;

}
