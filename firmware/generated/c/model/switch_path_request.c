#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "switch_path_request.h"


char* switch_path_request_path_ToString(labbridge_api_switch_path_request_PATH_e path) {
    char* pathArray[] =  { "NULL", "isolated", "charge", "discharge" };
    return pathArray[path];
}

labbridge_api_switch_path_request_PATH_e switch_path_request_path_FromString(char* path){
    int stringToReturn = 0;
    char *pathArray[] =  { "NULL", "isolated", "charge", "discharge" };
    size_t sizeofArray = sizeof(pathArray) / sizeof(pathArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(path, pathArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static switch_path_request_t *switch_path_request_create_internal(
    labbridge_api_switch_path_request_PATH_e path
    ) {
    switch_path_request_t *switch_path_request_local_var = malloc(sizeof(switch_path_request_t));
    if (!switch_path_request_local_var) {
        return NULL;
    }
    switch_path_request_local_var->path = path;

    switch_path_request_local_var->_library_owned = 1;
    return switch_path_request_local_var;
}

__attribute__((deprecated)) switch_path_request_t *switch_path_request_create(
    labbridge_api_switch_path_request_PATH_e path
    ) {
    return switch_path_request_create_internal (
        path
        );
}

void switch_path_request_free(switch_path_request_t *switch_path_request) {
    if(NULL == switch_path_request){
        return ;
    }
    if(switch_path_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "switch_path_request_free");
        return ;
    }
    listEntry_t *listEntry;
    free(switch_path_request);
}

cJSON *switch_path_request_convertToJSON(switch_path_request_t *switch_path_request) {
    cJSON *item = cJSON_CreateObject();

    // switch_path_request->path
    if (labbridge_api_switch_path_request_PATH_NULL == switch_path_request->path) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "path", switch_path_request_path_ToString(switch_path_request->path)) == NULL)
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

switch_path_request_t *switch_path_request_parseFromJSON(cJSON *switch_path_requestJSON){

    switch_path_request_t *switch_path_request_local_var = NULL;

    // switch_path_request->path
    cJSON *path = cJSON_GetObjectItemCaseSensitive(switch_path_requestJSON, "path");
    if (cJSON_IsNull(path)) {
        path = NULL;
    }
    if (!path) {
        goto end;
    }

    labbridge_api_switch_path_request_PATH_e pathVariable;
    
    if(!cJSON_IsString(path))
    {
    goto end; //Enum
    }
    pathVariable = switch_path_request_path_FromString(path->valuestring);


    switch_path_request_local_var = switch_path_request_create_internal (
        pathVariable
        );

    return switch_path_request_local_var;
end:
    return NULL;

}
