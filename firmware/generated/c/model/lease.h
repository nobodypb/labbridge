/*
 * lease.h
 *
 * 
 */

#ifndef _lease_H_
#define _lease_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct lease_t lease_t;


// Enum STATE for lease

typedef enum  { labbridge_api_lease_STATE_NULL = 0, labbridge_api_lease_STATE_active, labbridge_api_lease_STATE_stopped, labbridge_api_lease_STATE_expired } labbridge_api_lease_STATE_e;

char* lease_state_ToString(labbridge_api_lease_STATE_e state);

labbridge_api_lease_STATE_e lease_state_FromString(char* state);



typedef struct lease_t {
    char *id; // string
    list_t *device_ids; //primitive container
    long expires_at_ms; //numeric
    labbridge_api_lease_STATE_e state; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} lease_t;

__attribute__((deprecated)) lease_t *lease_create(
    char *id,
    list_t *device_ids,
    long expires_at_ms,
    labbridge_api_lease_STATE_e state
);

void lease_free(lease_t *lease);

lease_t *lease_parseFromJSON(cJSON *leaseJSON);

cJSON *lease_convertToJSON(lease_t *lease);

#endif /* _lease_H_ */

