/*
 * dl24m_target_request.h
 *
 * Set the selected mode target while OFF. Mode and unit must agree. CC 0.001 A; CV 0.01 V; CR 0.1 Ohm; CP 0.1 W. Configured local limits and activation prerequisites still apply.
 */

#ifndef _dl24m_target_request_H_
#define _dl24m_target_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct dl24m_target_request_t dl24m_target_request_t;


// Enum MODE for dl24m_target_request

typedef enum  { labbridge_api_dl24m_target_request_MODE_NULL = 0, labbridge_api_dl24m_target_request_MODE_cc, labbridge_api_dl24m_target_request_MODE_cv, labbridge_api_dl24m_target_request_MODE_cr, labbridge_api_dl24m_target_request_MODE_cp } labbridge_api_dl24m_target_request_MODE_e;

char* dl24m_target_request_mode_ToString(labbridge_api_dl24m_target_request_MODE_e mode);

labbridge_api_dl24m_target_request_MODE_e dl24m_target_request_mode_FromString(char* mode);

// Enum UNIT for dl24m_target_request

typedef enum  { labbridge_api_dl24m_target_request_UNIT_NULL = 0, labbridge_api_dl24m_target_request_UNIT_A, labbridge_api_dl24m_target_request_UNIT_V, labbridge_api_dl24m_target_request_UNIT_Ohm, labbridge_api_dl24m_target_request_UNIT_W } labbridge_api_dl24m_target_request_UNIT_e;

char* dl24m_target_request_unit_ToString(labbridge_api_dl24m_target_request_UNIT_e unit);

labbridge_api_dl24m_target_request_UNIT_e dl24m_target_request_unit_FromString(char* unit);



typedef struct dl24m_target_request_t {
    labbridge_api_dl24m_target_request_MODE_e mode; //enum
    double value; //numeric
    labbridge_api_dl24m_target_request_UNIT_e unit; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} dl24m_target_request_t;

__attribute__((deprecated)) dl24m_target_request_t *dl24m_target_request_create(
    labbridge_api_dl24m_target_request_MODE_e mode,
    double value,
    labbridge_api_dl24m_target_request_UNIT_e unit
);

void dl24m_target_request_free(dl24m_target_request_t *dl24m_target_request);

dl24m_target_request_t *dl24m_target_request_parseFromJSON(cJSON *dl24m_target_requestJSON);

cJSON *dl24m_target_request_convertToJSON(dl24m_target_request_t *dl24m_target_request);

#endif /* _dl24m_target_request_H_ */

