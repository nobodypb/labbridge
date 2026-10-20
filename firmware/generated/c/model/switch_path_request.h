/*
 * switch_path_request.h
 *
 * 
 */

#ifndef _switch_path_request_H_
#define _switch_path_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct switch_path_request_t switch_path_request_t;


// Enum PATH for switch_path_request

typedef enum  { labbridge_api_switch_path_request_PATH_NULL = 0, labbridge_api_switch_path_request_PATH_isolated, labbridge_api_switch_path_request_PATH_charge, labbridge_api_switch_path_request_PATH_discharge } labbridge_api_switch_path_request_PATH_e;

char* switch_path_request_path_ToString(labbridge_api_switch_path_request_PATH_e path);

labbridge_api_switch_path_request_PATH_e switch_path_request_path_FromString(char* path);



typedef struct switch_path_request_t {
    labbridge_api_switch_path_request_PATH_e path; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} switch_path_request_t;

__attribute__((deprecated)) switch_path_request_t *switch_path_request_create(
    labbridge_api_switch_path_request_PATH_e path
);

void switch_path_request_free(switch_path_request_t *switch_path_request);

switch_path_request_t *switch_path_request_parseFromJSON(cJSON *switch_path_requestJSON);

cJSON *switch_path_request_convertToJSON(switch_path_request_t *switch_path_request);

#endif /* _switch_path_request_H_ */

