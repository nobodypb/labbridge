/*
 * auth_status.h
 *
 * Authentication state for the current transport and browser cookie. Session secrets are private and must not be logged. Sessions last twelve hours, are boot-local and use the node monotonic clock.
 */

#ifndef _auth_status_H_
#define _auth_status_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct auth_status_t auth_status_t;




typedef struct auth_status_t {
    int password_configured; //boolean
    int setup_allowed; //boolean
    int authenticated; //boolean
    char *csrf_token; // string
    int expires_in_ms; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} auth_status_t;

__attribute__((deprecated)) auth_status_t *auth_status_create(
    int password_configured,
    int setup_allowed,
    int authenticated,
    char *csrf_token,
    int expires_in_ms
);

void auth_status_free(auth_status_t *auth_status);

auth_status_t *auth_status_parseFromJSON(cJSON *auth_statusJSON);

cJSON *auth_status_convertToJSON(auth_status_t *auth_status);

#endif /* _auth_status_H_ */

