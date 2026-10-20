/*
 * rd60xx_configuration.h
 *
 * Expected ordinary RD60xx model. Existing configurations without this object retain RD6024 behavior. Unsupported models/P variants must not be used with this profile.
 */

#ifndef _rd60xx_configuration_H_
#define _rd60xx_configuration_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct rd60xx_configuration_t rd60xx_configuration_t;


// Enum MODEL for rd60xx_configuration

typedef enum  { labbridge_api_rd60xx_configuration_MODEL_NULL = 0, labbridge_api_rd60xx_configuration_MODEL__6018, labbridge_api_rd60xx_configuration_MODEL__6024 } labbridge_api_rd60xx_configuration_MODEL_e;

char* rd60xx_configuration_model_ToString(labbridge_api_rd60xx_configuration_MODEL_e model);

labbridge_api_rd60xx_configuration_MODEL_e rd60xx_configuration_model_FromString(char* model);



typedef struct rd60xx_configuration_t {
    long model; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} rd60xx_configuration_t;

__attribute__((deprecated)) rd60xx_configuration_t *rd60xx_configuration_create(
    long model
);

void rd60xx_configuration_free(rd60xx_configuration_t *rd60xx_configuration);

rd60xx_configuration_t *rd60xx_configuration_parseFromJSON(cJSON *rd60xx_configurationJSON);

cJSON *rd60xx_configuration_convertToJSON(rd60xx_configuration_t *rd60xx_configuration);

#endif /* _rd60xx_configuration_H_ */

