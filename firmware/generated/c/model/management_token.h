/*
 * management_token.h
 *
 * Secret bearer credential returned only through physically trusted USB management. Never publish it or include it in shared test artifacts.
 */

#ifndef _management_token_H_
#define _management_token_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct management_token_t management_token_t;




typedef struct management_token_t {
    char *token; // string

    int _library_owned; // Is the library responsible for freeing this object?
} management_token_t;

__attribute__((deprecated)) management_token_t *management_token_create(
    char *token
);

void management_token_free(management_token_t *management_token);

management_token_t *management_token_parseFromJSON(cJSON *management_tokenJSON);

cJSON *management_token_convertToJSON(management_token_t *management_token);

#endif /* _management_token_H_ */

