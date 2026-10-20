#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "current_request.h"



static current_request_t *current_request_create_internal(
    double current_a
    ) {
    current_request_t *current_request_local_var = malloc(sizeof(current_request_t));
    if (!current_request_local_var) {
        return NULL;
    }
    current_request_local_var->current_a = current_a;

    current_request_local_var->_library_owned = 1;
    return current_request_local_var;
}

__attribute__((deprecated)) current_request_t *current_request_create(
    double current_a
    ) {
    return current_request_create_internal (
        current_a
        );
}

void current_request_free(current_request_t *current_request) {
    if(NULL == current_request){
        return ;
    }
    if(current_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "current_request_free");
        return ;
    }
    listEntry_t *listEntry;
    free(current_request);
}

cJSON *current_request_convertToJSON(current_request_t *current_request) {
    cJSON *item = cJSON_CreateObject();

    // current_request->current_a
    if (!current_request->current_a) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "current_a", current_request->current_a) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

current_request_t *current_request_parseFromJSON(cJSON *current_requestJSON){

    current_request_t *current_request_local_var = NULL;

    // current_request->current_a
    cJSON *current_a = cJSON_GetObjectItemCaseSensitive(current_requestJSON, "current_a");
    if (cJSON_IsNull(current_a)) {
        current_a = NULL;
    }
    if (!current_a) {
        goto end;
    }

    
    if(!cJSON_IsNumber(current_a))
    {
    goto end; //Numeric
    }


    current_request_local_var = current_request_create_internal (
        current_a->valuedouble
        );

    return current_request_local_var;
end:
    return NULL;

}
