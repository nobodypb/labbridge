/*
 * output_request.h
 *
 * 
 */

#ifndef _output_request_H_
#define _output_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct output_request_t output_request_t;




typedef struct output_request_t {
    int enabled; //boolean

    int _library_owned; // Is the library responsible for freeing this object?
} output_request_t;

__attribute__((deprecated)) output_request_t *output_request_create(
    int enabled
);

void output_request_free(output_request_t *output_request);

output_request_t *output_request_parseFromJSON(cJSON *output_requestJSON);

cJSON *output_request_convertToJSON(output_request_t *output_request);

#endif /* _output_request_H_ */

