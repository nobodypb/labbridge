#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sensor_reading.h"


char* sensor_reading_quality_ToString(labbridge_api_sensor_reading_QUALITY_e quality) {
    char* qualityArray[] =  { "NULL", "valid", "stale", "unavailable" };
    return qualityArray[quality];
}

labbridge_api_sensor_reading_QUALITY_e sensor_reading_quality_FromString(char* quality){
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

static sensor_reading_t *sensor_reading_create_internal(
    char *device_id,
    char *component_id,
    char *boot_id,
    char *quantity,
    char *unit,
    labbridge_api_sensor_reading_QUALITY_e quality,
    double value,
    long sequence,
    long received_ms,
    long age_ms,
    char *epoch
    ) {
    sensor_reading_t *sensor_reading_local_var = malloc(sizeof(sensor_reading_t));
    if (!sensor_reading_local_var) {
        return NULL;
    }
    sensor_reading_local_var->device_id = device_id;
    sensor_reading_local_var->component_id = component_id;
    sensor_reading_local_var->boot_id = boot_id;
    sensor_reading_local_var->quantity = quantity;
    sensor_reading_local_var->unit = unit;
    sensor_reading_local_var->quality = quality;
    sensor_reading_local_var->value = value;
    sensor_reading_local_var->sequence = sequence;
    sensor_reading_local_var->received_ms = received_ms;
    sensor_reading_local_var->age_ms = age_ms;
    sensor_reading_local_var->epoch = epoch;

    sensor_reading_local_var->_library_owned = 1;
    return sensor_reading_local_var;
}

__attribute__((deprecated)) sensor_reading_t *sensor_reading_create(
    char *device_id,
    char *component_id,
    char *boot_id,
    char *quantity,
    char *unit,
    labbridge_api_sensor_reading_QUALITY_e quality,
    double value,
    long sequence,
    long received_ms,
    long age_ms,
    char *epoch
    ) {
    return sensor_reading_create_internal (
        device_id,
        component_id,
        boot_id,
        quantity,
        unit,
        quality,
        value,
        sequence,
        received_ms,
        age_ms,
        epoch
        );
}

void sensor_reading_free(sensor_reading_t *sensor_reading) {
    if(NULL == sensor_reading){
        return ;
    }
    if(sensor_reading->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "sensor_reading_free");
        return ;
    }
    listEntry_t *listEntry;
    if (sensor_reading->device_id) {
        free(sensor_reading->device_id);
        sensor_reading->device_id = NULL;
    }
    if (sensor_reading->component_id) {
        free(sensor_reading->component_id);
        sensor_reading->component_id = NULL;
    }
    if (sensor_reading->boot_id) {
        free(sensor_reading->boot_id);
        sensor_reading->boot_id = NULL;
    }
    if (sensor_reading->quantity) {
        free(sensor_reading->quantity);
        sensor_reading->quantity = NULL;
    }
    if (sensor_reading->unit) {
        free(sensor_reading->unit);
        sensor_reading->unit = NULL;
    }
    if (sensor_reading->epoch) {
        free(sensor_reading->epoch);
        sensor_reading->epoch = NULL;
    }
    free(sensor_reading);
}

cJSON *sensor_reading_convertToJSON(sensor_reading_t *sensor_reading) {
    cJSON *item = cJSON_CreateObject();

    // sensor_reading->device_id
    if (!sensor_reading->device_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "device_id", sensor_reading->device_id) == NULL) {
    goto fail; //String
    }


    // sensor_reading->component_id
    if (!sensor_reading->component_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "component_id", sensor_reading->component_id) == NULL) {
    goto fail; //String
    }


    // sensor_reading->boot_id
    if (!sensor_reading->boot_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "boot_id", sensor_reading->boot_id) == NULL) {
    goto fail; //String
    }


    // sensor_reading->quantity
    if (!sensor_reading->quantity) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "quantity", sensor_reading->quantity) == NULL) {
    goto fail; //String
    }


    // sensor_reading->unit
    if (!sensor_reading->unit) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "unit", sensor_reading->unit) == NULL) {
    goto fail; //String
    }


    // sensor_reading->quality
    if (labbridge_api_sensor_reading_QUALITY_NULL == sensor_reading->quality) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "quality", sensor_reading_quality_ToString(sensor_reading->quality)) == NULL)
    {
    goto fail; //Enum
    }


    // sensor_reading->value
    if(sensor_reading->value) {
    if(cJSON_AddNumberToObject(item, "value", sensor_reading->value) == NULL) {
    goto fail; //Numeric
    }
    }


    // sensor_reading->sequence
    if (!sensor_reading->sequence) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "sequence", sensor_reading->sequence) == NULL) {
    goto fail; //Numeric
    }


    // sensor_reading->received_ms
    if (!sensor_reading->received_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "received_ms", sensor_reading->received_ms) == NULL) {
    goto fail; //Numeric
    }


    // sensor_reading->age_ms
    if (!sensor_reading->age_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "age_ms", sensor_reading->age_ms) == NULL) {
    goto fail; //Numeric
    }


    // sensor_reading->epoch
    if(sensor_reading->epoch) {
    if(cJSON_AddStringToObject(item, "epoch", sensor_reading->epoch) == NULL) {
    goto fail; //String
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

sensor_reading_t *sensor_reading_parseFromJSON(cJSON *sensor_readingJSON){

    sensor_reading_t *sensor_reading_local_var = NULL;

    // sensor_reading->device_id
    cJSON *device_id = cJSON_GetObjectItemCaseSensitive(sensor_readingJSON, "device_id");
    if (cJSON_IsNull(device_id)) {
        device_id = NULL;
    }
    if (!device_id) {
        goto end;
    }

    
    if(!cJSON_IsString(device_id))
    {
    goto end; //String
    }

    // sensor_reading->component_id
    cJSON *component_id = cJSON_GetObjectItemCaseSensitive(sensor_readingJSON, "component_id");
    if (cJSON_IsNull(component_id)) {
        component_id = NULL;
    }
    if (!component_id) {
        goto end;
    }

    
    if(!cJSON_IsString(component_id))
    {
    goto end; //String
    }

    // sensor_reading->boot_id
    cJSON *boot_id = cJSON_GetObjectItemCaseSensitive(sensor_readingJSON, "boot_id");
    if (cJSON_IsNull(boot_id)) {
        boot_id = NULL;
    }
    if (!boot_id) {
        goto end;
    }

    
    if(!cJSON_IsString(boot_id))
    {
    goto end; //String
    }

    // sensor_reading->quantity
    cJSON *quantity = cJSON_GetObjectItemCaseSensitive(sensor_readingJSON, "quantity");
    if (cJSON_IsNull(quantity)) {
        quantity = NULL;
    }
    if (!quantity) {
        goto end;
    }

    
    if(!cJSON_IsString(quantity))
    {
    goto end; //String
    }

    // sensor_reading->unit
    cJSON *unit = cJSON_GetObjectItemCaseSensitive(sensor_readingJSON, "unit");
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

    // sensor_reading->quality
    cJSON *quality = cJSON_GetObjectItemCaseSensitive(sensor_readingJSON, "quality");
    if (cJSON_IsNull(quality)) {
        quality = NULL;
    }
    if (!quality) {
        goto end;
    }

    labbridge_api_sensor_reading_QUALITY_e qualityVariable;
    
    if(!cJSON_IsString(quality))
    {
    goto end; //Enum
    }
    qualityVariable = sensor_reading_quality_FromString(quality->valuestring);

    // sensor_reading->value
    cJSON *value = cJSON_GetObjectItemCaseSensitive(sensor_readingJSON, "value");
    if (cJSON_IsNull(value)) {
        value = NULL;
    }
    if (value) { 
    if(!cJSON_IsNumber(value))
    {
    goto end; //Numeric
    }
    }

    // sensor_reading->sequence
    cJSON *sequence = cJSON_GetObjectItemCaseSensitive(sensor_readingJSON, "sequence");
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

    // sensor_reading->received_ms
    cJSON *received_ms = cJSON_GetObjectItemCaseSensitive(sensor_readingJSON, "received_ms");
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

    // sensor_reading->age_ms
    cJSON *age_ms = cJSON_GetObjectItemCaseSensitive(sensor_readingJSON, "age_ms");
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

    // sensor_reading->epoch
    cJSON *epoch = cJSON_GetObjectItemCaseSensitive(sensor_readingJSON, "epoch");
    if (cJSON_IsNull(epoch)) {
        epoch = NULL;
    }
    if (epoch) { 
    if(!cJSON_IsString(epoch) && !cJSON_IsNull(epoch))
    {
    goto end; //String
    }
    }


    sensor_reading_local_var = sensor_reading_create_internal (
        strdup(device_id->valuestring),
        strdup(component_id->valuestring),
        strdup(boot_id->valuestring),
        strdup(quantity->valuestring),
        strdup(unit->valuestring),
        qualityVariable,
        value ? value->valuedouble : 0,
        sequence->valuedouble,
        received_ms->valuedouble,
        age_ms->valuedouble,
        epoch && !cJSON_IsNull(epoch) ? strdup(epoch->valuestring) : NULL
        );

    return sensor_reading_local_var;
end:
    return NULL;

}
