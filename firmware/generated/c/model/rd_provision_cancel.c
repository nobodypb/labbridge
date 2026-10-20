#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rd_provision_cancel.h"



static rd_provision_cancel_t *rd_provision_cancel_create_internal(
    char *session_id
    ) {
    rd_provision_cancel_t *rd_provision_cancel_local_var = malloc(sizeof(rd_provision_cancel_t));
    if (!rd_provision_cancel_local_var) {
        return NULL;
    }
    rd_provision_cancel_local_var->session_id = session_id;

    rd_provision_cancel_local_var->_library_owned = 1;
    return rd_provision_cancel_local_var;
}

__attribute__((deprecated)) rd_provision_cancel_t *rd_provision_cancel_create(
    char *session_id
    ) {
    return rd_provision_cancel_create_internal (
        session_id
        );
}

void rd_provision_cancel_free(rd_provision_cancel_t *rd_provision_cancel) {
    if(NULL == rd_provision_cancel){
        return ;
    }
    if(rd_provision_cancel->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "rd_provision_cancel_free");
        return ;
    }
    listEntry_t *listEntry;
    if (rd_provision_cancel->session_id) {
        free(rd_provision_cancel->session_id);
        rd_provision_cancel->session_id = NULL;
    }
    free(rd_provision_cancel);
}

cJSON *rd_provision_cancel_convertToJSON(rd_provision_cancel_t *rd_provision_cancel) {
    cJSON *item = cJSON_CreateObject();

    // rd_provision_cancel->session_id
    if (!rd_provision_cancel->session_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "session_id", rd_provision_cancel->session_id) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

rd_provision_cancel_t *rd_provision_cancel_parseFromJSON(cJSON *rd_provision_cancelJSON){

    rd_provision_cancel_t *rd_provision_cancel_local_var = NULL;

    // rd_provision_cancel->session_id
    cJSON *session_id = cJSON_GetObjectItemCaseSensitive(rd_provision_cancelJSON, "session_id");
    if (cJSON_IsNull(session_id)) {
        session_id = NULL;
    }
    if (!session_id) {
        goto end;
    }

    
    if(!cJSON_IsString(session_id))
    {
    goto end; //String
    }


    rd_provision_cancel_local_var = rd_provision_cancel_create_internal (
        strdup(session_id->valuestring)
        );

    return rd_provision_cancel_local_var;
end:
    return NULL;

}
