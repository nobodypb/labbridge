#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rd60xx_configuration.h"


char* rd60xx_configuration_model_ToString(labbridge_api_rd60xx_configuration_MODEL_e model) {
    char* modelArray[] =  { "NULL", "6018", "6024" };
    return modelArray[model];
}

labbridge_api_rd60xx_configuration_MODEL_e rd60xx_configuration_model_FromString(char* model){
    int stringToReturn = 0;
    char *modelArray[] =  { "NULL", "6018", "6024" };
    size_t sizeofArray = sizeof(modelArray) / sizeof(modelArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(model, modelArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static rd60xx_configuration_t *rd60xx_configuration_create_internal(
    long model
    ) {
    rd60xx_configuration_t *rd60xx_configuration_local_var = malloc(sizeof(rd60xx_configuration_t));
    if (!rd60xx_configuration_local_var) {
        return NULL;
    }
    rd60xx_configuration_local_var->model = model;

    rd60xx_configuration_local_var->_library_owned = 1;
    return rd60xx_configuration_local_var;
}

__attribute__((deprecated)) rd60xx_configuration_t *rd60xx_configuration_create(
    long model
    ) {
    return rd60xx_configuration_create_internal (
        model
        );
}

void rd60xx_configuration_free(rd60xx_configuration_t *rd60xx_configuration) {
    if(NULL == rd60xx_configuration){
        return ;
    }
    if(rd60xx_configuration->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "rd60xx_configuration_free");
        return ;
    }
    listEntry_t *listEntry;
    free(rd60xx_configuration);
}

cJSON *rd60xx_configuration_convertToJSON(rd60xx_configuration_t *rd60xx_configuration) {
    cJSON *item = cJSON_CreateObject();

    // rd60xx_configuration->model
    if (labbridge_api_rd60xx_configuration_MODEL_NULL == rd60xx_configuration->model) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "model", rd60xx_configuration->model) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

rd60xx_configuration_t *rd60xx_configuration_parseFromJSON(cJSON *rd60xx_configurationJSON){

    rd60xx_configuration_t *rd60xx_configuration_local_var = NULL;

    // rd60xx_configuration->model
    cJSON *model = cJSON_GetObjectItemCaseSensitive(rd60xx_configurationJSON, "model");
    if (cJSON_IsNull(model)) {
        model = NULL;
    }
    if (!model) {
        goto end;
    }

    
    if(!cJSON_IsNumber(model))
    {
    goto end; //Numeric
    }


    rd60xx_configuration_local_var = rd60xx_configuration_create_internal (
        model->valuedouble
        );

    return rd60xx_configuration_local_var;
end:
    return NULL;

}
