#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dl24m_mode_request.h"


char* dl24m_mode_request_mode_ToString(labbridge_api_dl24m_mode_request_MODE_e mode) {
    char* modeArray[] =  { "NULL", "cc", "cv", "cr", "cp" };
    return modeArray[mode];
}

labbridge_api_dl24m_mode_request_MODE_e dl24m_mode_request_mode_FromString(char* mode){
    int stringToReturn = 0;
    char *modeArray[] =  { "NULL", "cc", "cv", "cr", "cp" };
    size_t sizeofArray = sizeof(modeArray) / sizeof(modeArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(mode, modeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static dl24m_mode_request_t *dl24m_mode_request_create_internal(
    labbridge_api_dl24m_mode_request_MODE_e mode
    ) {
    dl24m_mode_request_t *dl24m_mode_request_local_var = malloc(sizeof(dl24m_mode_request_t));
    if (!dl24m_mode_request_local_var) {
        return NULL;
    }
    dl24m_mode_request_local_var->mode = mode;

    dl24m_mode_request_local_var->_library_owned = 1;
    return dl24m_mode_request_local_var;
}

__attribute__((deprecated)) dl24m_mode_request_t *dl24m_mode_request_create(
    labbridge_api_dl24m_mode_request_MODE_e mode
    ) {
    return dl24m_mode_request_create_internal (
        mode
        );
}

void dl24m_mode_request_free(dl24m_mode_request_t *dl24m_mode_request) {
    if(NULL == dl24m_mode_request){
        return ;
    }
    if(dl24m_mode_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "dl24m_mode_request_free");
        return ;
    }
    listEntry_t *listEntry;
    free(dl24m_mode_request);
}

cJSON *dl24m_mode_request_convertToJSON(dl24m_mode_request_t *dl24m_mode_request) {
    cJSON *item = cJSON_CreateObject();

    // dl24m_mode_request->mode
    if (labbridge_api_dl24m_mode_request_MODE_NULL == dl24m_mode_request->mode) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "mode", dl24m_mode_request_mode_ToString(dl24m_mode_request->mode)) == NULL)
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

dl24m_mode_request_t *dl24m_mode_request_parseFromJSON(cJSON *dl24m_mode_requestJSON){

    dl24m_mode_request_t *dl24m_mode_request_local_var = NULL;

    // dl24m_mode_request->mode
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(dl24m_mode_requestJSON, "mode");
    if (cJSON_IsNull(mode)) {
        mode = NULL;
    }
    if (!mode) {
        goto end;
    }

    labbridge_api_dl24m_mode_request_MODE_e modeVariable;
    
    if(!cJSON_IsString(mode))
    {
    goto end; //Enum
    }
    modeVariable = dl24m_mode_request_mode_FromString(mode->valuestring);


    dl24m_mode_request_local_var = dl24m_mode_request_create_internal (
        modeVariable
        );

    return dl24m_mode_request_local_var;
end:
    return NULL;

}
