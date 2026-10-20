#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rd_provision_start.h"



static rd_provision_start_t *rd_provision_start_create_internal(
    int confirm_outputs_off
    ) {
    rd_provision_start_t *rd_provision_start_local_var = malloc(sizeof(rd_provision_start_t));
    if (!rd_provision_start_local_var) {
        return NULL;
    }
    rd_provision_start_local_var->confirm_outputs_off = confirm_outputs_off;

    rd_provision_start_local_var->_library_owned = 1;
    return rd_provision_start_local_var;
}

__attribute__((deprecated)) rd_provision_start_t *rd_provision_start_create(
    int confirm_outputs_off
    ) {
    return rd_provision_start_create_internal (
        confirm_outputs_off
        );
}

void rd_provision_start_free(rd_provision_start_t *rd_provision_start) {
    if(NULL == rd_provision_start){
        return ;
    }
    if(rd_provision_start->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "rd_provision_start_free");
        return ;
    }
    listEntry_t *listEntry;
    free(rd_provision_start);
}

cJSON *rd_provision_start_convertToJSON(rd_provision_start_t *rd_provision_start) {
    cJSON *item = cJSON_CreateObject();

    // rd_provision_start->confirm_outputs_off
    if (!rd_provision_start->confirm_outputs_off) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "confirm_outputs_off", rd_provision_start->confirm_outputs_off) == NULL) {
    goto fail; //Bool
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

rd_provision_start_t *rd_provision_start_parseFromJSON(cJSON *rd_provision_startJSON){

    rd_provision_start_t *rd_provision_start_local_var = NULL;

    // rd_provision_start->confirm_outputs_off
    cJSON *confirm_outputs_off = cJSON_GetObjectItemCaseSensitive(rd_provision_startJSON, "confirm_outputs_off");
    if (cJSON_IsNull(confirm_outputs_off)) {
        confirm_outputs_off = NULL;
    }
    if (!confirm_outputs_off) {
        goto end;
    }

    
    if(!cJSON_IsBool(confirm_outputs_off))
    {
    goto end; //Bool
    }


    rd_provision_start_local_var = rd_provision_start_create_internal (
        confirm_outputs_off->valueint
        );

    return rd_provision_start_local_var;
end:
    return NULL;

}
