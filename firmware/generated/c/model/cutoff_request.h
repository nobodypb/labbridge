/*
 * cutoff_request.h
 *
 * 
 */

#ifndef _cutoff_request_H_
#define _cutoff_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct cutoff_request_t cutoff_request_t;




typedef struct cutoff_request_t {
    double cutoff_voltage_v; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} cutoff_request_t;

__attribute__((deprecated)) cutoff_request_t *cutoff_request_create(
    double cutoff_voltage_v
);

void cutoff_request_free(cutoff_request_t *cutoff_request);

cutoff_request_t *cutoff_request_parseFromJSON(cJSON *cutoff_requestJSON);

cJSON *cutoff_request_convertToJSON(cutoff_request_t *cutoff_request);

#endif /* _cutoff_request_H_ */

