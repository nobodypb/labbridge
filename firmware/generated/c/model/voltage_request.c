#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "voltage_request.h"



static voltage_request_t *voltage_request_create_internal(
    double voltage_v
    ) {
    voltage_request_t *voltage_request_local_var = malloc(sizeof(voltage_request_t));
    if (!voltage_request_local_var) {
        return NULL;
    }
    voltage_request_local_var->voltage_v = voltage_v;

    voltage_request_local_var->_library_owned = 1;
    return voltage_request_local_var;
}

__attribute__((deprecated)) voltage_request_t *voltage_request_create(
    double voltage_v
    ) {
    return voltage_request_create_internal (
        voltage_v
        );
}

void voltage_request_free(voltage_request_t *voltage_request) {
    if(NULL == voltage_request){
        return ;
    }
    if(voltage_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "voltage_request_free");
        return ;
    }
    listEntry_t *listEntry;
    free(voltage_request);
}

cJSON *voltage_request_convertToJSON(voltage_request_t *voltage_request) {
    cJSON *item = cJSON_CreateObject();

    // voltage_request->voltage_v
    if (!voltage_request->voltage_v) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "voltage_v", voltage_request->voltage_v) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

voltage_request_t *voltage_request_parseFromJSON(cJSON *voltage_requestJSON){

    voltage_request_t *voltage_request_local_var = NULL;

    // voltage_request->voltage_v
    cJSON *voltage_v = cJSON_GetObjectItemCaseSensitive(voltage_requestJSON, "voltage_v");
    if (cJSON_IsNull(voltage_v)) {
        voltage_v = NULL;
    }
    if (!voltage_v) {
        goto end;
    }

    
    if(!cJSON_IsNumber(voltage_v))
    {
    goto end; //Numeric
    }


    voltage_request_local_var = voltage_request_create_internal (
        voltage_v->valuedouble
        );

    return voltage_request_local_var;
end:
    return NULL;

}
