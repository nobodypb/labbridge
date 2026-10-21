/*
 * dl24m_mode_request.h
 *
 * Select a native mode without enabling output or modifying that bank target. Confirmed OFF, live original mode/target snapshot, persistent rollback and checked readback required.
 */

#ifndef _dl24m_mode_request_H_
#define _dl24m_mode_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct dl24m_mode_request_t dl24m_mode_request_t;


// Enum MODE for dl24m_mode_request

typedef enum  { labbridge_api_dl24m_mode_request_MODE_NULL = 0, labbridge_api_dl24m_mode_request_MODE_cc, labbridge_api_dl24m_mode_request_MODE_cv, labbridge_api_dl24m_mode_request_MODE_cr, labbridge_api_dl24m_mode_request_MODE_cp } labbridge_api_dl24m_mode_request_MODE_e;

char* dl24m_mode_request_mode_ToString(labbridge_api_dl24m_mode_request_MODE_e mode);

labbridge_api_dl24m_mode_request_MODE_e dl24m_mode_request_mode_FromString(char* mode);



typedef struct dl24m_mode_request_t {
    labbridge_api_dl24m_mode_request_MODE_e mode; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} dl24m_mode_request_t;

__attribute__((deprecated)) dl24m_mode_request_t *dl24m_mode_request_create(
    labbridge_api_dl24m_mode_request_MODE_e mode
);

void dl24m_mode_request_free(dl24m_mode_request_t *dl24m_mode_request);

dl24m_mode_request_t *dl24m_mode_request_parseFromJSON(cJSON *dl24m_mode_requestJSON);

cJSON *dl24m_mode_request_convertToJSON(dl24m_mode_request_t *dl24m_mode_request);

#endif /* _dl24m_mode_request_H_ */

