/*
 * rd_provision_advance.h
 *
 * Acknowledge the target PSU display and advance this boot-local provisioning session. Repeating the same session/expected_phase is idempotent and never skips a phase.
 */

#ifndef _rd_provision_advance_H_
#define _rd_provision_advance_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct rd_provision_advance_t rd_provision_advance_t;


// Enum EXPECTEDPHASE for rd_provision_advance

typedef enum  { labbridge_api_rd_provision_advance_EXPECTEDPHASE_NULL = 0, labbridge_api_rd_provision_advance_EXPECTEDPHASE_server_address, labbridge_api_rd_provision_advance_EXPECTEDPHASE_wifi } labbridge_api_rd_provision_advance_EXPECTEDPHASE_e;

char* rd_provision_advance_expected_phase_ToString(labbridge_api_rd_provision_advance_EXPECTEDPHASE_e expected_phase);

labbridge_api_rd_provision_advance_EXPECTEDPHASE_e rd_provision_advance_expected_phase_FromString(char* expected_phase);



typedef struct rd_provision_advance_t {
    char *session_id; // string
    labbridge_api_rd_provision_advance_EXPECTEDPHASE_e expected_phase; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} rd_provision_advance_t;

__attribute__((deprecated)) rd_provision_advance_t *rd_provision_advance_create(
    char *session_id,
    labbridge_api_rd_provision_advance_EXPECTEDPHASE_e expected_phase
);

void rd_provision_advance_free(rd_provision_advance_t *rd_provision_advance);

rd_provision_advance_t *rd_provision_advance_parseFromJSON(cJSON *rd_provision_advanceJSON);

cJSON *rd_provision_advance_convertToJSON(rd_provision_advance_t *rd_provision_advance);

#endif /* _rd_provision_advance_H_ */

