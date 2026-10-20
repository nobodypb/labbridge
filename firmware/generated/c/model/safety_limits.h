/*
 * safety_limits.h
 *
 * Bridge-local operating envelope for an actuator. The platform requires positive voltage/current/power limits for actuators; read-only sensors may omit limits. These are additional limits, not a replacement for instrument protection or a complete battery profile.
 */

#ifndef _safety_limits_H_
#define _safety_limits_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct safety_limits_t safety_limits_t;




typedef struct safety_limits_t {
    double voltage_v; //numeric
    double current_a; //numeric
    double power_w; //numeric
    long max_age_ms; //numeric
    long max_duration_s; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} safety_limits_t;

__attribute__((deprecated)) safety_limits_t *safety_limits_create(
    double voltage_v,
    double current_a,
    double power_w,
    long max_age_ms,
    long max_duration_s
);

void safety_limits_free(safety_limits_t *safety_limits);

safety_limits_t *safety_limits_parseFromJSON(cJSON *safety_limitsJSON);

cJSON *safety_limits_convertToJSON(safety_limits_t *safety_limits);

#endif /* _safety_limits_H_ */

