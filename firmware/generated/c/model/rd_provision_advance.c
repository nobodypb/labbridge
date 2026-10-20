#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rd_provision_advance.h"


char* rd_provision_advance_expected_phase_ToString(labbridge_api_rd_provision_advance_EXPECTEDPHASE_e expected_phase) {
    char* expected_phaseArray[] =  { "NULL", "server_address", "wifi" };
    return expected_phaseArray[expected_phase];
}

labbridge_api_rd_provision_advance_EXPECTEDPHASE_e rd_provision_advance_expected_phase_FromString(char* expected_phase){
    int stringToReturn = 0;
    char *expected_phaseArray[] =  { "NULL", "server_address", "wifi" };
    size_t sizeofArray = sizeof(expected_phaseArray) / sizeof(expected_phaseArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(expected_phase, expected_phaseArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static rd_provision_advance_t *rd_provision_advance_create_internal(
    char *session_id,
    labbridge_api_rd_provision_advance_EXPECTEDPHASE_e expected_phase
    ) {
    rd_provision_advance_t *rd_provision_advance_local_var = malloc(sizeof(rd_provision_advance_t));
    if (!rd_provision_advance_local_var) {
        return NULL;
    }
    rd_provision_advance_local_var->session_id = session_id;
    rd_provision_advance_local_var->expected_phase = expected_phase;

    rd_provision_advance_local_var->_library_owned = 1;
    return rd_provision_advance_local_var;
}

__attribute__((deprecated)) rd_provision_advance_t *rd_provision_advance_create(
    char *session_id,
    labbridge_api_rd_provision_advance_EXPECTEDPHASE_e expected_phase
    ) {
    return rd_provision_advance_create_internal (
        session_id,
        expected_phase
        );
}

void rd_provision_advance_free(rd_provision_advance_t *rd_provision_advance) {
    if(NULL == rd_provision_advance){
        return ;
    }
    if(rd_provision_advance->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "rd_provision_advance_free");
        return ;
    }
    listEntry_t *listEntry;
    if (rd_provision_advance->session_id) {
        free(rd_provision_advance->session_id);
        rd_provision_advance->session_id = NULL;
    }
    free(rd_provision_advance);
}

cJSON *rd_provision_advance_convertToJSON(rd_provision_advance_t *rd_provision_advance) {
    cJSON *item = cJSON_CreateObject();

    // rd_provision_advance->session_id
    if (!rd_provision_advance->session_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "session_id", rd_provision_advance->session_id) == NULL) {
    goto fail; //String
    }


    // rd_provision_advance->expected_phase
    if (labbridge_api_rd_provision_advance_EXPECTEDPHASE_NULL == rd_provision_advance->expected_phase) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "expected_phase", rd_provision_advance_expected_phase_ToString(rd_provision_advance->expected_phase)) == NULL)
    {
    goto fail; //Enum
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

rd_provision_advance_t *rd_provision_advance_parseFromJSON(cJSON *rd_provision_advanceJSON){

    rd_provision_advance_t *rd_provision_advance_local_var = NULL;

    // rd_provision_advance->session_id
    cJSON *session_id = cJSON_GetObjectItemCaseSensitive(rd_provision_advanceJSON, "session_id");
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

    // rd_provision_advance->expected_phase
    cJSON *expected_phase = cJSON_GetObjectItemCaseSensitive(rd_provision_advanceJSON, "expected_phase");
    if (cJSON_IsNull(expected_phase)) {
        expected_phase = NULL;
    }
    if (!expected_phase) {
        goto end;
    }

    labbridge_api_rd_provision_advance_EXPECTEDPHASE_e expected_phaseVariable;
    
    if(!cJSON_IsString(expected_phase))
    {
    goto end; //Enum
    }
    expected_phaseVariable = rd_provision_advance_expected_phase_FromString(expected_phase->valuestring);


    rd_provision_advance_local_var = rd_provision_advance_create_internal (
        strdup(session_id->valuestring),
        expected_phaseVariable
        );

    return rd_provision_advance_local_var;
end:
    return NULL;

}
