/*
 * dl24m_register.h
 *
 * 
 */

#ifndef _dl24m_register_H_
#define _dl24m_register_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct dl24m_register_t dl24m_register_t;


// Enum QUALITY for dl24m_register

typedef enum  { labbridge_api_dl24m_register_QUALITY_NULL = 0, labbridge_api_dl24m_register_QUALITY_valid, labbridge_api_dl24m_register_QUALITY_stale, labbridge_api_dl24m_register_QUALITY_unavailable } labbridge_api_dl24m_register_QUALITY_e;

char* dl24m_register_quality_ToString(labbridge_api_dl24m_register_QUALITY_e quality);

labbridge_api_dl24m_register_QUALITY_e dl24m_register_quality_FromString(char* quality);



typedef struct dl24m_register_t {
    char *name; // string
    int command; //numeric
    labbridge_api_dl24m_register_QUALITY_e quality; //enum
    int value; //numeric
    int age_ms; //numeric
    int received_ms; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} dl24m_register_t;

__attribute__((deprecated)) dl24m_register_t *dl24m_register_create(
    char *name,
    int command,
    labbridge_api_dl24m_register_QUALITY_e quality,
    int value,
    int age_ms,
    int received_ms
);

void dl24m_register_free(dl24m_register_t *dl24m_register);

dl24m_register_t *dl24m_register_parseFromJSON(cJSON *dl24m_registerJSON);

cJSON *dl24m_register_convertToJSON(dl24m_register_t *dl24m_register);

#endif /* _dl24m_register_H_ */

