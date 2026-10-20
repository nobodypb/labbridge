#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dl24m_register.h"


char* dl24m_register_quality_ToString(labbridge_api_dl24m_register_QUALITY_e quality) {
    char* qualityArray[] =  { "NULL", "valid", "stale", "unavailable" };
    return qualityArray[quality];
}

labbridge_api_dl24m_register_QUALITY_e dl24m_register_quality_FromString(char* quality){
    int stringToReturn = 0;
    char *qualityArray[] =  { "NULL", "valid", "stale", "unavailable" };
    size_t sizeofArray = sizeof(qualityArray) / sizeof(qualityArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(quality, qualityArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static dl24m_register_t *dl24m_register_create_internal(
    char *name,
    int command,
    labbridge_api_dl24m_register_QUALITY_e quality,
    int value,
    int age_ms,
    int received_ms
    ) {
    dl24m_register_t *dl24m_register_local_var = malloc(sizeof(dl24m_register_t));
    if (!dl24m_register_local_var) {
        return NULL;
    }
    dl24m_register_local_var->name = name;
    dl24m_register_local_var->command = command;
    dl24m_register_local_var->quality = quality;
    dl24m_register_local_var->value = value;
    dl24m_register_local_var->age_ms = age_ms;
    dl24m_register_local_var->received_ms = received_ms;

    dl24m_register_local_var->_library_owned = 1;
    return dl24m_register_local_var;
}

__attribute__((deprecated)) dl24m_register_t *dl24m_register_create(
    char *name,
    int command,
    labbridge_api_dl24m_register_QUALITY_e quality,
    int value,
    int age_ms,
    int received_ms
    ) {
    return dl24m_register_create_internal (
        name,
        command,
        quality,
        value,
        age_ms,
        received_ms
        );
}

void dl24m_register_free(dl24m_register_t *dl24m_register) {
    if(NULL == dl24m_register){
        return ;
    }
    if(dl24m_register->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "dl24m_register_free");
        return ;
    }
    listEntry_t *listEntry;
    if (dl24m_register->name) {
        free(dl24m_register->name);
        dl24m_register->name = NULL;
    }
    free(dl24m_register);
}

cJSON *dl24m_register_convertToJSON(dl24m_register_t *dl24m_register) {
    cJSON *item = cJSON_CreateObject();

    // dl24m_register->name
    if (!dl24m_register->name) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "name", dl24m_register->name) == NULL) {
    goto fail; //String
    }


    // dl24m_register->command
    if (!dl24m_register->command) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "command", dl24m_register->command) == NULL) {
    goto fail; //Numeric
    }


    // dl24m_register->quality
    if (labbridge_api_dl24m_register_QUALITY_NULL == dl24m_register->quality) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "quality", dl24m_register_quality_ToString(dl24m_register->quality)) == NULL)
    {
    goto fail; //Enum
    }


    // dl24m_register->value
    if(dl24m_register->value) {
    if(cJSON_AddNumberToObject(item, "value", dl24m_register->value) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_register->age_ms
    if(dl24m_register->age_ms) {
    if(cJSON_AddNumberToObject(item, "age_ms", dl24m_register->age_ms) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_register->received_ms
    if(dl24m_register->received_ms) {
    if(cJSON_AddNumberToObject(item, "received_ms", dl24m_register->received_ms) == NULL) {
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

dl24m_register_t *dl24m_register_parseFromJSON(cJSON *dl24m_registerJSON){

    dl24m_register_t *dl24m_register_local_var = NULL;

    // dl24m_register->name
    cJSON *name = cJSON_GetObjectItemCaseSensitive(dl24m_registerJSON, "name");
    if (cJSON_IsNull(name)) {
        name = NULL;
    }
    if (!name) {
        goto end;
    }

    
    if(!cJSON_IsString(name))
    {
    goto end; //String
    }

    // dl24m_register->command
    cJSON *command = cJSON_GetObjectItemCaseSensitive(dl24m_registerJSON, "command");
    if (cJSON_IsNull(command)) {
        command = NULL;
    }
    if (!command) {
        goto end;
    }

    
    if(!cJSON_IsNumber(command))
    {
    goto end; //Numeric
    }

    // dl24m_register->quality
    cJSON *quality = cJSON_GetObjectItemCaseSensitive(dl24m_registerJSON, "quality");
    if (cJSON_IsNull(quality)) {
        quality = NULL;
    }
    if (!quality) {
        goto end;
    }

    labbridge_api_dl24m_register_QUALITY_e qualityVariable;
    
    if(!cJSON_IsString(quality))
    {
    goto end; //Enum
    }
    qualityVariable = dl24m_register_quality_FromString(quality->valuestring);

    // dl24m_register->value
    cJSON *value = cJSON_GetObjectItemCaseSensitive(dl24m_registerJSON, "value");
    if (cJSON_IsNull(value)) {
        value = NULL;
    }
    if (value) { 
    if(!cJSON_IsNumber(value))
    {
    goto end; //Numeric
    }
    }

    // dl24m_register->age_ms
    cJSON *age_ms = cJSON_GetObjectItemCaseSensitive(dl24m_registerJSON, "age_ms");
    if (cJSON_IsNull(age_ms)) {
        age_ms = NULL;
    }
    if (age_ms) { 
    if(!cJSON_IsNumber(age_ms))
    {
    goto end; //Numeric
    }
    }

    // dl24m_register->received_ms
    cJSON *received_ms = cJSON_GetObjectItemCaseSensitive(dl24m_registerJSON, "received_ms");
    if (cJSON_IsNull(received_ms)) {
        received_ms = NULL;
    }
    if (received_ms) { 
    if(!cJSON_IsNumber(received_ms))
    {
    goto end; //Numeric
    }
    }


    dl24m_register_local_var = dl24m_register_create_internal (
        strdup(name->valuestring),
        command->valuedouble,
        qualityVariable,
        value ? value->valuedouble : 0,
        age_ms ? age_ms->valuedouble : 0,
        received_ms ? received_ms->valuedouble : 0
        );

    return dl24m_register_local_var;
end:
    return NULL;

}
