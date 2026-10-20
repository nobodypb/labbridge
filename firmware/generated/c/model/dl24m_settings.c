#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dl24m_settings.h"



static dl24m_settings_t *dl24m_settings_create_internal(
    double cutoff_voltage_v,
    long time_limit_s,
    long working_brightness,
    long standby_brightness,
    long current_resolution,
    double overvoltage_v,
    double overcurrent_a,
    double overpower_w,
    long overtemperature_c,
    long fan_temperature_c,
    long battery_overtemperature_c,
    long language,
    int mode,
    int standby_time
    ) {
    dl24m_settings_t *dl24m_settings_local_var = malloc(sizeof(dl24m_settings_t));
    if (!dl24m_settings_local_var) {
        return NULL;
    }
    dl24m_settings_local_var->cutoff_voltage_v = cutoff_voltage_v;
    dl24m_settings_local_var->time_limit_s = time_limit_s;
    dl24m_settings_local_var->working_brightness = working_brightness;
    dl24m_settings_local_var->standby_brightness = standby_brightness;
    dl24m_settings_local_var->current_resolution = current_resolution;
    dl24m_settings_local_var->overvoltage_v = overvoltage_v;
    dl24m_settings_local_var->overcurrent_a = overcurrent_a;
    dl24m_settings_local_var->overpower_w = overpower_w;
    dl24m_settings_local_var->overtemperature_c = overtemperature_c;
    dl24m_settings_local_var->fan_temperature_c = fan_temperature_c;
    dl24m_settings_local_var->battery_overtemperature_c = battery_overtemperature_c;
    dl24m_settings_local_var->language = language;
    dl24m_settings_local_var->mode = mode;
    dl24m_settings_local_var->standby_time = standby_time;

    dl24m_settings_local_var->_library_owned = 1;
    return dl24m_settings_local_var;
}

__attribute__((deprecated)) dl24m_settings_t *dl24m_settings_create(
    double cutoff_voltage_v,
    long time_limit_s,
    long working_brightness,
    long standby_brightness,
    long current_resolution,
    double overvoltage_v,
    double overcurrent_a,
    double overpower_w,
    long overtemperature_c,
    long fan_temperature_c,
    long battery_overtemperature_c,
    long language,
    int mode,
    int standby_time
    ) {
    return dl24m_settings_create_internal (
        cutoff_voltage_v,
        time_limit_s,
        working_brightness,
        standby_brightness,
        current_resolution,
        overvoltage_v,
        overcurrent_a,
        overpower_w,
        overtemperature_c,
        fan_temperature_c,
        battery_overtemperature_c,
        language,
        mode,
        standby_time
        );
}

void dl24m_settings_free(dl24m_settings_t *dl24m_settings) {
    if(NULL == dl24m_settings){
        return ;
    }
    if(dl24m_settings->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "dl24m_settings_free");
        return ;
    }
    listEntry_t *listEntry;
    free(dl24m_settings);
}

cJSON *dl24m_settings_convertToJSON(dl24m_settings_t *dl24m_settings) {
    cJSON *item = cJSON_CreateObject();

    // dl24m_settings->cutoff_voltage_v
    if(dl24m_settings->cutoff_voltage_v) {
    if(cJSON_AddNumberToObject(item, "cutoff_voltage_v", dl24m_settings->cutoff_voltage_v) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->time_limit_s
    if(dl24m_settings->time_limit_s) {
    if(cJSON_AddNumberToObject(item, "time_limit_s", dl24m_settings->time_limit_s) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->working_brightness
    if(dl24m_settings->working_brightness) {
    if(cJSON_AddNumberToObject(item, "working_brightness", dl24m_settings->working_brightness) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->standby_brightness
    if(dl24m_settings->standby_brightness) {
    if(cJSON_AddNumberToObject(item, "standby_brightness", dl24m_settings->standby_brightness) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->current_resolution
    if(dl24m_settings->current_resolution) {
    if(cJSON_AddNumberToObject(item, "current_resolution", dl24m_settings->current_resolution) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->overvoltage_v
    if(dl24m_settings->overvoltage_v) {
    if(cJSON_AddNumberToObject(item, "overvoltage_v", dl24m_settings->overvoltage_v) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->overcurrent_a
    if(dl24m_settings->overcurrent_a) {
    if(cJSON_AddNumberToObject(item, "overcurrent_a", dl24m_settings->overcurrent_a) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->overpower_w
    if(dl24m_settings->overpower_w) {
    if(cJSON_AddNumberToObject(item, "overpower_w", dl24m_settings->overpower_w) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->overtemperature_c
    if(dl24m_settings->overtemperature_c) {
    if(cJSON_AddNumberToObject(item, "overtemperature_c", dl24m_settings->overtemperature_c) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->fan_temperature_c
    if(dl24m_settings->fan_temperature_c) {
    if(cJSON_AddNumberToObject(item, "fan_temperature_c", dl24m_settings->fan_temperature_c) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->battery_overtemperature_c
    if(dl24m_settings->battery_overtemperature_c) {
    if(cJSON_AddNumberToObject(item, "battery_overtemperature_c", dl24m_settings->battery_overtemperature_c) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->language
    if(dl24m_settings->language) {
    if(cJSON_AddNumberToObject(item, "language", dl24m_settings->language) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->mode
    if(dl24m_settings->mode) {
    if(cJSON_AddNumberToObject(item, "mode", dl24m_settings->mode) == NULL) {
    goto fail; //Numeric
    }
    }


    // dl24m_settings->standby_time
    if(dl24m_settings->standby_time) {
    if(cJSON_AddNumberToObject(item, "standby_time", dl24m_settings->standby_time) == NULL) {
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

dl24m_settings_t *dl24m_settings_parseFromJSON(cJSON *dl24m_settingsJSON){

    dl24m_settings_t *dl24m_settings_local_var = NULL;

    // dl24m_settings->cutoff_voltage_v
    cJSON *cutoff_voltage_v = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "cutoff_voltage_v");
    if (cJSON_IsNull(cutoff_voltage_v)) {
        cutoff_voltage_v = NULL;
    }
    if (cutoff_voltage_v) { 
    if(!cJSON_IsNumber(cutoff_voltage_v))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->time_limit_s
    cJSON *time_limit_s = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "time_limit_s");
    if (cJSON_IsNull(time_limit_s)) {
        time_limit_s = NULL;
    }
    if (time_limit_s) { 
    if(!cJSON_IsNumber(time_limit_s))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->working_brightness
    cJSON *working_brightness = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "working_brightness");
    if (cJSON_IsNull(working_brightness)) {
        working_brightness = NULL;
    }
    if (working_brightness) { 
    if(!cJSON_IsNumber(working_brightness))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->standby_brightness
    cJSON *standby_brightness = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "standby_brightness");
    if (cJSON_IsNull(standby_brightness)) {
        standby_brightness = NULL;
    }
    if (standby_brightness) { 
    if(!cJSON_IsNumber(standby_brightness))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->current_resolution
    cJSON *current_resolution = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "current_resolution");
    if (cJSON_IsNull(current_resolution)) {
        current_resolution = NULL;
    }
    if (current_resolution) { 
    if(!cJSON_IsNumber(current_resolution))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->overvoltage_v
    cJSON *overvoltage_v = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "overvoltage_v");
    if (cJSON_IsNull(overvoltage_v)) {
        overvoltage_v = NULL;
    }
    if (overvoltage_v) { 
    if(!cJSON_IsNumber(overvoltage_v))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->overcurrent_a
    cJSON *overcurrent_a = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "overcurrent_a");
    if (cJSON_IsNull(overcurrent_a)) {
        overcurrent_a = NULL;
    }
    if (overcurrent_a) { 
    if(!cJSON_IsNumber(overcurrent_a))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->overpower_w
    cJSON *overpower_w = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "overpower_w");
    if (cJSON_IsNull(overpower_w)) {
        overpower_w = NULL;
    }
    if (overpower_w) { 
    if(!cJSON_IsNumber(overpower_w))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->overtemperature_c
    cJSON *overtemperature_c = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "overtemperature_c");
    if (cJSON_IsNull(overtemperature_c)) {
        overtemperature_c = NULL;
    }
    if (overtemperature_c) { 
    if(!cJSON_IsNumber(overtemperature_c))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->fan_temperature_c
    cJSON *fan_temperature_c = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "fan_temperature_c");
    if (cJSON_IsNull(fan_temperature_c)) {
        fan_temperature_c = NULL;
    }
    if (fan_temperature_c) { 
    if(!cJSON_IsNumber(fan_temperature_c))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->battery_overtemperature_c
    cJSON *battery_overtemperature_c = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "battery_overtemperature_c");
    if (cJSON_IsNull(battery_overtemperature_c)) {
        battery_overtemperature_c = NULL;
    }
    if (battery_overtemperature_c) { 
    if(!cJSON_IsNumber(battery_overtemperature_c))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->language
    cJSON *language = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "language");
    if (cJSON_IsNull(language)) {
        language = NULL;
    }
    if (language) { 
    if(!cJSON_IsNumber(language))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->mode
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "mode");
    if (cJSON_IsNull(mode)) {
        mode = NULL;
    }
    if (mode) { 
    if(!cJSON_IsNumber(mode))
    {
    goto end; //Numeric
    }
    }

    // dl24m_settings->standby_time
    cJSON *standby_time = cJSON_GetObjectItemCaseSensitive(dl24m_settingsJSON, "standby_time");
    if (cJSON_IsNull(standby_time)) {
        standby_time = NULL;
    }
    if (standby_time) { 
    if(!cJSON_IsNumber(standby_time))
    {
    goto end; //Numeric
    }
    }


    dl24m_settings_local_var = dl24m_settings_create_internal (
        cutoff_voltage_v ? cutoff_voltage_v->valuedouble : 0,
        time_limit_s ? time_limit_s->valuedouble : 0,
        working_brightness ? working_brightness->valuedouble : 0,
        standby_brightness ? standby_brightness->valuedouble : 0,
        current_resolution ? current_resolution->valuedouble : 0,
        overvoltage_v ? overvoltage_v->valuedouble : 0,
        overcurrent_a ? overcurrent_a->valuedouble : 0,
        overpower_w ? overpower_w->valuedouble : 0,
        overtemperature_c ? overtemperature_c->valuedouble : 0,
        fan_temperature_c ? fan_temperature_c->valuedouble : 0,
        battery_overtemperature_c ? battery_overtemperature_c->valuedouble : 0,
        language ? language->valuedouble : 0,
        mode ? mode->valuedouble : 0,
        standby_time ? standby_time->valuedouble : 0
        );

    return dl24m_settings_local_var;
end:
    return NULL;

}
