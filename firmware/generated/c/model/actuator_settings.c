#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "actuator_settings.h"


char* actuator_settings_regulation_mode_ToString(labbridge_api_actuator_settings_REGULATIONMODE_e regulation_mode) {
    char* regulation_modeArray[] =  { "NULL", "cc", "cv", "unknown" };
    return regulation_modeArray[regulation_mode];
}

labbridge_api_actuator_settings_REGULATIONMODE_e actuator_settings_regulation_mode_FromString(char* regulation_mode){
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

static actuator_settings_t *actuator_settings_create_internal(
    double voltage_v,
    double current_a,
    double cutoff_voltage_v,
    int time_limit_s,
    labbridge_api_actuator_settings_REGULATIONMODE_e regulation_mode
    ) {
    actuator_settings_t *actuator_settings_local_var = malloc(sizeof(actuator_settings_t));
    if (!actuator_settings_local_var) {
        return NULL;
    }
    actuator_settings_local_var->voltage_v = voltage_v;
    actuator_settings_local_var->current_a = current_a;
    actuator_settings_local_var->cutoff_voltage_v = cutoff_voltage_v;
    actuator_settings_local_var->time_limit_s = time_limit_s;
    actuator_settings_local_var->regulation_mode = regulation_mode;

    actuator_settings_local_var->_library_owned = 1;
    return actuator_settings_local_var;
}

__attribute__((deprecated)) actuator_settings_t *actuator_settings_create(
    double voltage_v,
    double current_a,
    double cutoff_voltage_v,
    int time_limit_s,
    labbridge_api_actuator_settings_REGULATIONMODE_e regulation_mode
    ) {
    return actuator_settings_create_internal (
        voltage_v,
        current_a,
        cutoff_voltage_v,
        time_limit_s,
        regulation_mode
        );
}

void actuator_settings_free(actuator_settings_t *actuator_settings) {
    if(NULL == actuator_settings){
        return ;
    }
    if(actuator_settings->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "actuator_settings_free");
        return ;
    }
    listEntry_t *listEntry;
    free(actuator_settings);
}

cJSON *actuator_settings_convertToJSON(actuator_settings_t *actuator_settings) {
    cJSON *item = cJSON_CreateObject();

    // actuator_settings->voltage_v
    if(actuator_settings->voltage_v) {
    if(cJSON_AddNumberToObject(item, "voltage_v", actuator_settings->voltage_v) == NULL) {
    goto fail; //Numeric
    }
    }


    // actuator_settings->current_a
    if(actuator_settings->current_a) {
    if(cJSON_AddNumberToObject(item, "current_a", actuator_settings->current_a) == NULL) {
    goto fail; //Numeric
    }
    }


    // actuator_settings->cutoff_voltage_v
    if(actuator_settings->cutoff_voltage_v) {
    if(cJSON_AddNumberToObject(item, "cutoff_voltage_v", actuator_settings->cutoff_voltage_v) == NULL) {
    goto fail; //Numeric
    }
    }


    // actuator_settings->time_limit_s
    if(actuator_settings->time_limit_s) {
    if(cJSON_AddNumberToObject(item, "time_limit_s", actuator_settings->time_limit_s) == NULL) {
    goto fail; //Numeric
    }
    }


    // actuator_settings->regulation_mode
    if(actuator_settings->regulation_mode != labbridge_api_actuator_settings_REGULATIONMODE_NULL) {
    if(cJSON_AddStringToObject(item, "regulation_mode", actuator_settings_regulation_mode_ToString(actuator_settings->regulation_mode)) == NULL)
    {
    goto fail; //Enum
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

actuator_settings_t *actuator_settings_parseFromJSON(cJSON *actuator_settingsJSON){

    actuator_settings_t *actuator_settings_local_var = NULL;

    // actuator_settings->voltage_v
    cJSON *voltage_v = cJSON_GetObjectItemCaseSensitive(actuator_settingsJSON, "voltage_v");
    if (cJSON_IsNull(voltage_v)) {
        voltage_v = NULL;
    }
    if (voltage_v) { 
    if(!cJSON_IsNumber(voltage_v))
    {
    goto end; //Numeric
    }
    }

    // actuator_settings->current_a
    cJSON *current_a = cJSON_GetObjectItemCaseSensitive(actuator_settingsJSON, "current_a");
    if (cJSON_IsNull(current_a)) {
        current_a = NULL;
    }
    if (current_a) { 
    if(!cJSON_IsNumber(current_a))
    {
    goto end; //Numeric
    }
    }

    // actuator_settings->cutoff_voltage_v
    cJSON *cutoff_voltage_v = cJSON_GetObjectItemCaseSensitive(actuator_settingsJSON, "cutoff_voltage_v");
    if (cJSON_IsNull(cutoff_voltage_v)) {
        cutoff_voltage_v = NULL;
    }
    if (cutoff_voltage_v) { 
    if(!cJSON_IsNumber(cutoff_voltage_v))
    {
    goto end; //Numeric
    }
    }

    // actuator_settings->time_limit_s
    cJSON *time_limit_s = cJSON_GetObjectItemCaseSensitive(actuator_settingsJSON, "time_limit_s");
    if (cJSON_IsNull(time_limit_s)) {
        time_limit_s = NULL;
    }
    if (time_limit_s) { 
    if(!cJSON_IsNumber(time_limit_s))
    {
    goto end; //Numeric
    }
    }

    // actuator_settings->regulation_mode
    cJSON *regulation_mode = cJSON_GetObjectItemCaseSensitive(actuator_settingsJSON, "regulation_mode");
    if (cJSON_IsNull(regulation_mode)) {
        regulation_mode = NULL;
    }
    labbridge_api_actuator_settings_REGULATIONMODE_e regulation_modeVariable;
    if (regulation_mode) { 
    if(!cJSON_IsString(regulation_mode))
    {
    goto end; //Enum
    }
    regulation_modeVariable = actuator_settings_regulation_mode_FromString(regulation_mode->valuestring);
    }


    actuator_settings_local_var = actuator_settings_create_internal (
        voltage_v ? voltage_v->valuedouble : 0,
        current_a ? current_a->valuedouble : 0,
        cutoff_voltage_v ? cutoff_voltage_v->valuedouble : 0,
        time_limit_s ? time_limit_s->valuedouble : 0,
        regulation_mode ? regulation_modeVariable : labbridge_api_actuator_settings_REGULATIONMODE_NULL
        );

    return actuator_settings_local_var;
end:
    return NULL;

}
