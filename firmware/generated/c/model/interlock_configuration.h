/*
 * interlock_configuration.h
 *
 * 
 */

#ifndef _interlock_configuration_H_
#define _interlock_configuration_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct interlock_configuration_t interlock_configuration_t;




typedef struct interlock_configuration_t {
    int active_high; //boolean
    int feedback_active_high; //boolean
    int break_ms; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} interlock_configuration_t;

__attribute__((deprecated)) interlock_configuration_t *interlock_configuration_create(
    int active_high,
    int feedback_active_high,
    int break_ms
);

void interlock_configuration_free(interlock_configuration_t *interlock_configuration);

interlock_configuration_t *interlock_configuration_parseFromJSON(cJSON *interlock_configurationJSON);

cJSON *interlock_configuration_convertToJSON(interlock_configuration_t *interlock_configuration);

#endif /* _interlock_configuration_H_ */

