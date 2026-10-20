/*
 * operation.h
 *
 * 
 */

#ifndef _operation_H_
#define _operation_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct operation_t operation_t;

#include "problem.h"

// Enum STATUS for operation

typedef enum  { labbridge_api_operation_STATUS_NULL = 0, labbridge_api_operation_STATUS_pending, labbridge_api_operation_STATUS_running, labbridge_api_operation_STATUS_succeeded, labbridge_api_operation_STATUS_failed, labbridge_api_operation_STATUS_cancelled } labbridge_api_operation_STATUS_e;

char* operation_status_ToString(labbridge_api_operation_STATUS_e status);

labbridge_api_operation_STATUS_e operation_status_FromString(char* status);

// Enum FINALOUTPUT for operation

typedef enum  { labbridge_api_operation_FINALOUTPUT_NULL = 0, labbridge_api_operation_FINALOUTPUT_on, labbridge_api_operation_FINALOUTPUT_off, labbridge_api_operation_FINALOUTPUT_unknown } labbridge_api_operation_FINALOUTPUT_e;

char* operation_final_output_ToString(labbridge_api_operation_FINALOUTPUT_e final_output);

labbridge_api_operation_FINALOUTPUT_e operation_final_output_FromString(char* final_output);



typedef struct operation_t {
    char *id; // string
    char *device_id; // string
    labbridge_api_operation_STATUS_e status; //enum
    char *stage; // string
    long read_attempts; //numeric
    long write_attempts; //numeric
    long reconnects; //numeric
    long duration_ms; //numeric
    double confirmed_value; //numeric
    labbridge_api_operation_FINALOUTPUT_e final_output; //enum
    int restore_pending; //boolean
    struct problem_t *error; //model
    char *component_id; // string

    int _library_owned; // Is the library responsible for freeing this object?
} operation_t;

__attribute__((deprecated)) operation_t *operation_create(
    char *id,
    char *device_id,
    labbridge_api_operation_STATUS_e status,
    char *stage,
    long read_attempts,
    long write_attempts,
    long reconnects,
    long duration_ms,
    double confirmed_value,
    labbridge_api_operation_FINALOUTPUT_e final_output,
    int restore_pending,
    problem_t *error,
    char *component_id
);

void operation_free(operation_t *operation);

operation_t *operation_parseFromJSON(cJSON *operationJSON);

cJSON *operation_convertToJSON(operation_t *operation);

#endif /* _operation_H_ */

