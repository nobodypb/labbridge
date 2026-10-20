/*
 * voltage_request.h
 *
 * Voltage target in V. Driver precision, capabilities and configured voltage/power limits may reject a value within the generic schema range.
 */

#ifndef _voltage_request_H_
#define _voltage_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct voltage_request_t voltage_request_t;




typedef struct voltage_request_t {
    double voltage_v; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} voltage_request_t;

__attribute__((deprecated)) voltage_request_t *voltage_request_create(
    double voltage_v
);

void voltage_request_free(voltage_request_t *voltage_request);

voltage_request_t *voltage_request_parseFromJSON(cJSON *voltage_requestJSON);

cJSON *voltage_request_convertToJSON(voltage_request_t *voltage_request);

#endif /* _voltage_request_H_ */

