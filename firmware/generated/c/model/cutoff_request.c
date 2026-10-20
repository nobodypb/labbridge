#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cutoff_request.h"



static cutoff_request_t *cutoff_request_create_internal(
    double cutoff_voltage_v
    ) {
    cutoff_request_t *cutoff_request_local_var = malloc(sizeof(cutoff_request_t));
    if (!cutoff_request_local_var) {
        return NULL;
    }
    cutoff_request_local_var->cutoff_voltage_v = cutoff_voltage_v;

    cutoff_request_local_var->_library_owned = 1;
    return cutoff_request_local_var;
}

__attribute__((deprecated)) cutoff_request_t *cutoff_request_create(
    double cutoff_voltage_v
    ) {
    return cutoff_request_create_internal (
        cutoff_voltage_v
        );
}

void cutoff_request_free(cutoff_request_t *cutoff_request) {
    if(NULL == cutoff_request){
        return ;
    }
    if(cutoff_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "cutoff_request_free");
        return ;
    }
    listEntry_t *listEntry;
    free(cutoff_request);
}

cJSON *cutoff_request_convertToJSON(cutoff_request_t *cutoff_request) {
    cJSON *item = cJSON_CreateObject();

    // cutoff_request->cutoff_voltage_v
    if (!cutoff_request->cutoff_voltage_v) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "cutoff_voltage_v", cutoff_request->cutoff_voltage_v) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

cutoff_request_t *cutoff_request_parseFromJSON(cJSON *cutoff_requestJSON){

    cutoff_request_t *cutoff_request_local_var = NULL;

    // cutoff_request->cutoff_voltage_v
    cJSON *cutoff_voltage_v = cJSON_GetObjectItemCaseSensitive(cutoff_requestJSON, "cutoff_voltage_v");
    if (cJSON_IsNull(cutoff_voltage_v)) {
        cutoff_voltage_v = NULL;
    }
    if (!cutoff_voltage_v) {
        goto end;
    }

    
    if(!cJSON_IsNumber(cutoff_voltage_v))
    {
    goto end; //Numeric
    }


    cutoff_request_local_var = cutoff_request_create_internal (
        cutoff_voltage_v->valuedouble
        );

    return cutoff_request_local_var;
end:
    return NULL;

}
