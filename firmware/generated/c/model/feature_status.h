/*
 * feature_status.h
 *
 * Per-device evidence and prerequisites. Unverified is not enabled merely because a wire command is known.
 */

#ifndef _feature_status_H_
#define _feature_status_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct feature_status_t feature_status_t;


// Enum STATUS for feature_status

typedef enum  { labbridge_api_feature_status_STATUS_NULL = 0, labbridge_api_feature_status_STATUS_supported, labbridge_api_feature_status_STATUS_unverified, labbridge_api_feature_status_STATUS_unavailable } labbridge_api_feature_status_STATUS_e;

char* feature_status_status_ToString(labbridge_api_feature_status_STATUS_e status);

labbridge_api_feature_status_STATUS_e feature_status_status_FromString(char* status);



typedef struct feature_status_t {
    char *id; // string
    labbridge_api_feature_status_STATUS_e status; //enum
    char *reason; // string
    char *unit; // string
    char *operation; // string
    int requires_off; //boolean
    int requires_lease; //boolean
    char *evidence; // string

    int _library_owned; // Is the library responsible for freeing this object?
} feature_status_t;

__attribute__((deprecated)) feature_status_t *feature_status_create(
    char *id,
    labbridge_api_feature_status_STATUS_e status,
    char *reason,
    char *unit,
    char *operation,
    int requires_off,
    int requires_lease,
    char *evidence
);

void feature_status_free(feature_status_t *feature_status);

feature_status_t *feature_status_parseFromJSON(cJSON *feature_statusJSON);

cJSON *feature_status_convertToJSON(feature_status_t *feature_status);

#endif /* _feature_status_H_ */

