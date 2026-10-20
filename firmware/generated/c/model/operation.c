#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "operation.h"


char* operation_status_ToString(labbridge_api_operation_STATUS_e status) {
    char* statusArray[] =  { "NULL", "pending", "running", "succeeded", "failed", "cancelled" };
    return statusArray[status];
}

labbridge_api_operation_STATUS_e operation_status_FromString(char* status){
    int stringToReturn = 0;
    char *statusArray[] =  { "NULL", "pending", "running", "succeeded", "failed", "cancelled" };
    size_t sizeofArray = sizeof(statusArray) / sizeof(statusArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(status, statusArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}
char* operation_final_output_ToString(labbridge_api_operation_FINALOUTPUT_e final_output) {
    char* final_outputArray[] =  { "NULL", "on", "off", "unknown" };
    return final_outputArray[final_output];
}

labbridge_api_operation_FINALOUTPUT_e operation_final_output_FromString(char* final_output){
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

static operation_t *operation_create_internal(
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
    ) {
    operation_t *operation_local_var = malloc(sizeof(operation_t));
    if (!operation_local_var) {
        return NULL;
    }
    operation_local_var->id = id;
    operation_local_var->device_id = device_id;
    operation_local_var->status = status;
    operation_local_var->stage = stage;
    operation_local_var->read_attempts = read_attempts;
    operation_local_var->write_attempts = write_attempts;
    operation_local_var->reconnects = reconnects;
    operation_local_var->duration_ms = duration_ms;
    operation_local_var->confirmed_value = confirmed_value;
    operation_local_var->final_output = final_output;
    operation_local_var->restore_pending = restore_pending;
    operation_local_var->error = error;
    operation_local_var->component_id = component_id;

    operation_local_var->_library_owned = 1;
    return operation_local_var;
}

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
    ) {
    return operation_create_internal (
        id,
        device_id,
        status,
        stage,
        read_attempts,
        write_attempts,
        reconnects,
        duration_ms,
        confirmed_value,
        final_output,
        restore_pending,
        error,
        component_id
        );
}

void operation_free(operation_t *operation) {
    if(NULL == operation){
        return ;
    }
    if(operation->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "operation_free");
        return ;
    }
    listEntry_t *listEntry;
    if (operation->id) {
        free(operation->id);
        operation->id = NULL;
    }
    if (operation->device_id) {
        free(operation->device_id);
        operation->device_id = NULL;
    }
    if (operation->stage) {
        free(operation->stage);
        operation->stage = NULL;
    }
    if (operation->error) {
        problem_free(operation->error);
        operation->error = NULL;
    }
    if (operation->component_id) {
        free(operation->component_id);
        operation->component_id = NULL;
    }
    free(operation);
}

cJSON *operation_convertToJSON(operation_t *operation) {
    cJSON *item = cJSON_CreateObject();

    // operation->id
    if (!operation->id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "id", operation->id) == NULL) {
    goto fail; //String
    }


    // operation->device_id
    if (!operation->device_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "device_id", operation->device_id) == NULL) {
    goto fail; //String
    }


    // operation->status
    if (labbridge_api_operation_STATUS_NULL == operation->status) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "status", operation_status_ToString(operation->status)) == NULL)
    {
    goto fail; //Enum
    }


    // operation->stage
    if (!operation->stage) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "stage", operation->stage) == NULL) {
    goto fail; //String
    }


    // operation->read_attempts
    if (!operation->read_attempts) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "read_attempts", operation->read_attempts) == NULL) {
    goto fail; //Numeric
    }


    // operation->write_attempts
    if (!operation->write_attempts) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "write_attempts", operation->write_attempts) == NULL) {
    goto fail; //Numeric
    }


    // operation->reconnects
    if (!operation->reconnects) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "reconnects", operation->reconnects) == NULL) {
    goto fail; //Numeric
    }


    // operation->duration_ms
    if (!operation->duration_ms) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "duration_ms", operation->duration_ms) == NULL) {
    goto fail; //Numeric
    }


    // operation->confirmed_value
    if(operation->confirmed_value) {
    if(cJSON_AddNumberToObject(item, "confirmed_value", operation->confirmed_value) == NULL) {
    goto fail; //Numeric
    }
    }


    // operation->final_output
    if (labbridge_api_operation_FINALOUTPUT_NULL == operation->final_output) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "final_output", operation_final_output_ToString(operation->final_output)) == NULL)
    {
    goto fail; //Enum
    }


    // operation->restore_pending
    if (!operation->restore_pending) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "restore_pending", operation->restore_pending) == NULL) {
    goto fail; //Bool
    }


    // operation->error
    if(operation->error) {
    cJSON *error_local_JSON = problem_convertToJSON(operation->error);
    if(error_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "error", error_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // operation->component_id
    if(operation->component_id) {
    if(cJSON_AddStringToObject(item, "component_id", operation->component_id) == NULL) {
    goto fail; //String
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

operation_t *operation_parseFromJSON(cJSON *operationJSON){

    operation_t *operation_local_var = NULL;

    // define the local variable for operation->error
    problem_t *error_local_nonprim = NULL;

    // operation->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(operationJSON, "id");
    if (cJSON_IsNull(id)) {
        id = NULL;
    }
    if (!id) {
        goto end;
    }

    
    if(!cJSON_IsString(id))
    {
    goto end; //String
    }

    // operation->device_id
    cJSON *device_id = cJSON_GetObjectItemCaseSensitive(operationJSON, "device_id");
    if (cJSON_IsNull(device_id)) {
        device_id = NULL;
    }
    if (!device_id) {
        goto end;
    }

    
    if(!cJSON_IsString(device_id))
    {
    goto end; //String
    }

    // operation->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(operationJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (!status) {
        goto end;
    }

    labbridge_api_operation_STATUS_e statusVariable;
    
    if(!cJSON_IsString(status))
    {
    goto end; //Enum
    }
    statusVariable = operation_status_FromString(status->valuestring);

    // operation->stage
    cJSON *stage = cJSON_GetObjectItemCaseSensitive(operationJSON, "stage");
    if (cJSON_IsNull(stage)) {
        stage = NULL;
    }
    if (!stage) {
        goto end;
    }

    
    if(!cJSON_IsString(stage))
    {
    goto end; //String
    }

    // operation->read_attempts
    cJSON *read_attempts = cJSON_GetObjectItemCaseSensitive(operationJSON, "read_attempts");
    if (cJSON_IsNull(read_attempts)) {
        read_attempts = NULL;
    }
    if (!read_attempts) {
        goto end;
    }

    
    if(!cJSON_IsNumber(read_attempts))
    {
    goto end; //Numeric
    }

    // operation->write_attempts
    cJSON *write_attempts = cJSON_GetObjectItemCaseSensitive(operationJSON, "write_attempts");
    if (cJSON_IsNull(write_attempts)) {
        write_attempts = NULL;
    }
    if (!write_attempts) {
        goto end;
    }

    
    if(!cJSON_IsNumber(write_attempts))
    {
    goto end; //Numeric
    }

    // operation->reconnects
    cJSON *reconnects = cJSON_GetObjectItemCaseSensitive(operationJSON, "reconnects");
    if (cJSON_IsNull(reconnects)) {
        reconnects = NULL;
    }
    if (!reconnects) {
        goto end;
    }

    
    if(!cJSON_IsNumber(reconnects))
    {
    goto end; //Numeric
    }

    // operation->duration_ms
    cJSON *duration_ms = cJSON_GetObjectItemCaseSensitive(operationJSON, "duration_ms");
    if (cJSON_IsNull(duration_ms)) {
        duration_ms = NULL;
    }
    if (!duration_ms) {
        goto end;
    }

    
    if(!cJSON_IsNumber(duration_ms))
    {
    goto end; //Numeric
    }

    // operation->confirmed_value
    cJSON *confirmed_value = cJSON_GetObjectItemCaseSensitive(operationJSON, "confirmed_value");
    if (cJSON_IsNull(confirmed_value)) {
        confirmed_value = NULL;
    }
    if (confirmed_value) { 
    if(!cJSON_IsNumber(confirmed_value))
    {
    goto end; //Numeric
    }
    }

    // operation->final_output
    cJSON *final_output = cJSON_GetObjectItemCaseSensitive(operationJSON, "final_output");
    if (cJSON_IsNull(final_output)) {
        final_output = NULL;
    }
    if (!final_output) {
        goto end;
    }

    labbridge_api_operation_FINALOUTPUT_e final_outputVariable;
    
    if(!cJSON_IsString(final_output))
    {
    goto end; //Enum
    }
    final_outputVariable = operation_final_output_FromString(final_output->valuestring);

    // operation->restore_pending
    cJSON *restore_pending = cJSON_GetObjectItemCaseSensitive(operationJSON, "restore_pending");
    if (cJSON_IsNull(restore_pending)) {
        restore_pending = NULL;
    }
    if (!restore_pending) {
        goto end;
    }

    
    if(!cJSON_IsBool(restore_pending))
    {
    goto end; //Bool
    }

    // operation->error
    cJSON *error = cJSON_GetObjectItemCaseSensitive(operationJSON, "error");
    if (cJSON_IsNull(error)) {
        error = NULL;
    }
    if (error) { 
    error_local_nonprim = problem_parseFromJSON(error); //nonprimitive
    }

    // operation->component_id
    cJSON *component_id = cJSON_GetObjectItemCaseSensitive(operationJSON, "component_id");
    if (cJSON_IsNull(component_id)) {
        component_id = NULL;
    }
    if (component_id) { 
    if(!cJSON_IsString(component_id) && !cJSON_IsNull(component_id))
    {
    goto end; //String
    }
    }


    operation_local_var = operation_create_internal (
        strdup(id->valuestring),
        strdup(device_id->valuestring),
        statusVariable,
        strdup(stage->valuestring),
        read_attempts->valuedouble,
        write_attempts->valuedouble,
        reconnects->valuedouble,
        duration_ms->valuedouble,
        confirmed_value ? confirmed_value->valuedouble : 0,
        final_outputVariable,
        restore_pending->valueint,
        error ? error_local_nonprim : NULL,
        component_id && !cJSON_IsNull(component_id) ? strdup(component_id->valuestring) : NULL
        );

    return operation_local_var;
end:
    if (error_local_nonprim) {
        problem_free(error_local_nonprim);
        error_local_nonprim = NULL;
    }
    return NULL;

}
