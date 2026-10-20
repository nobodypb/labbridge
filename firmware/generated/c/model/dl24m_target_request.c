#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dl24m_target_request.h"


char* dl24m_target_request_mode_ToString(labbridge_api_dl24m_target_request_MODE_e mode) {
    char* modeArray[] =  { "NULL", "cc", "cv", "cr", "cp" };
    return modeArray[mode];
}

labbridge_api_dl24m_target_request_MODE_e dl24m_target_request_mode_FromString(char* mode){
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
char* dl24m_target_request_unit_ToString(labbridge_api_dl24m_target_request_UNIT_e unit) {
    char* unitArray[] =  { "NULL", "A", "V", "Ohm", "W" };
    return unitArray[unit];
}

labbridge_api_dl24m_target_request_UNIT_e dl24m_target_request_unit_FromString(char* unit){
    int stringToReturn = 0;
    char *unitArray[] =  { "NULL", "A", "V", "Ohm", "W" };
    size_t sizeofArray = sizeof(unitArray) / sizeof(unitArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(unit, unitArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static dl24m_target_request_t *dl24m_target_request_create_internal(
    labbridge_api_dl24m_target_request_MODE_e mode,
    double value,
    labbridge_api_dl24m_target_request_UNIT_e unit
    ) {
    dl24m_target_request_t *dl24m_target_request_local_var = malloc(sizeof(dl24m_target_request_t));
    if (!dl24m_target_request_local_var) {
        return NULL;
    }
    dl24m_target_request_local_var->mode = mode;
    dl24m_target_request_local_var->value = value;
    dl24m_target_request_local_var->unit = unit;

    dl24m_target_request_local_var->_library_owned = 1;
    return dl24m_target_request_local_var;
}

__attribute__((deprecated)) dl24m_target_request_t *dl24m_target_request_create(
    labbridge_api_dl24m_target_request_MODE_e mode,
    double value,
    labbridge_api_dl24m_target_request_UNIT_e unit
    ) {
    return dl24m_target_request_create_internal (
        mode,
        value,
        unit
        );
}

void dl24m_target_request_free(dl24m_target_request_t *dl24m_target_request) {
    if(NULL == dl24m_target_request){
        return ;
    }
    if(dl24m_target_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "dl24m_target_request_free");
        return ;
    }
    listEntry_t *listEntry;
    free(dl24m_target_request);
}

cJSON *dl24m_target_request_convertToJSON(dl24m_target_request_t *dl24m_target_request) {
    cJSON *item = cJSON_CreateObject();

    // dl24m_target_request->mode
    if (labbridge_api_dl24m_target_request_MODE_NULL == dl24m_target_request->mode) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "mode", dl24m_target_request_mode_ToString(dl24m_target_request->mode)) == NULL)
    {
    goto fail; //Enum
    }


    // dl24m_target_request->value
    if (!dl24m_target_request->value) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "value", dl24m_target_request->value) == NULL) {
    goto fail; //Numeric
    }


    // dl24m_target_request->unit
    if (labbridge_api_dl24m_target_request_UNIT_NULL == dl24m_target_request->unit) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "unit", dl24m_target_request_unit_ToString(dl24m_target_request->unit)) == NULL)
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

dl24m_target_request_t *dl24m_target_request_parseFromJSON(cJSON *dl24m_target_requestJSON){

    dl24m_target_request_t *dl24m_target_request_local_var = NULL;

    // dl24m_target_request->mode
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(dl24m_target_requestJSON, "mode");
    if (cJSON_IsNull(mode)) {
        mode = NULL;
    }
    if (!mode) {
        goto end;
    }

    labbridge_api_dl24m_target_request_MODE_e modeVariable;
    
    if(!cJSON_IsString(mode))
    {
    goto end; //Enum
    }
    modeVariable = dl24m_target_request_mode_FromString(mode->valuestring);

    // dl24m_target_request->value
    cJSON *value = cJSON_GetObjectItemCaseSensitive(dl24m_target_requestJSON, "value");
    if (cJSON_IsNull(value)) {
        value = NULL;
    }
    if (!value) {
        goto end;
    }

    
    if(!cJSON_IsNumber(value))
    {
    goto end; //Numeric
    }

    // dl24m_target_request->unit
    cJSON *unit = cJSON_GetObjectItemCaseSensitive(dl24m_target_requestJSON, "unit");
    if (cJSON_IsNull(unit)) {
        unit = NULL;
    }
    if (!unit) {
        goto end;
    }

    labbridge_api_dl24m_target_request_UNIT_e unitVariable;
    
    if(!cJSON_IsString(unit))
    {
    goto end; //Enum
    }
    unitVariable = dl24m_target_request_unit_FromString(unit->valuestring);


    dl24m_target_request_local_var = dl24m_target_request_create_internal (
        modeVariable,
        value->valuedouble,
        unitVariable
        );

    return dl24m_target_request_local_var;
end:
    return NULL;

}
