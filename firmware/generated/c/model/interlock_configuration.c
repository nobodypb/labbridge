#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "interlock_configuration.h"



static interlock_configuration_t *interlock_configuration_create_internal(
    int active_high,
    int feedback_active_high,
    int break_ms
    ) {
    interlock_configuration_t *interlock_configuration_local_var = malloc(sizeof(interlock_configuration_t));
    if (!interlock_configuration_local_var) {
        return NULL;
    }
    interlock_configuration_local_var->active_high = active_high;
    interlock_configuration_local_var->feedback_active_high = feedback_active_high;
    interlock_configuration_local_var->break_ms = break_ms;

    interlock_configuration_local_var->_library_owned = 1;
    return interlock_configuration_local_var;
}

__attribute__((deprecated)) interlock_configuration_t *interlock_configuration_create(
    int active_high,
    int feedback_active_high,
    int break_ms
    ) {
    return interlock_configuration_create_internal (
        active_high,
        feedback_active_high,
        break_ms
        );
}

void interlock_configuration_free(interlock_configuration_t *interlock_configuration) {
    if(NULL == interlock_configuration){
        return ;
    }
    if(interlock_configuration->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "interlock_configuration_free");
        return ;
    }
    listEntry_t *listEntry;
    free(interlock_configuration);
}

cJSON *interlock_configuration_convertToJSON(interlock_configuration_t *interlock_configuration) {
    cJSON *item = cJSON_CreateObject();

    // interlock_configuration->active_high
    if (!interlock_configuration->active_high) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "active_high", interlock_configuration->active_high) == NULL) {
    goto fail; //Bool
    }


    // interlock_configuration->feedback_active_high
    if (!interlock_configuration->feedback_active_high) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "feedback_active_high", interlock_configuration->feedback_active_high) == NULL) {
    goto fail; //Bool
    }


    // interlock_configuration->break_ms
    if (!interlock_configuration->break_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "break_ms", interlock_configuration->break_ms) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

interlock_configuration_t *interlock_configuration_parseFromJSON(cJSON *interlock_configurationJSON){

    interlock_configuration_t *interlock_configuration_local_var = NULL;

    // interlock_configuration->active_high
    cJSON *active_high = cJSON_GetObjectItemCaseSensitive(interlock_configurationJSON, "active_high");
    if (cJSON_IsNull(active_high)) {
        active_high = NULL;
    }
    if (!active_high) {
        goto end;
    }

    
    if(!cJSON_IsBool(active_high))
    {
    goto end; //Bool
    }

    // interlock_configuration->feedback_active_high
    cJSON *feedback_active_high = cJSON_GetObjectItemCaseSensitive(interlock_configurationJSON, "feedback_active_high");
    if (cJSON_IsNull(feedback_active_high)) {
        feedback_active_high = NULL;
    }
    if (!feedback_active_high) {
        goto end;
    }

    
    if(!cJSON_IsBool(feedback_active_high))
    {
    goto end; //Bool
    }

    // interlock_configuration->break_ms
    cJSON *break_ms = cJSON_GetObjectItemCaseSensitive(interlock_configurationJSON, "break_ms");
    if (cJSON_IsNull(break_ms)) {
        break_ms = NULL;
    }
    if (!break_ms) {
        goto end;
    }

    
    if(!cJSON_IsNumber(break_ms))
    {
    goto end; //Numeric
    }


    interlock_configuration_local_var = interlock_configuration_create_internal (
        active_high->valueint,
        feedback_active_high->valueint,
        break_ms->valuedouble
        );

    return interlock_configuration_local_var;
end:
    return NULL;

}
