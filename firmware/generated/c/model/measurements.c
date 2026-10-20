#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "measurements.h"


char* measurements_quality_ToString(labbridge_api_measurements_QUALITY_e quality) {
    char* qualityArray[] =  { "NULL", "valid", "stale", "unavailable" };
    return qualityArray[quality];
}

labbridge_api_measurements_QUALITY_e measurements_quality_FromString(char* quality){
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
char* measurements_regulation_mode_ToString(labbridge_api_measurements_REGULATIONMODE_e regulation_mode) {
    char* regulation_modeArray[] =  { "NULL", "cc", "cv", "unknown" };
    return regulation_modeArray[regulation_mode];
}

labbridge_api_measurements_REGULATIONMODE_e measurements_regulation_mode_FromString(char* regulation_mode){
    int stringToReturn = 0;
    char *regulation_modeArray[] =  { "NULL", "cc", "cv", "unknown" };
    size_t sizeofArray = sizeof(regulation_modeArray) / sizeof(regulation_modeArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(regulation_mode, regulation_modeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static measurements_t *measurements_create_internal(
    char *device_id,
    long sequence,
    long received_ms,
    long age_ms,
    labbridge_api_measurements_QUALITY_e quality,
    double voltage_v,
    double current_a,
    double power_w,
    double capacity_ah,
    double energy_wh,
    list_t *temperature_channels,
    double set_voltage_v,
    double set_current_a,
    labbridge_api_measurements_REGULATIONMODE_e regulation_mode,
    int protection_code,
    char *boot_id
    ) {
    measurements_t *measurements_local_var = malloc(sizeof(measurements_t));
    if (!measurements_local_var) {
        return NULL;
    }
    measurements_local_var->device_id = device_id;
    measurements_local_var->sequence = sequence;
    measurements_local_var->received_ms = received_ms;
    measurements_local_var->age_ms = age_ms;
    measurements_local_var->quality = quality;
    measurements_local_var->voltage_v = voltage_v;
    measurements_local_var->current_a = current_a;
    measurements_local_var->power_w = power_w;
    measurements_local_var->capacity_ah = capacity_ah;
    measurements_local_var->energy_wh = energy_wh;
    measurements_local_var->temperature_channels = temperature_channels;
    measurements_local_var->set_voltage_v = set_voltage_v;
    measurements_local_var->set_current_a = set_current_a;
    measurements_local_var->regulation_mode = regulation_mode;
    measurements_local_var->protection_code = protection_code;
    measurements_local_var->boot_id = boot_id;

    measurements_local_var->_library_owned = 1;
    return measurements_local_var;
}

__attribute__((deprecated)) measurements_t *measurements_create(
    char *device_id,
    long sequence,
    long received_ms,
    long age_ms,
    labbridge_api_measurements_QUALITY_e quality,
    double voltage_v,
    double current_a,
    double power_w,
    double capacity_ah,
    double energy_wh,
    list_t *temperature_channels,
    double set_voltage_v,
    double set_current_a,
    labbridge_api_measurements_REGULATIONMODE_e regulation_mode,
    int protection_code,
    char *boot_id
    ) {
    return measurements_create_internal (
        device_id,
        sequence,
        received_ms,
        age_ms,
        quality,
        voltage_v,
        current_a,
        power_w,
        capacity_ah,
        energy_wh,
        temperature_channels,
        set_voltage_v,
        set_current_a,
        regulation_mode,
        protection_code,
        boot_id
        );
}

void measurements_free(measurements_t *measurements) {
    if(NULL == measurements){
        return ;
    }
    if(measurements->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "measurements_free");
        return ;
    }
    listEntry_t *listEntry;
    if (measurements->device_id) {
        free(measurements->device_id);
        measurements->device_id = NULL;
    }
    if (measurements->temperature_channels) {
        list_ForEach(listEntry, measurements->temperature_channels) {
            temperature_channel_free(listEntry->data);
        }
        list_freeList(measurements->temperature_channels);
        measurements->temperature_channels = NULL;
    }
    if (measurements->boot_id) {
        free(measurements->boot_id);
        measurements->boot_id = NULL;
    }
    free(measurements);
}

cJSON *measurements_convertToJSON(measurements_t *measurements) {
    cJSON *item = cJSON_CreateObject();

    // measurements->device_id
    if (!measurements->device_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "device_id", measurements->device_id) == NULL) {
    goto fail; //String
    }


    // measurements->sequence
    if (!measurements->sequence) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "sequence", measurements->sequence) == NULL) {
    goto fail; //Numeric
    }


    // measurements->received_ms
    if (!measurements->received_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "received_ms", measurements->received_ms) == NULL) {
    goto fail; //Numeric
    }


    // measurements->age_ms
    if (!measurements->age_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "age_ms", measurements->age_ms) == NULL) {
    goto fail; //Numeric
    }


    // measurements->quality
    if (labbridge_api_measurements_QUALITY_NULL == measurements->quality) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "quality", measurements_quality_ToString(measurements->quality)) == NULL)
    {
    goto fail; //Enum
    }


    // measurements->voltage_v
    if(measurements->voltage_v) {
    if(cJSON_AddNumberToObject(item, "voltage_v", measurements->voltage_v) == NULL) {
    goto fail; //Numeric
    }
    }


    // measurements->current_a
    if(measurements->current_a) {
    if(cJSON_AddNumberToObject(item, "current_a", measurements->current_a) == NULL) {
    goto fail; //Numeric
    }
    }


    // measurements->power_w
    if(measurements->power_w) {
    if(cJSON_AddNumberToObject(item, "power_w", measurements->power_w) == NULL) {
    goto fail; //Numeric
    }
    }


    // measurements->capacity_ah
    if(measurements->capacity_ah) {
    if(cJSON_AddNumberToObject(item, "capacity_ah", measurements->capacity_ah) == NULL) {
    goto fail; //Numeric
    }
    }


    // measurements->energy_wh
    if(measurements->energy_wh) {
    if(cJSON_AddNumberToObject(item, "energy_wh", measurements->energy_wh) == NULL) {
    goto fail; //Numeric
    }
    }


    // measurements->temperature_channels
    if (!measurements->temperature_channels) {
        goto fail;
    }
    cJSON *temperature_channels = cJSON_AddArrayToObject(item, "temperature_channels");
    if(temperature_channels == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *temperature_channelsListEntry;
    if (measurements->temperature_channels) {
    list_ForEach(temperature_channelsListEntry, measurements->temperature_channels) {
    cJSON *itemLocal = temperature_channel_convertToJSON(temperature_channelsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(temperature_channels, itemLocal);
    }
    }


    // measurements->set_voltage_v
    if(measurements->set_voltage_v) {
    if(cJSON_AddNumberToObject(item, "set_voltage_v", measurements->set_voltage_v) == NULL) {
    goto fail; //Numeric
    }
    }


    // measurements->set_current_a
    if(measurements->set_current_a) {
    if(cJSON_AddNumberToObject(item, "set_current_a", measurements->set_current_a) == NULL) {
    goto fail; //Numeric
    }
    }


    // measurements->regulation_mode
    if(measurements->regulation_mode != labbridge_api_measurements_REGULATIONMODE_NULL) {
    if(cJSON_AddStringToObject(item, "regulation_mode", measurements_regulation_mode_ToString(measurements->regulation_mode)) == NULL)
    {
    goto fail; //Enum
    }
    }


    // measurements->protection_code
    if(measurements->protection_code) {
    if(cJSON_AddNumberToObject(item, "protection_code", measurements->protection_code) == NULL) {
    goto fail; //Numeric
    }
    }


    // measurements->boot_id
    if(measurements->boot_id) {
    if(cJSON_AddStringToObject(item, "boot_id", measurements->boot_id) == NULL) {
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

measurements_t *measurements_parseFromJSON(cJSON *measurementsJSON){

    measurements_t *measurements_local_var = NULL;

    // define the local list for measurements->temperature_channels
    list_t *temperature_channelsList = NULL;

    // measurements->device_id
    cJSON *device_id = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "device_id");
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

    // measurements->sequence
    cJSON *sequence = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "sequence");
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

    // measurements->received_ms
    cJSON *received_ms = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "received_ms");
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

    // measurements->age_ms
    cJSON *age_ms = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "age_ms");
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

    // measurements->quality
    cJSON *quality = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "quality");
    if (cJSON_IsNull(quality)) {
        quality = NULL;
    }
    if (!quality) {
        goto end;
    }

    labbridge_api_measurements_QUALITY_e qualityVariable;
    
    if(!cJSON_IsString(quality))
    {
    goto end; //Enum
    }
    qualityVariable = measurements_quality_FromString(quality->valuestring);

    // measurements->voltage_v
    cJSON *voltage_v = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "voltage_v");
    if (cJSON_IsNull(voltage_v)) {
        voltage_v = NULL;
    }
    if (voltage_v) { 
    if(!cJSON_IsNumber(voltage_v))
    {
    goto end; //Numeric
    }
    }

    // measurements->current_a
    cJSON *current_a = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "current_a");
    if (cJSON_IsNull(current_a)) {
        current_a = NULL;
    }
    if (current_a) { 
    if(!cJSON_IsNumber(current_a))
    {
    goto end; //Numeric
    }
    }

    // measurements->power_w
    cJSON *power_w = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "power_w");
    if (cJSON_IsNull(power_w)) {
        power_w = NULL;
    }
    if (power_w) { 
    if(!cJSON_IsNumber(power_w))
    {
    goto end; //Numeric
    }
    }

    // measurements->capacity_ah
    cJSON *capacity_ah = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "capacity_ah");
    if (cJSON_IsNull(capacity_ah)) {
        capacity_ah = NULL;
    }
    if (capacity_ah) { 
    if(!cJSON_IsNumber(capacity_ah))
    {
    goto end; //Numeric
    }
    }

    // measurements->energy_wh
    cJSON *energy_wh = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "energy_wh");
    if (cJSON_IsNull(energy_wh)) {
        energy_wh = NULL;
    }
    if (energy_wh) { 
    if(!cJSON_IsNumber(energy_wh))
    {
    goto end; //Numeric
    }
    }

    // measurements->temperature_channels
    cJSON *temperature_channels = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "temperature_channels");
    if (cJSON_IsNull(temperature_channels)) {
        temperature_channels = NULL;
    }
    if (!temperature_channels) {
        goto end;
    }

    
    cJSON *temperature_channels_local_nonprimitive = NULL;
    if(!cJSON_IsArray(temperature_channels)){
        goto end; //nonprimitive container
    }

    temperature_channelsList = list_createList();

    cJSON_ArrayForEach(temperature_channels_local_nonprimitive,temperature_channels )
    {
        if(!cJSON_IsObject(temperature_channels_local_nonprimitive)){
            goto end;
        }
        temperature_channel_t *temperature_channelsItem = temperature_channel_parseFromJSON(temperature_channels_local_nonprimitive);

        list_addElement(temperature_channelsList, temperature_channelsItem);
    }

    // measurements->set_voltage_v
    cJSON *set_voltage_v = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "set_voltage_v");
    if (cJSON_IsNull(set_voltage_v)) {
        set_voltage_v = NULL;
    }
    if (set_voltage_v) { 
    if(!cJSON_IsNumber(set_voltage_v))
    {
    goto end; //Numeric
    }
    }

    // measurements->set_current_a
    cJSON *set_current_a = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "set_current_a");
    if (cJSON_IsNull(set_current_a)) {
        set_current_a = NULL;
    }
    if (set_current_a) { 
    if(!cJSON_IsNumber(set_current_a))
    {
    goto end; //Numeric
    }
    }

    // measurements->regulation_mode
    cJSON *regulation_mode = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "regulation_mode");
    if (cJSON_IsNull(regulation_mode)) {
        regulation_mode = NULL;
    }
    labbridge_api_measurements_REGULATIONMODE_e regulation_modeVariable;
    if (regulation_mode) { 
    if(!cJSON_IsString(regulation_mode))
    {
    goto end; //Enum
    }
    regulation_modeVariable = measurements_regulation_mode_FromString(regulation_mode->valuestring);
    }

    // measurements->protection_code
    cJSON *protection_code = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "protection_code");
    if (cJSON_IsNull(protection_code)) {
        protection_code = NULL;
    }
    if (protection_code) { 
    if(!cJSON_IsNumber(protection_code))
    {
    goto end; //Numeric
    }
    }

    // measurements->boot_id
    cJSON *boot_id = cJSON_GetObjectItemCaseSensitive(measurementsJSON, "boot_id");
    if (cJSON_IsNull(boot_id)) {
        boot_id = NULL;
    }
    if (boot_id) { 
    if(!cJSON_IsString(boot_id) && !cJSON_IsNull(boot_id))
    {
    goto end; //String
    }
    }


    measurements_local_var = measurements_create_internal (
        strdup(device_id->valuestring),
        sequence->valuedouble,
        received_ms->valuedouble,
        age_ms->valuedouble,
        qualityVariable,
        voltage_v ? voltage_v->valuedouble : 0,
        current_a ? current_a->valuedouble : 0,
        power_w ? power_w->valuedouble : 0,
        capacity_ah ? capacity_ah->valuedouble : 0,
        energy_wh ? energy_wh->valuedouble : 0,
        temperature_channelsList,
        set_voltage_v ? set_voltage_v->valuedouble : 0,
        set_current_a ? set_current_a->valuedouble : 0,
        regulation_mode ? regulation_modeVariable : labbridge_api_measurements_REGULATIONMODE_NULL,
        protection_code ? protection_code->valuedouble : 0,
        boot_id && !cJSON_IsNull(boot_id) ? strdup(boot_id->valuestring) : NULL
        );

    return measurements_local_var;
end:
    if (temperature_channelsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, temperature_channelsList) {
            temperature_channel_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(temperature_channelsList);
        temperature_channelsList = NULL;
    }
    return NULL;

}
