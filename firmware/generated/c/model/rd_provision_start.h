/*
 * rd_provision_start.h
 *
 * Begin two-stage ESPTouch provisioning from the ESP AP. Instruments must be stopped/unleased; no actuation is performed. The operator confirms that the target PSU output is OFF.
 */

#ifndef _rd_provision_start_H_
#define _rd_provision_start_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct rd_provision_start_t rd_provision_start_t;


// Enum PROFILE for rd_provision_start

typedef enum  { labbridge_api_rd_provision_start_PROFILE_NULL = 0, labbridge_api_rd_provision_start_PROFILE_pc_v22, labbridge_api_rd_provision_start_PROFILE_pc_v22_target_ssid } labbridge_api_rd_provision_start_PROFILE_e;

char* rd_provision_start_profile_ToString(labbridge_api_rd_provision_start_PROFILE_e profile);

labbridge_api_rd_provision_start_PROFILE_e rd_provision_start_profile_FromString(char* profile);



typedef struct rd_provision_start_t {
    int confirm_outputs_off; //boolean
    labbridge_api_rd_provision_start_PROFILE_e profile; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} rd_provision_start_t;

__attribute__((deprecated)) rd_provision_start_t *rd_provision_start_create(
    int confirm_outputs_off,
    labbridge_api_rd_provision_start_PROFILE_e profile
);

void rd_provision_start_free(rd_provision_start_t *rd_provision_start);

rd_provision_start_t *rd_provision_start_parseFromJSON(cJSON *rd_provision_startJSON);

cJSON *rd_provision_start_convertToJSON(rd_provision_start_t *rd_provision_start);

#endif /* _rd_provision_start_H_ */

