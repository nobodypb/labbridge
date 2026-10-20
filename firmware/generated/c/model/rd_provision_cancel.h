/*
 * rd_provision_cancel.h
 *
 * Stop transmission for this provisioning session. Previously delivered network settings cannot be undone by cancellation; no output command is sent.
 */

#ifndef _rd_provision_cancel_H_
#define _rd_provision_cancel_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct rd_provision_cancel_t rd_provision_cancel_t;




typedef struct rd_provision_cancel_t {
    char *session_id; // string

    int _library_owned; // Is the library responsible for freeing this object?
} rd_provision_cancel_t;

__attribute__((deprecated)) rd_provision_cancel_t *rd_provision_cancel_create(
    char *session_id
);

void rd_provision_cancel_free(rd_provision_cancel_t *rd_provision_cancel);

rd_provision_cancel_t *rd_provision_cancel_parseFromJSON(cJSON *rd_provision_cancelJSON);

cJSON *rd_provision_cancel_convertToJSON(rd_provision_cancel_t *rd_provision_cancel);

#endif /* _rd_provision_cancel_H_ */

