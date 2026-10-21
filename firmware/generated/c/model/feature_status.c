#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "feature_status.h"


char* feature_status_status_ToString(labbridge_api_feature_status_STATUS_e status) {
    char* statusArray[] =  { "NULL", "supported", "unverified", "unavailable" };
    return statusArray[status];
}

labbridge_api_feature_status_STATUS_e feature_status_status_FromString(char* status){
    int stringToReturn = 0;
    char *statusArray[] =  { "NULL", "supported", "unverified", "unavailable" };
    size_t sizeofArray = sizeof(statusArray) / sizeof(statusArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(status, statusArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static feature_status_t *feature_status_create_internal(
    char *id,
    labbridge_api_feature_status_STATUS_e status,
    char *reason,
    char *unit,
    char *operation,
    int requires_off,
    int requires_lease,
    char *evidence
    ) {
    feature_status_t *feature_status_local_var = malloc(sizeof(feature_status_t));
    if (!feature_status_local_var) {
        return NULL;
    }
    feature_status_local_var->id = id;
    feature_status_local_var->status = status;
    feature_status_local_var->reason = reason;
    feature_status_local_var->unit = unit;
    feature_status_local_var->operation = operation;
    feature_status_local_var->requires_off = requires_off;
    feature_status_local_var->requires_lease = requires_lease;
    feature_status_local_var->evidence = evidence;

    feature_status_local_var->_library_owned = 1;
    return feature_status_local_var;
}

__attribute__((deprecated)) feature_status_t *feature_status_create(
    char *id,
    labbridge_api_feature_status_STATUS_e status,
    char *reason,
    char *unit,
    char *operation,
    int requires_off,
    int requires_lease,
    char *evidence
    ) {
    return feature_status_create_internal (
        id,
        status,
        reason,
        unit,
        operation,
        requires_off,
        requires_lease,
        evidence
        );
}

void feature_status_free(feature_status_t *feature_status) {
    if(NULL == feature_status){
        return ;
    }
    if(feature_status->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "feature_status_free");
        return ;
    }
    listEntry_t *listEntry;
    if (feature_status->id) {
        free(feature_status->id);
        feature_status->id = NULL;
    }
    if (feature_status->reason) {
        free(feature_status->reason);
        feature_status->reason = NULL;
    }
    if (feature_status->unit) {
        free(feature_status->unit);
        feature_status->unit = NULL;
    }
    if (feature_status->operation) {
        free(feature_status->operation);
        feature_status->operation = NULL;
    }
    if (feature_status->evidence) {
        free(feature_status->evidence);
        feature_status->evidence = NULL;
    }
    free(feature_status);
}

cJSON *feature_status_convertToJSON(feature_status_t *feature_status) {
    cJSON *item = cJSON_CreateObject();

    // feature_status->id
    if (!feature_status->id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "id", feature_status->id) == NULL) {
    goto fail; //String
    }


    // feature_status->status
    if (labbridge_api_feature_status_STATUS_NULL == feature_status->status) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "status", feature_status_status_ToString(feature_status->status)) == NULL)
    {
    goto fail; //Enum
    }


    // feature_status->reason
    if (!feature_status->reason) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "reason", feature_status->reason) == NULL) {
    goto fail; //String
    }


    // feature_status->unit
    if(feature_status->unit) {
    if(cJSON_AddStringToObject(item, "unit", feature_status->unit) == NULL) {
    goto fail; //String
    }
    }


    // feature_status->operation
    if(feature_status->operation) {
    if(cJSON_AddStringToObject(item, "operation", feature_status->operation) == NULL) {
    goto fail; //String
    }
    }


    // feature_status->requires_off
    if(feature_status->requires_off) {
    if(cJSON_AddBoolToObject(item, "requires_off", feature_status->requires_off) == NULL) {
    goto fail; //Bool
    }
    }


    // feature_status->requires_lease
    if(feature_status->requires_lease) {
    if(cJSON_AddBoolToObject(item, "requires_lease", feature_status->requires_lease) == NULL) {
    goto fail; //Bool
    }
    }


    // feature_status->evidence
    if(feature_status->evidence) {
    if(cJSON_AddStringToObject(item, "evidence", feature_status->evidence) == NULL) {
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

feature_status_t *feature_status_parseFromJSON(cJSON *feature_statusJSON){

    feature_status_t *feature_status_local_var = NULL;

    // feature_status->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(feature_statusJSON, "id");
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

    // feature_status->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(feature_statusJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (!status) {
        goto end;
    }

    labbridge_api_feature_status_STATUS_e statusVariable;
    
    if(!cJSON_IsString(status))
    {
    goto end; //Enum
    }
    statusVariable = feature_status_status_FromString(status->valuestring);

    // feature_status->reason
    cJSON *reason = cJSON_GetObjectItemCaseSensitive(feature_statusJSON, "reason");
    if (cJSON_IsNull(reason)) {
        reason = NULL;
    }
    if (!reason) {
        goto end;
    }

    
    if(!cJSON_IsString(reason))
    {
    goto end; //String
    }

    // feature_status->unit
    cJSON *unit = cJSON_GetObjectItemCaseSensitive(feature_statusJSON, "unit");
    if (cJSON_IsNull(unit)) {
        unit = NULL;
    }
    if (unit) { 
    if(!cJSON_IsString(unit) && !cJSON_IsNull(unit))
    {
    goto end; //String
    }
    }

    // feature_status->operation
    cJSON *operation = cJSON_GetObjectItemCaseSensitive(feature_statusJSON, "operation");
    if (cJSON_IsNull(operation)) {
        operation = NULL;
    }
    if (operation) { 
    if(!cJSON_IsString(operation) && !cJSON_IsNull(operation))
    {
    goto end; //String
    }
    }

    // feature_status->requires_off
    cJSON *requires_off = cJSON_GetObjectItemCaseSensitive(feature_statusJSON, "requires_off");
    if (cJSON_IsNull(requires_off)) {
        requires_off = NULL;
    }
    if (requires_off) { 
    if(!cJSON_IsBool(requires_off))
    {
    goto end; //Bool
    }
    }

    // feature_status->requires_lease
    cJSON *requires_lease = cJSON_GetObjectItemCaseSensitive(feature_statusJSON, "requires_lease");
    if (cJSON_IsNull(requires_lease)) {
        requires_lease = NULL;
    }
    if (requires_lease) { 
    if(!cJSON_IsBool(requires_lease))
    {
    goto end; //Bool
    }
    }

    // feature_status->evidence
    cJSON *evidence = cJSON_GetObjectItemCaseSensitive(feature_statusJSON, "evidence");
    if (cJSON_IsNull(evidence)) {
        evidence = NULL;
    }
    if (evidence) { 
    if(!cJSON_IsString(evidence) && !cJSON_IsNull(evidence))
    {
    goto end; //String
    }
    }


    feature_status_local_var = feature_status_create_internal (
        strdup(id->valuestring),
        statusVariable,
        strdup(reason->valuestring),
        unit && !cJSON_IsNull(unit) ? strdup(unit->valuestring) : NULL,
        operation && !cJSON_IsNull(operation) ? strdup(operation->valuestring) : NULL,
        requires_off ? requires_off->valueint : 0,
        requires_lease ? requires_lease->valueint : 0,
        evidence && !cJSON_IsNull(evidence) ? strdup(evidence->valuestring) : NULL
        );

    return feature_status_local_var;
end:
    return NULL;

}
