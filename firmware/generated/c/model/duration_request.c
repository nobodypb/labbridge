#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "duration_request.h"



static duration_request_t *duration_request_create_internal(
    int time_limit_s
    ) {
    duration_request_t *duration_request_local_var = malloc(sizeof(duration_request_t));
    if (!duration_request_local_var) {
        return NULL;
    }
    duration_request_local_var->time_limit_s = time_limit_s;

    duration_request_local_var->_library_owned = 1;
    return duration_request_local_var;
}

__attribute__((deprecated)) duration_request_t *duration_request_create(
    int time_limit_s
    ) {
    return duration_request_create_internal (
        time_limit_s
        );
}

void duration_request_free(duration_request_t *duration_request) {
    if(NULL == duration_request){
        return ;
    }
    if(duration_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "duration_request_free");
        return ;
    }
    listEntry_t *listEntry;
    free(duration_request);
}

cJSON *duration_request_convertToJSON(duration_request_t *duration_request) {
    cJSON *item = cJSON_CreateObject();

    // duration_request->time_limit_s
    if (!duration_request->time_limit_s) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "time_limit_s", duration_request->time_limit_s) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

duration_request_t *duration_request_parseFromJSON(cJSON *duration_requestJSON){

    duration_request_t *duration_request_local_var = NULL;

    // duration_request->time_limit_s
    cJSON *time_limit_s = cJSON_GetObjectItemCaseSensitive(duration_requestJSON, "time_limit_s");
    if (cJSON_IsNull(time_limit_s)) {
        time_limit_s = NULL;
    }
    if (!time_limit_s) {
        goto end;
    }

    
    if(!cJSON_IsNumber(time_limit_s))
    {
    goto end; //Numeric
    }


    duration_request_local_var = duration_request_create_internal (
        time_limit_s->valuedouble
        );

    return duration_request_local_var;
end:
    return NULL;

}
