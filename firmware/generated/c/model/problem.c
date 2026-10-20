#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "problem.h"


char* problem_final_output_ToString(labbridge_api_problem_FINALOUTPUT_e final_output) {
    char* final_outputArray[] =  { "NULL", "on", "off", "unknown" };
    return final_outputArray[final_output];
}

labbridge_api_problem_FINALOUTPUT_e problem_final_output_FromString(char* final_output){
    int stringToReturn = 0;
    char *final_outputArray[] =  { "NULL", "on", "off", "unknown" };
    size_t sizeofArray = sizeof(final_outputArray) / sizeof(final_outputArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(final_output, final_outputArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static problem_t *problem_create_internal(
    char *type,
    char *title,
    long status,
    char *detail,
    char *instance,
    char *request_id,
    char *stage,
    labbridge_api_problem_FINALOUTPUT_e final_output,
    int restore_pending
    ) {
    problem_t *problem_local_var = malloc(sizeof(problem_t));
    if (!problem_local_var) {
        return NULL;
    }
    problem_local_var->type = type;
    problem_local_var->title = title;
    problem_local_var->status = status;
    problem_local_var->detail = detail;
    problem_local_var->instance = instance;
    problem_local_var->request_id = request_id;
    problem_local_var->stage = stage;
    problem_local_var->final_output = final_output;
    problem_local_var->restore_pending = restore_pending;

    problem_local_var->_library_owned = 1;
    return problem_local_var;
}

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
    ) {
    return problem_create_internal (
        type,
        title,
        status,
        detail,
        instance,
        request_id,
        stage,
        final_output,
        restore_pending
        );
}

void problem_free(problem_t *problem) {
    if(NULL == problem){
        return ;
    }
    if(problem->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "problem_free");
        return ;
    }
    listEntry_t *listEntry;
    if (problem->type) {
        free(problem->type);
        problem->type = NULL;
    }
    if (problem->title) {
        free(problem->title);
        problem->title = NULL;
    }
    if (problem->detail) {
        free(problem->detail);
        problem->detail = NULL;
    }
    if (problem->instance) {
        free(problem->instance);
        problem->instance = NULL;
    }
    if (problem->request_id) {
        free(problem->request_id);
        problem->request_id = NULL;
    }
    if (problem->stage) {
        free(problem->stage);
        problem->stage = NULL;
    }
    free(problem);
}

cJSON *problem_convertToJSON(problem_t *problem) {
    cJSON *item = cJSON_CreateObject();

    // problem->type
    if (!problem->type) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "type", problem->type) == NULL) {
    goto fail; //String
    }


    // problem->title
    if (!problem->title) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "title", problem->title) == NULL) {
    goto fail; //String
    }


    // problem->status
    if (!problem->status) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "status", problem->status) == NULL) {
    goto fail; //Numeric
    }


    // problem->detail
    if (!problem->detail) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "detail", problem->detail) == NULL) {
    goto fail; //String
    }


    // problem->instance
    if(problem->instance) {
    if(cJSON_AddStringToObject(item, "instance", problem->instance) == NULL) {
    goto fail; //String
    }
    }


    // problem->request_id
    if(problem->request_id) {
    if(cJSON_AddStringToObject(item, "request_id", problem->request_id) == NULL) {
    goto fail; //String
    }
    }


    // problem->stage
    if(problem->stage) {
    if(cJSON_AddStringToObject(item, "stage", problem->stage) == NULL) {
    goto fail; //String
    }
    }


    // problem->final_output
    if(problem->final_output != labbridge_api_problem_FINALOUTPUT_NULL) {
    if(cJSON_AddStringToObject(item, "final_output", problem_final_output_ToString(problem->final_output)) == NULL)
    {
    goto fail; //Enum
    }
    }


    // problem->restore_pending
    if(problem->restore_pending) {
    if(cJSON_AddBoolToObject(item, "restore_pending", problem->restore_pending) == NULL) {
    goto fail; //Bool
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

problem_t *problem_parseFromJSON(cJSON *problemJSON){

    problem_t *problem_local_var = NULL;

    // problem->type
    cJSON *type = cJSON_GetObjectItemCaseSensitive(problemJSON, "type");
    if (cJSON_IsNull(type)) {
        type = NULL;
    }
    if (!type) {
        goto end;
    }

    
    if(!cJSON_IsString(type))
    {
    goto end; //String
    }

    // problem->title
    cJSON *title = cJSON_GetObjectItemCaseSensitive(problemJSON, "title");
    if (cJSON_IsNull(title)) {
        title = NULL;
    }
    if (!title) {
        goto end;
    }

    
    if(!cJSON_IsString(title))
    {
    goto end; //String
    }

    // problem->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(problemJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (!status) {
        goto end;
    }

    
    if(!cJSON_IsNumber(status))
    {
    goto end; //Numeric
    }

    // problem->detail
    cJSON *detail = cJSON_GetObjectItemCaseSensitive(problemJSON, "detail");
    if (cJSON_IsNull(detail)) {
        detail = NULL;
    }
    if (!detail) {
        goto end;
    }

    
    if(!cJSON_IsString(detail))
    {
    goto end; //String
    }

    // problem->instance
    cJSON *instance = cJSON_GetObjectItemCaseSensitive(problemJSON, "instance");
    if (cJSON_IsNull(instance)) {
        instance = NULL;
    }
    if (instance) { 
    if(!cJSON_IsString(instance) && !cJSON_IsNull(instance))
    {
    goto end; //String
    }
    }

    // problem->request_id
    cJSON *request_id = cJSON_GetObjectItemCaseSensitive(problemJSON, "request_id");
    if (cJSON_IsNull(request_id)) {
        request_id = NULL;
    }
    if (request_id) { 
    if(!cJSON_IsString(request_id) && !cJSON_IsNull(request_id))
    {
    goto end; //String
    }
    }

    // problem->stage
    cJSON *stage = cJSON_GetObjectItemCaseSensitive(problemJSON, "stage");
    if (cJSON_IsNull(stage)) {
        stage = NULL;
    }
    if (stage) { 
    if(!cJSON_IsString(stage) && !cJSON_IsNull(stage))
    {
    goto end; //String
    }
    }

    // problem->final_output
    cJSON *final_output = cJSON_GetObjectItemCaseSensitive(problemJSON, "final_output");
    if (cJSON_IsNull(final_output)) {
        final_output = NULL;
    }
    labbridge_api_problem_FINALOUTPUT_e final_outputVariable;
    if (final_output) { 
    if(!cJSON_IsString(final_output))
    {
    goto end; //Enum
    }
    final_outputVariable = problem_final_output_FromString(final_output->valuestring);
    }

    // problem->restore_pending
    cJSON *restore_pending = cJSON_GetObjectItemCaseSensitive(problemJSON, "restore_pending");
    if (cJSON_IsNull(restore_pending)) {
        restore_pending = NULL;
    }
    if (restore_pending) { 
    if(!cJSON_IsBool(restore_pending))
    {
    goto end; //Bool
    }
    }


    problem_local_var = problem_create_internal (
        strdup(type->valuestring),
        strdup(title->valuestring),
        status->valuedouble,
        strdup(detail->valuestring),
        instance && !cJSON_IsNull(instance) ? strdup(instance->valuestring) : NULL,
        request_id && !cJSON_IsNull(request_id) ? strdup(request_id->valuestring) : NULL,
        stage && !cJSON_IsNull(stage) ? strdup(stage->valuestring) : NULL,
        final_output ? final_outputVariable : labbridge_api_problem_FINALOUTPUT_NULL,
        restore_pending ? restore_pending->valueint : 0
        );

    return problem_local_var;
end:
    return NULL;

}
