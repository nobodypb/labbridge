/*
 * lease_request.h
 *
 * Exclusive reservation of physical devices on one node. Reserve each physical device once, even when controlling several of its components.
 */

#ifndef _lease_request_H_
#define _lease_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct lease_request_t lease_request_t;




typedef struct lease_request_t {
    list_t *device_ids; //primitive container
    long ttl_ms; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} lease_request_t;

__attribute__((deprecated)) lease_request_t *lease_request_create(
    list_t *device_ids,
    long ttl_ms
);

void lease_request_free(lease_request_t *lease_request);

lease_request_t *lease_request_parseFromJSON(cJSON *lease_requestJSON);

cJSON *lease_request_convertToJSON(lease_request_t *lease_request);

#endif /* _lease_request_H_ */

