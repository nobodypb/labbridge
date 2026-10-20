#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "safety_limits.h"



static safety_limits_t *safety_limits_create_internal(
    double voltage_v,
    double current_a,
    double power_w,
    long max_age_ms,
    long max_duration_s
    ) {
    safety_limits_t *safety_limits_local_var = malloc(sizeof(safety_limits_t));
    if (!safety_limits_local_var) {
        return NULL;
    }
    safety_limits_local_var->voltage_v = voltage_v;
    safety_limits_local_var->current_a = current_a;
    safety_limits_local_var->power_w = power_w;
    safety_limits_local_var->max_age_ms = max_age_ms;
    safety_limits_local_var->max_duration_s = max_duration_s;

    safety_limits_local_var->_library_owned = 1;
    return safety_limits_local_var;
}

__attribute__((deprecated)) safety_limits_t *safety_limits_create(
    double voltage_v,
    double current_a,
    double power_w,
    long max_age_ms,
    long max_duration_s
    ) {
    return safety_limits_create_internal (
        voltage_v,
        current_a,
        power_w,
        max_age_ms,
        max_duration_s
        );
}

void safety_limits_free(safety_limits_t *safety_limits) {
    if(NULL == safety_limits){
        return ;
    }
    if(safety_limits->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "safety_limits_free");
        return ;
    }
    listEntry_t *listEntry;
    free(safety_limits);
}

cJSON *safety_limits_convertToJSON(safety_limits_t *safety_limits) {
    cJSON *item = cJSON_CreateObject();

    // safety_limits->voltage_v
    if (!safety_limits->voltage_v) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "voltage_v", safety_limits->voltage_v) == NULL) {
    goto fail; //Numeric
    }


    // safety_limits->current_a
    if (!safety_limits->current_a) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "current_a", safety_limits->current_a) == NULL) {
    goto fail; //Numeric
    }


    // safety_limits->power_w
    if (!safety_limits->power_w) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "power_w", safety_limits->power_w) == NULL) {
    goto fail; //Numeric
    }


    // safety_limits->max_age_ms
    if (!safety_limits->max_age_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "max_age_ms", safety_limits->max_age_ms) == NULL) {
    goto fail; //Numeric
    }


    // safety_limits->max_duration_s
    if(safety_limits->max_duration_s) {
    if(cJSON_AddNumberToObject(item, "max_duration_s", safety_limits->max_duration_s) == NULL) {
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

safety_limits_t *safety_limits_parseFromJSON(cJSON *safety_limitsJSON){

    safety_limits_t *safety_limits_local_var = NULL;

    // safety_limits->voltage_v
    cJSON *voltage_v = cJSON_GetObjectItemCaseSensitive(safety_limitsJSON, "voltage_v");
    if (cJSON_IsNull(voltage_v)) {
        voltage_v = NULL;
    }
    if (!voltage_v) {
        goto end;
    }

    
    if(!cJSON_IsNumber(voltage_v))
    {
    goto end; //Numeric
    }

    // safety_limits->current_a
    cJSON *current_a = cJSON_GetObjectItemCaseSensitive(safety_limitsJSON, "current_a");
    if (cJSON_IsNull(current_a)) {
        current_a = NULL;
    }
    if (!current_a) {
        goto end;
    }

    
    if(!cJSON_IsNumber(current_a))
    {
    goto end; //Numeric
    }

    // safety_limits->power_w
    cJSON *power_w = cJSON_GetObjectItemCaseSensitive(safety_limitsJSON, "power_w");
    if (cJSON_IsNull(power_w)) {
        power_w = NULL;
    }
    if (!power_w) {
        goto end;
    }

    
    if(!cJSON_IsNumber(power_w))
    {
    goto end; //Numeric
    }

    // safety_limits->max_age_ms
    cJSON *max_age_ms = cJSON_GetObjectItemCaseSensitive(safety_limitsJSON, "max_age_ms");
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

    // safety_limits->max_duration_s
    cJSON *max_duration_s = cJSON_GetObjectItemCaseSensitive(safety_limitsJSON, "max_duration_s");
    if (cJSON_IsNull(max_duration_s)) {
        max_duration_s = NULL;
    }
    if (max_duration_s) { 
    if(!cJSON_IsNumber(max_duration_s))
    {
    goto end; //Numeric
    }
    }


    safety_limits_local_var = safety_limits_create_internal (
        voltage_v->valuedouble,
        current_a->valuedouble,
        power_w->valuedouble,
        max_age_ms->valuedouble,
        max_duration_s ? max_duration_s->valuedouble : 0
        );

    return safety_limits_local_var;
end:
    return NULL;

}
