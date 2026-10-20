#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ina228_configuration.h"



static ina228_configuration_t *ina228_configuration_create_internal(
    double shunt_ohms,
    double max_current_a,
    int adc_range
    ) {
    ina228_configuration_t *ina228_configuration_local_var = malloc(sizeof(ina228_configuration_t));
    if (!ina228_configuration_local_var) {
        return NULL;
    }
    ina228_configuration_local_var->shunt_ohms = shunt_ohms;
    ina228_configuration_local_var->max_current_a = max_current_a;
    ina228_configuration_local_var->adc_range = adc_range;

    ina228_configuration_local_var->_library_owned = 1;
    return ina228_configuration_local_var;
}

__attribute__((deprecated)) ina228_configuration_t *ina228_configuration_create(
    double shunt_ohms,
    double max_current_a,
    int adc_range
    ) {
    return ina228_configuration_create_internal (
        shunt_ohms,
        max_current_a,
        adc_range
        );
}

void ina228_configuration_free(ina228_configuration_t *ina228_configuration) {
    if(NULL == ina228_configuration){
        return ;
    }
    if(ina228_configuration->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "ina228_configuration_free");
        return ;
    }
    listEntry_t *listEntry;
    free(ina228_configuration);
}

cJSON *ina228_configuration_convertToJSON(ina228_configuration_t *ina228_configuration) {
    cJSON *item = cJSON_CreateObject();

    // ina228_configuration->shunt_ohms
    if (!ina228_configuration->shunt_ohms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "shunt_ohms", ina228_configuration->shunt_ohms) == NULL) {
    goto fail; //Numeric
    }


    // ina228_configuration->max_current_a
    if (!ina228_configuration->max_current_a) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "max_current_a", ina228_configuration->max_current_a) == NULL) {
    goto fail; //Numeric
    }


    // ina228_configuration->adc_range
    if (!ina228_configuration->adc_range) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "adc_range", ina228_configuration->adc_range) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

ina228_configuration_t *ina228_configuration_parseFromJSON(cJSON *ina228_configurationJSON){

    ina228_configuration_t *ina228_configuration_local_var = NULL;

    // ina228_configuration->shunt_ohms
    cJSON *shunt_ohms = cJSON_GetObjectItemCaseSensitive(ina228_configurationJSON, "shunt_ohms");
    if (cJSON_IsNull(shunt_ohms)) {
        shunt_ohms = NULL;
    }
    if (!shunt_ohms) {
        goto end;
    }

    
    if(!cJSON_IsNumber(shunt_ohms))
    {
    goto end; //Numeric
    }

    // ina228_configuration->max_current_a
    cJSON *max_current_a = cJSON_GetObjectItemCaseSensitive(ina228_configurationJSON, "max_current_a");
    if (cJSON_IsNull(max_current_a)) {
        max_current_a = NULL;
    }
    if (!max_current_a) {
        goto end;
    }

    
    if(!cJSON_IsNumber(max_current_a))
    {
    goto end; //Numeric
    }

    // ina228_configuration->adc_range
    cJSON *adc_range = cJSON_GetObjectItemCaseSensitive(ina228_configurationJSON, "adc_range");
    if (cJSON_IsNull(adc_range)) {
        adc_range = NULL;
    }
    if (!adc_range) {
        goto end;
    }

    
    if(!cJSON_IsNumber(adc_range))
    {
    goto end; //Numeric
    }


    ina228_configuration_local_var = ina228_configuration_create_internal (
        shunt_ohms->valuedouble,
        max_current_a->valuedouble,
        adc_range->valuedouble
        );

    return ina228_configuration_local_var;
end:
    return NULL;

}
