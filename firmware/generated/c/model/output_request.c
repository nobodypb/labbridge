#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "output_request.h"



static output_request_t *output_request_create_internal(
    int enabled,
    int max_duration_ms
    ) {
    output_request_t *output_request_local_var = malloc(sizeof(output_request_t));
    if (!output_request_local_var) {
        return NULL;
    }
    output_request_local_var->enabled = enabled;
    output_request_local_var->max_duration_ms = max_duration_ms;

    output_request_local_var->_library_owned = 1;
    return output_request_local_var;
}

__attribute__((deprecated)) output_request_t *output_request_create(
    int enabled,
    int max_duration_ms
    ) {
    return output_request_create_internal (
        enabled,
        max_duration_ms
        );
}

void output_request_free(output_request_t *output_request) {
    if(NULL == output_request){
        return ;
    }
    if(output_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "output_request_free");
        return ;
    }
    listEntry_t *listEntry;
    free(output_request);
}

cJSON *output_request_convertToJSON(output_request_t *output_request) {
    cJSON *item = cJSON_CreateObject();

    // output_request->enabled
    if (!output_request->enabled) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "enabled", output_request->enabled) == NULL) {
    goto fail; //Bool
    }


    // output_request->max_duration_ms
    if(output_request->max_duration_ms) {
    if(cJSON_AddNumberToObject(item, "max_duration_ms", output_request->max_duration_ms) == NULL) {
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

output_request_t *output_request_parseFromJSON(cJSON *output_requestJSON){

    output_request_t *output_request_local_var = NULL;

    // output_request->enabled
    cJSON *enabled = cJSON_GetObjectItemCaseSensitive(output_requestJSON, "enabled");
    if (cJSON_IsNull(enabled)) {
        enabled = NULL;
    }
    if (!enabled) {
        goto end;
    }

    
    if(!cJSON_IsBool(enabled))
    {
    goto end; //Bool
    }

    // output_request->max_duration_ms
    cJSON *max_duration_ms = cJSON_GetObjectItemCaseSensitive(output_requestJSON, "max_duration_ms");
    if (cJSON_IsNull(max_duration_ms)) {
        max_duration_ms = NULL;
    }
    if (max_duration_ms) { 
    if(!cJSON_IsNumber(max_duration_ms))
    {
    goto end; //Numeric
    }
    }


    output_request_local_var = output_request_create_internal (
        enabled->valueint,
        max_duration_ms ? max_duration_ms->valuedouble : 0
        );

    return output_request_local_var;
end:
    return NULL;

}
