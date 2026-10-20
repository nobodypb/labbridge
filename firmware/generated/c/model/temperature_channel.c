#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "temperature_channel.h"


char* temperature_channel_kind_ToString(labbridge_api_temperature_channel_KIND_e kind) {
    char* kindArray[] =  { "NULL", "internal", "external_probe", "cell" };
    return kindArray[kind];
}

labbridge_api_temperature_channel_KIND_e temperature_channel_kind_FromString(char* kind){
    int stringToReturn = 0;
    char *kindArray[] =  { "NULL", "internal", "external_probe", "cell" };
    size_t sizeofArray = sizeof(kindArray) / sizeof(kindArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(kind, kindArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}
char* temperature_channel_quality_ToString(labbridge_api_temperature_channel_QUALITY_e quality) {
    char* qualityArray[] =  { "NULL", "valid", "stale", "unavailable" };
    return qualityArray[quality];
}

labbridge_api_temperature_channel_QUALITY_e temperature_channel_quality_FromString(char* quality){
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

static temperature_channel_t *temperature_channel_create_internal(
    char *id,
    labbridge_api_temperature_channel_KIND_e kind,
    labbridge_api_temperature_channel_QUALITY_e quality,
    double value_c,
    long received_ms,
    long age_ms
    ) {
    temperature_channel_t *temperature_channel_local_var = malloc(sizeof(temperature_channel_t));
    if (!temperature_channel_local_var) {
        return NULL;
    }
    temperature_channel_local_var->id = id;
    temperature_channel_local_var->kind = kind;
    temperature_channel_local_var->quality = quality;
    temperature_channel_local_var->value_c = value_c;
    temperature_channel_local_var->received_ms = received_ms;
    temperature_channel_local_var->age_ms = age_ms;

    temperature_channel_local_var->_library_owned = 1;
    return temperature_channel_local_var;
}

__attribute__((deprecated)) temperature_channel_t *temperature_channel_create(
    char *id,
    labbridge_api_temperature_channel_KIND_e kind,
    labbridge_api_temperature_channel_QUALITY_e quality,
    double value_c,
    long received_ms,
    long age_ms
    ) {
    return temperature_channel_create_internal (
        id,
        kind,
        quality,
        value_c,
        received_ms,
        age_ms
        );
}

void temperature_channel_free(temperature_channel_t *temperature_channel) {
    if(NULL == temperature_channel){
        return ;
    }
    if(temperature_channel->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "temperature_channel_free");
        return ;
    }
    listEntry_t *listEntry;
    if (temperature_channel->id) {
        free(temperature_channel->id);
        temperature_channel->id = NULL;
    }
    free(temperature_channel);
}

cJSON *temperature_channel_convertToJSON(temperature_channel_t *temperature_channel) {
    cJSON *item = cJSON_CreateObject();

    // temperature_channel->id
    if (!temperature_channel->id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "id", temperature_channel->id) == NULL) {
    goto fail; //String
    }


    // temperature_channel->kind
    if (labbridge_api_temperature_channel_KIND_NULL == temperature_channel->kind) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "kind", temperature_channel_kind_ToString(temperature_channel->kind)) == NULL)
    {
    goto fail; //Enum
    }


    // temperature_channel->quality
    if (labbridge_api_temperature_channel_QUALITY_NULL == temperature_channel->quality) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "quality", temperature_channel_quality_ToString(temperature_channel->quality)) == NULL)
    {
    goto fail; //Enum
    }


    // temperature_channel->value_c
    if(temperature_channel->value_c) {
    if(cJSON_AddNumberToObject(item, "value_c", temperature_channel->value_c) == NULL) {
    goto fail; //Numeric
    }
    }


    // temperature_channel->received_ms
    if (!temperature_channel->received_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "received_ms", temperature_channel->received_ms) == NULL) {
    goto fail; //Numeric
    }


    // temperature_channel->age_ms
    if (!temperature_channel->age_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "age_ms", temperature_channel->age_ms) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

temperature_channel_t *temperature_channel_parseFromJSON(cJSON *temperature_channelJSON){

    temperature_channel_t *temperature_channel_local_var = NULL;

    // temperature_channel->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(temperature_channelJSON, "id");
    if (cJSON_IsNull(id)) {
        id = NULL;
    }
    if (!id) {
        goto end;
    }

    
    if(!cJSON_IsString(id))
    {
    goto end; //String
    }

    // temperature_channel->kind
    cJSON *kind = cJSON_GetObjectItemCaseSensitive(temperature_channelJSON, "kind");
    if (cJSON_IsNull(kind)) {
        kind = NULL;
    }
    if (!kind) {
        goto end;
    }

    labbridge_api_temperature_channel_KIND_e kindVariable;
    
    if(!cJSON_IsString(kind))
    {
    goto end; //Enum
    }
    kindVariable = temperature_channel_kind_FromString(kind->valuestring);

    // temperature_channel->quality
    cJSON *quality = cJSON_GetObjectItemCaseSensitive(temperature_channelJSON, "quality");
    if (cJSON_IsNull(quality)) {
        quality = NULL;
    }
    if (!quality) {
        goto end;
    }

    labbridge_api_temperature_channel_QUALITY_e qualityVariable;
    
    if(!cJSON_IsString(quality))
    {
    goto end; //Enum
    }
    qualityVariable = temperature_channel_quality_FromString(quality->valuestring);

    // temperature_channel->value_c
    cJSON *value_c = cJSON_GetObjectItemCaseSensitive(temperature_channelJSON, "value_c");
    if (cJSON_IsNull(value_c)) {
        value_c = NULL;
    }
    if (value_c) { 
    if(!cJSON_IsNumber(value_c))
    {
    goto end; //Numeric
    }
    }

    // temperature_channel->received_ms
    cJSON *received_ms = cJSON_GetObjectItemCaseSensitive(temperature_channelJSON, "received_ms");
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

    // temperature_channel->age_ms
    cJSON *age_ms = cJSON_GetObjectItemCaseSensitive(temperature_channelJSON, "age_ms");
    if (cJSON_IsNull(age_ms)) {
        age_ms = NULL;
    }
    if (!age_ms) {
        goto end;
    }

    
    if(!cJSON_IsNumber(age_ms))
    {
    goto end; //Numeric
    }


    temperature_channel_local_var = temperature_channel_create_internal (
        strdup(id->valuestring),
        kindVariable,
        qualityVariable,
        value_c ? value_c->valuedouble : 0,
        received_ms->valuedouble,
        age_ms->valuedouble
        );

    return temperature_channel_local_var;
end:
    return NULL;

}
