/*
 * stop_request.h
 *
 * 
 */

#ifndef _stop_request_H_
#define _stop_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct stop_request_t stop_request_t;




typedef struct stop_request_t {
    char *reason; // string

    int _library_owned; // Is the library responsible for freeing this object?
} stop_request_t;

__attribute__((deprecated)) stop_request_t *stop_request_create(
    char *reason
);

void stop_request_free(stop_request_t *stop_request);

stop_request_t *stop_request_parseFromJSON(cJSON *stop_requestJSON);

cJSON *stop_request_convertToJSON(stop_request_t *stop_request);

#endif /* _stop_request_H_ */

