/*
 * actuator_settings.h
 *
 * Available cached settings for one source/sink. Omitted values are unknown or unsupported, not zero. Settings do not imply that output is enabled.
 */

#ifndef _actuator_settings_H_
#define _actuator_settings_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct actuator_settings_t actuator_settings_t;


// Enum REGULATIONMODE for actuator_settings

typedef enum  { labbridge_api_actuator_settings_REGULATIONMODE_NULL = 0, labbridge_api_actuator_settings_REGULATIONMODE_cc, labbridge_api_actuator_settings_REGULATIONMODE_cv, labbridge_api_actuator_settings_REGULATIONMODE_cr, labbridge_api_actuator_settings_REGULATIONMODE_cp, labbridge_api_actuator_settings_REGULATIONMODE_unknown } labbridge_api_actuator_settings_REGULATIONMODE_e;

char* actuator_settings_regulation_mode_ToString(labbridge_api_actuator_settings_REGULATIONMODE_e regulation_mode);

labbridge_api_actuator_settings_REGULATIONMODE_e actuator_settings_regulation_mode_FromString(char* regulation_mode);



typedef struct actuator_settings_t {
    double voltage_v; //numeric
    double current_a; //numeric
    double cutoff_voltage_v; //numeric
    int time_limit_s; //numeric
    labbridge_api_actuator_settings_REGULATIONMODE_e regulation_mode; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} actuator_settings_t;

__attribute__((deprecated)) actuator_settings_t *actuator_settings_create(
    double voltage_v,
    double current_a,
    double cutoff_voltage_v,
    int time_limit_s,
    labbridge_api_actuator_settings_REGULATIONMODE_e regulation_mode
);

void actuator_settings_free(actuator_settings_t *actuator_settings);

actuator_settings_t *actuator_settings_parseFromJSON(cJSON *actuator_settingsJSON);

cJSON *actuator_settings_convertToJSON(actuator_settings_t *actuator_settings);

#endif /* _actuator_settings_H_ */

