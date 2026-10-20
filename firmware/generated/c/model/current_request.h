/*
 * current_request.h
 *
 * 
 */

#ifndef _current_request_H_
#define _current_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct current_request_t current_request_t;




typedef struct current_request_t {
    double current_a; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} current_request_t;

__attribute__((deprecated)) current_request_t *current_request_create(
    double current_a
);

void current_request_free(current_request_t *current_request);

current_request_t *current_request_parseFromJSON(cJSON *current_requestJSON);

cJSON *current_request_convertToJSON(current_request_t *current_request);

#endif /* _current_request_H_ */

