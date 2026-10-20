#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cell_temperature_guard.h"



static cell_temperature_guard_t *cell_temperature_guard_create_internal(
    char *sensor_device_id,
    char *channel_id,
    double minimum_c,
    double maximum_c,
    int max_age_ms
    ) {
    cell_temperature_guard_t *cell_temperature_guard_local_var = malloc(sizeof(cell_temperature_guard_t));
    if (!cell_temperature_guard_local_var) {
        return NULL;
    }
    cell_temperature_guard_local_var->sensor_device_id = sensor_device_id;
    cell_temperature_guard_local_var->channel_id = channel_id;
    cell_temperature_guard_local_var->minimum_c = minimum_c;
    cell_temperature_guard_local_var->maximum_c = maximum_c;
    cell_temperature_guard_local_var->max_age_ms = max_age_ms;

    cell_temperature_guard_local_var->_library_owned = 1;
    return cell_temperature_guard_local_var;
}

__attribute__((deprecated)) cell_temperature_guard_t *cell_temperature_guard_create(
    char *sensor_device_id,
    char *channel_id,
    double minimum_c,
    double maximum_c,
    int max_age_ms
    ) {
    return cell_temperature_guard_create_internal (
        sensor_device_id,
        channel_id,
        minimum_c,
        maximum_c,
        max_age_ms
        );
}

void cell_temperature_guard_free(cell_temperature_guard_t *cell_temperature_guard) {
    if(NULL == cell_temperature_guard){
        return ;
    }
    if(cell_temperature_guard->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "cell_temperature_guard_free");
        return ;
    }
    listEntry_t *listEntry;
    if (cell_temperature_guard->sensor_device_id) {
        free(cell_temperature_guard->sensor_device_id);
        cell_temperature_guard->sensor_device_id = NULL;
    }
    if (cell_temperature_guard->channel_id) {
        free(cell_temperature_guard->channel_id);
        cell_temperature_guard->channel_id = NULL;
    }
    free(cell_temperature_guard);
}

cJSON *cell_temperature_guard_convertToJSON(cell_temperature_guard_t *cell_temperature_guard) {
    cJSON *item = cJSON_CreateObject();

    // cell_temperature_guard->sensor_device_id
    if (!cell_temperature_guard->sensor_device_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "sensor_device_id", cell_temperature_guard->sensor_device_id) == NULL) {
    goto fail; //String
    }


    // cell_temperature_guard->channel_id
    if (!cell_temperature_guard->channel_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "channel_id", cell_temperature_guard->channel_id) == NULL) {
    goto fail; //String
    }


    // cell_temperature_guard->minimum_c
    if (!cell_temperature_guard->minimum_c) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "minimum_c", cell_temperature_guard->minimum_c) == NULL) {
    goto fail; //Numeric
    }


    // cell_temperature_guard->maximum_c
    if (!cell_temperature_guard->maximum_c) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "maximum_c", cell_temperature_guard->maximum_c) == NULL) {
    goto fail; //Numeric
    }


    // cell_temperature_guard->max_age_ms
    if (!cell_temperature_guard->max_age_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "max_age_ms", cell_temperature_guard->max_age_ms) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

cell_temperature_guard_t *cell_temperature_guard_parseFromJSON(cJSON *cell_temperature_guardJSON){

    cell_temperature_guard_t *cell_temperature_guard_local_var = NULL;

    // cell_temperature_guard->sensor_device_id
    cJSON *sensor_device_id = cJSON_GetObjectItemCaseSensitive(cell_temperature_guardJSON, "sensor_device_id");
    if (cJSON_IsNull(sensor_device_id)) {
        sensor_device_id = NULL;
    }
    if (!sensor_device_id) {
        goto end;
    }

    
    if(!cJSON_IsString(sensor_device_id))
    {
    goto end; //String
    }

    // cell_temperature_guard->channel_id
    cJSON *channel_id = cJSON_GetObjectItemCaseSensitive(cell_temperature_guardJSON, "channel_id");
    if (cJSON_IsNull(channel_id)) {
        channel_id = NULL;
    }
    if (!channel_id) {
        goto end;
    }

    
    if(!cJSON_IsString(channel_id))
    {
    goto end; //String
    }

    // cell_temperature_guard->minimum_c
    cJSON *minimum_c = cJSON_GetObjectItemCaseSensitive(cell_temperature_guardJSON, "minimum_c");
    if (cJSON_IsNull(minimum_c)) {
        minimum_c = NULL;
    }
    if (!minimum_c) {
        goto end;
    }

    
    if(!cJSON_IsNumber(minimum_c))
    {
    goto end; //Numeric
    }

    // cell_temperature_guard->maximum_c
    cJSON *maximum_c = cJSON_GetObjectItemCaseSensitive(cell_temperature_guardJSON, "maximum_c");
    if (cJSON_IsNull(maximum_c)) {
        maximum_c = NULL;
    }
    if (!maximum_c) {
        goto end;
    }

    
    if(!cJSON_IsNumber(maximum_c))
    {
    goto end; //Numeric
    }

    // cell_temperature_guard->max_age_ms
    cJSON *max_age_ms = cJSON_GetObjectItemCaseSensitive(cell_temperature_guardJSON, "max_age_ms");
    if (cJSON_IsNull(max_age_ms)) {
        max_age_ms = NULL;
    }
    if (!max_age_ms) {
        goto end;
    }

    
    if(!cJSON_IsNumber(max_age_ms))
    {
    goto end; //Numeric
    }


    cell_temperature_guard_local_var = cell_temperature_guard_create_internal (
        strdup(sensor_device_id->valuestring),
        strdup(channel_id->valuestring),
        minimum_c->valuedouble,
        maximum_c->valuedouble,
        max_age_ms->valuedouble
        );

    return cell_temperature_guard_local_var;
end:
    return NULL;

}
