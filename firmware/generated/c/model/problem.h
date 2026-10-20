/*
 * problem.h
 *
 * Structured request or operation failure using application/problem+json. A failed write may already have changed hardware. Inspect final_output and restore_pending and request independent STOP when the outcome is uncertain.
 */

#ifndef _problem_H_
#define _problem_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct problem_t problem_t;


// Enum FINALOUTPUT for problem

typedef enum  { labbridge_api_problem_FINALOUTPUT_NULL = 0, labbridge_api_problem_FINALOUTPUT_on, labbridge_api_problem_FINALOUTPUT_off, labbridge_api_problem_FINALOUTPUT_unknown } labbridge_api_problem_FINALOUTPUT_e;

char* problem_final_output_ToString(labbridge_api_problem_FINALOUTPUT_e final_output);

labbridge_api_problem_FINALOUTPUT_e problem_final_output_FromString(char* final_output);



typedef struct problem_t {
    char *type; // string
    char *title; // string
    long status; //numeric
    char *detail; // string
    char *instance; // string
    char *request_id; // string
    char *stage; // string
    labbridge_api_problem_FINALOUTPUT_e final_output; //enum
    int restore_pending; //boolean

    int _library_owned; // Is the library responsible for freeing this object?
} problem_t;

__attribute__((deprecated)) problem_t *problem_create(
    char *type,
    char *title,
    long status,
    char *detail,
    char *instance,
    char *request_id,
    char *stage,
    labbridge_api_problem_FINALOUTPUT_e final_output,
    int restore_pending
);

void problem_free(problem_t *problem);

problem_t *problem_parseFromJSON(cJSON *problemJSON);

cJSON *problem_convertToJSON(problem_t *problem);

#endif /* _problem_H_ */

