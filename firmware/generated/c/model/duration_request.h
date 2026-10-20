/*
 * duration_request.h
 *
 * Instrument run timer in seconds, separate from the bridge-local activation deadline. Zero disables the instrument timer; hardware granularity still applies.
 */

#ifndef _duration_request_H_
#define _duration_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct duration_request_t duration_request_t;




typedef struct duration_request_t {
    int time_limit_s; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} duration_request_t;

__attribute__((deprecated)) duration_request_t *duration_request_create(
    int time_limit_s
);

void duration_request_free(duration_request_t *duration_request);

duration_request_t *duration_request_parseFromJSON(cJSON *duration_requestJSON);

cJSON *duration_request_convertToJSON(duration_request_t *duration_request);

#endif /* _duration_request_H_ */

