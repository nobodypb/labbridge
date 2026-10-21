#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dl24m_trace_entry.h"


char* dl24m_trace_entry_direction_ToString(labbridge_api_dl24m_trace_entry_DIRECTION_e direction) {
    char* directionArray[] =  { "NULL", "tx", "rx" };
    return directionArray[direction];
}

labbridge_api_dl24m_trace_entry_DIRECTION_e dl24m_trace_entry_direction_FromString(char* direction){
    int stringToReturn = 0;
    char *directionArray[] =  { "NULL", "tx", "rx" };
    size_t sizeofArray = sizeof(directionArray) / sizeof(directionArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(direction, directionArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}
char* dl24m_trace_entry_origin_ToString(labbridge_api_dl24m_trace_entry_ORIGIN_e origin) {
    char* originArray[] =  { "NULL", "ble-stack", "endpoint" };
    return originArray[origin];
}

labbridge_api_dl24m_trace_entry_ORIGIN_e dl24m_trace_entry_origin_FromString(char* origin){
    int stringToReturn = 0;
    char *originArray[] =  { "NULL", "ble-stack", "endpoint" };
    size_t sizeofArray = sizeof(originArray) / sizeof(originArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(origin, originArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static dl24m_trace_entry_t *dl24m_trace_entry_create_internal(
    int sequence,
    int received_ms,
    int generation,
    labbridge_api_dl24m_trace_entry_DIRECTION_e direction,
    char *hex,
    int truncated,
    labbridge_api_dl24m_trace_entry_ORIGIN_e origin
    ) {
    dl24m_trace_entry_t *dl24m_trace_entry_local_var = malloc(sizeof(dl24m_trace_entry_t));
    if (!dl24m_trace_entry_local_var) {
        return NULL;
    }
    dl24m_trace_entry_local_var->sequence = sequence;
    dl24m_trace_entry_local_var->received_ms = received_ms;
    dl24m_trace_entry_local_var->generation = generation;
    dl24m_trace_entry_local_var->direction = direction;
    dl24m_trace_entry_local_var->hex = hex;
    dl24m_trace_entry_local_var->truncated = truncated;
    dl24m_trace_entry_local_var->origin = origin;

    dl24m_trace_entry_local_var->_library_owned = 1;
    return dl24m_trace_entry_local_var;
}

__attribute__((deprecated)) dl24m_trace_entry_t *dl24m_trace_entry_create(
    int sequence,
    int received_ms,
    int generation,
    labbridge_api_dl24m_trace_entry_DIRECTION_e direction,
    char *hex,
    int truncated,
    labbridge_api_dl24m_trace_entry_ORIGIN_e origin
    ) {
    return dl24m_trace_entry_create_internal (
        sequence,
        received_ms,
        generation,
        direction,
        hex,
        truncated,
        origin
        );
}

void dl24m_trace_entry_free(dl24m_trace_entry_t *dl24m_trace_entry) {
    if(NULL == dl24m_trace_entry){
        return ;
    }
    if(dl24m_trace_entry->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "dl24m_trace_entry_free");
        return ;
    }
    listEntry_t *listEntry;
    if (dl24m_trace_entry->hex) {
        free(dl24m_trace_entry->hex);
        dl24m_trace_entry->hex = NULL;
    }
    free(dl24m_trace_entry);
}

cJSON *dl24m_trace_entry_convertToJSON(dl24m_trace_entry_t *dl24m_trace_entry) {
    cJSON *item = cJSON_CreateObject();

    // dl24m_trace_entry->sequence
    if (!dl24m_trace_entry->sequence) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "sequence", dl24m_trace_entry->sequence) == NULL) {
    goto fail; //Numeric
    }


    // dl24m_trace_entry->received_ms
    if (!dl24m_trace_entry->received_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "received_ms", dl24m_trace_entry->received_ms) == NULL) {
    goto fail; //Numeric
    }


    // dl24m_trace_entry->generation
    if (!dl24m_trace_entry->generation) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "generation", dl24m_trace_entry->generation) == NULL) {
    goto fail; //Numeric
    }


    // dl24m_trace_entry->direction
    if (labbridge_api_dl24m_trace_entry_DIRECTION_NULL == dl24m_trace_entry->direction) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "direction", dl24m_trace_entry_direction_ToString(dl24m_trace_entry->direction)) == NULL)
    {
    goto fail; //Enum
    }


    // dl24m_trace_entry->hex
    if (!dl24m_trace_entry->hex) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "hex", dl24m_trace_entry->hex) == NULL) {
    goto fail; //String
    }


    // dl24m_trace_entry->truncated
    if (!dl24m_trace_entry->truncated) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "truncated", dl24m_trace_entry->truncated) == NULL) {
    goto fail; //Bool
    }


    // dl24m_trace_entry->origin
    if (labbridge_api_dl24m_trace_entry_ORIGIN_NULL == dl24m_trace_entry->origin) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "origin", dl24m_trace_entry_origin_ToString(dl24m_trace_entry->origin)) == NULL)
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

dl24m_trace_entry_t *dl24m_trace_entry_parseFromJSON(cJSON *dl24m_trace_entryJSON){

    dl24m_trace_entry_t *dl24m_trace_entry_local_var = NULL;

    // dl24m_trace_entry->sequence
    cJSON *sequence = cJSON_GetObjectItemCaseSensitive(dl24m_trace_entryJSON, "sequence");
    if (cJSON_IsNull(sequence)) {
        sequence = NULL;
    }
    if (!sequence) {
        goto end;
    }

    
    if(!cJSON_IsNumber(sequence))
    {
    goto end; //Numeric
    }

    // dl24m_trace_entry->received_ms
    cJSON *received_ms = cJSON_GetObjectItemCaseSensitive(dl24m_trace_entryJSON, "received_ms");
    if (cJSON_IsNull(received_ms)) {
        received_ms = NULL;
    }
    if (!received_ms) {
        goto end;
    }

    
    if(!cJSON_IsNumber(received_ms))
    {
    goto end; //Numeric
    }

    // dl24m_trace_entry->generation
    cJSON *generation = cJSON_GetObjectItemCaseSensitive(dl24m_trace_entryJSON, "generation");
    if (cJSON_IsNull(generation)) {
        generation = NULL;
    }
    if (!generation) {
        goto end;
    }

    
    if(!cJSON_IsNumber(generation))
    {
    goto end; //Numeric
    }

    // dl24m_trace_entry->direction
    cJSON *direction = cJSON_GetObjectItemCaseSensitive(dl24m_trace_entryJSON, "direction");
    if (cJSON_IsNull(direction)) {
        direction = NULL;
    }
    if (!direction) {
        goto end;
    }

    labbridge_api_dl24m_trace_entry_DIRECTION_e directionVariable;
    
    if(!cJSON_IsString(direction))
    {
    goto end; //Enum
    }
    directionVariable = dl24m_trace_entry_direction_FromString(direction->valuestring);

    // dl24m_trace_entry->hex
    cJSON *hex = cJSON_GetObjectItemCaseSensitive(dl24m_trace_entryJSON, "hex");
    if (cJSON_IsNull(hex)) {
        hex = NULL;
    }
    if (!hex) {
        goto end;
    }

    
    if(!cJSON_IsString(hex))
    {
    goto end; //String
    }

    // dl24m_trace_entry->truncated
    cJSON *truncated = cJSON_GetObjectItemCaseSensitive(dl24m_trace_entryJSON, "truncated");
    if (cJSON_IsNull(truncated)) {
        truncated = NULL;
    }
    if (!truncated) {
        goto end;
    }

    
    if(!cJSON_IsBool(truncated))
    {
    goto end; //Bool
    }

    // dl24m_trace_entry->origin
    cJSON *origin = cJSON_GetObjectItemCaseSensitive(dl24m_trace_entryJSON, "origin");
    if (cJSON_IsNull(origin)) {
        origin = NULL;
    }
    if (!origin) {
        goto end;
    }

    labbridge_api_dl24m_trace_entry_ORIGIN_e originVariable;
    
    if(!cJSON_IsString(origin))
    {
    goto end; //Enum
    }
    originVariable = dl24m_trace_entry_origin_FromString(origin->valuestring);


    dl24m_trace_entry_local_var = dl24m_trace_entry_create_internal (
        sequence->valuedouble,
        received_ms->valuedouble,
        generation->valuedouble,
        directionVariable,
        strdup(hex->valuestring),
        truncated->valueint,
        originVariable
        );

    return dl24m_trace_entry_local_var;
end:
    return NULL;

}
