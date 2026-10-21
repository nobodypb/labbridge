#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "native_reading.h"


char* native_reading_quality_ToString(labbridge_api_native_reading_QUALITY_e quality) {
    char* qualityArray[] =  { "NULL", "valid", "stale", "unavailable" };
    return qualityArray[quality];
}

labbridge_api_native_reading_QUALITY_e native_reading_quality_FromString(char* quality){
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

static native_reading_t *native_reading_create_internal(
    char *id,
    double value,
    char *unit,
    char *origin,
    labbridge_api_native_reading_QUALITY_e quality,
    int received_ms,
    int age_ms
    ) {
    native_reading_t *native_reading_local_var = malloc(sizeof(native_reading_t));
    if (!native_reading_local_var) {
        return NULL;
    }
    native_reading_local_var->id = id;
    native_reading_local_var->value = value;
    native_reading_local_var->unit = unit;
    native_reading_local_var->origin = origin;
    native_reading_local_var->quality = quality;
    native_reading_local_var->received_ms = received_ms;
    native_reading_local_var->age_ms = age_ms;

    native_reading_local_var->_library_owned = 1;
    return native_reading_local_var;
}

__attribute__((deprecated)) native_reading_t *native_reading_create(
    char *id,
    double value,
    char *unit,
    char *origin,
    labbridge_api_native_reading_QUALITY_e quality,
    int received_ms,
    int age_ms
    ) {
    return native_reading_create_internal (
        id,
        value,
        unit,
        origin,
        quality,
        received_ms,
        age_ms
        );
}

void native_reading_free(native_reading_t *native_reading) {
    if(NULL == native_reading){
        return ;
    }
    if(native_reading->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "native_reading_free");
        return ;
    }
    listEntry_t *listEntry;
    if (native_reading->id) {
        free(native_reading->id);
        native_reading->id = NULL;
    }
    if (native_reading->unit) {
        free(native_reading->unit);
        native_reading->unit = NULL;
    }
    if (native_reading->origin) {
        free(native_reading->origin);
        native_reading->origin = NULL;
    }
    free(native_reading);
}

cJSON *native_reading_convertToJSON(native_reading_t *native_reading) {
    cJSON *item = cJSON_CreateObject();

    // native_reading->id
    if (!native_reading->id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "id", native_reading->id) == NULL) {
    goto fail; //String
    }


    // native_reading->value
    if(native_reading->value) {
    if(cJSON_AddNumberToObject(item, "value", native_reading->value) == NULL) {
    goto fail; //Numeric
    }
    }


    // native_reading->unit
    if (!native_reading->unit) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "unit", native_reading->unit) == NULL) {
    goto fail; //String
    }


    // native_reading->origin
    if (!native_reading->origin) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "origin", native_reading->origin) == NULL) {
    goto fail; //String
    }


    // native_reading->quality
    if (labbridge_api_native_reading_QUALITY_NULL == native_reading->quality) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "quality", native_reading_quality_ToString(native_reading->quality)) == NULL)
    {
    goto fail; //Enum
    }


    // native_reading->received_ms
    if(native_reading->received_ms) {
    if(cJSON_AddNumberToObject(item, "received_ms", native_reading->received_ms) == NULL) {
    goto fail; //Numeric
    }
    }


    // native_reading->age_ms
    if(native_reading->age_ms) {
    if(cJSON_AddNumberToObject(item, "age_ms", native_reading->age_ms) == NULL) {
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

native_reading_t *native_reading_parseFromJSON(cJSON *native_readingJSON){

    native_reading_t *native_reading_local_var = NULL;

    // native_reading->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(native_readingJSON, "id");
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

    // native_reading->value
    cJSON *value = cJSON_GetObjectItemCaseSensitive(native_readingJSON, "value");
    if (cJSON_IsNull(value)) {
        value = NULL;
    }
    if (value) { 
    if(!cJSON_IsNumber(value))
    {
    goto end; //Numeric
    }
    }

    // native_reading->unit
    cJSON *unit = cJSON_GetObjectItemCaseSensitive(native_readingJSON, "unit");
    if (cJSON_IsNull(unit)) {
        unit = NULL;
    }
    if (!unit) {
        goto end;
    }

    
    if(!cJSON_IsString(unit))
    {
    goto end; //String
    }

    // native_reading->origin
    cJSON *origin = cJSON_GetObjectItemCaseSensitive(native_readingJSON, "origin");
    if (cJSON_IsNull(origin)) {
        origin = NULL;
    }
    if (!origin) {
        goto end;
    }

    
    if(!cJSON_IsString(origin))
    {
    goto end; //String
    }

    // native_reading->quality
    cJSON *quality = cJSON_GetObjectItemCaseSensitive(native_readingJSON, "quality");
    if (cJSON_IsNull(quality)) {
        quality = NULL;
    }
    if (!quality) {
        goto end;
    }

    labbridge_api_native_reading_QUALITY_e qualityVariable;
    
    if(!cJSON_IsString(quality))
    {
    goto end; //Enum
    }
    qualityVariable = native_reading_quality_FromString(quality->valuestring);

    // native_reading->received_ms
    cJSON *received_ms = cJSON_GetObjectItemCaseSensitive(native_readingJSON, "received_ms");
    if (cJSON_IsNull(received_ms)) {
        received_ms = NULL;
    }
    if (received_ms) { 
    if(!cJSON_IsNumber(received_ms))
    {
    goto end; //Numeric
    }
    }

    // native_reading->age_ms
    cJSON *age_ms = cJSON_GetObjectItemCaseSensitive(native_readingJSON, "age_ms");
    if (cJSON_IsNull(age_ms)) {
        age_ms = NULL;
    }
    if (age_ms) { 
    if(!cJSON_IsNumber(age_ms))
    {
    goto end; //Numeric
    }
    }


    native_reading_local_var = native_reading_create_internal (
        strdup(id->valuestring),
        value ? value->valuedouble : 0,
        strdup(unit->valuestring),
        strdup(origin->valuestring),
        qualityVariable,
        received_ms ? received_ms->valuedouble : 0,
        age_ms ? age_ms->valuedouble : 0
        );

    return native_reading_local_var;
end:
    return NULL;

}
