/*
 * dl24m_bank_target_request.h
 *
 * Inactive bank target in raw register units. This interface does not claim that the corresponding active CV/CR/CP behavior or scales are commissioned.
 */

#ifndef _dl24m_bank_target_request_H_
#define _dl24m_bank_target_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct dl24m_bank_target_request_t dl24m_bank_target_request_t;


// Enum BANK for dl24m_bank_target_request

typedef enum  { labbridge_api_dl24m_bank_target_request_BANK_NULL = 0, labbridge_api_dl24m_bank_target_request_BANK__1, labbridge_api_dl24m_bank_target_request_BANK__2, labbridge_api_dl24m_bank_target_request_BANK__3 } labbridge_api_dl24m_bank_target_request_BANK_e;

char* dl24m_bank_target_request_bank_ToString(labbridge_api_dl24m_bank_target_request_BANK_e bank);

labbridge_api_dl24m_bank_target_request_BANK_e dl24m_bank_target_request_bank_FromString(char* bank);



typedef struct dl24m_bank_target_request_t {
    int bank; //numeric
    int target_raw; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} dl24m_bank_target_request_t;

__attribute__((deprecated)) dl24m_bank_target_request_t *dl24m_bank_target_request_create(
    int bank,
    int target_raw
);

void dl24m_bank_target_request_free(dl24m_bank_target_request_t *dl24m_bank_target_request);

dl24m_bank_target_request_t *dl24m_bank_target_request_parseFromJSON(cJSON *dl24m_bank_target_requestJSON);

cJSON *dl24m_bank_target_request_convertToJSON(dl24m_bank_target_request_t *dl24m_bank_target_request);

#endif /* _dl24m_bank_target_request_H_ */

