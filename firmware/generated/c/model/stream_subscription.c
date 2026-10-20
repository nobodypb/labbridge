#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stream_subscription.h"


char* stream_subscription_type_ToString(labbridge_api_stream_subscription_TYPE_e type) {
    char* typeArray[] =  { "NULL", "subscribe" };
    return typeArray[type];
}

labbridge_api_stream_subscription_TYPE_e stream_subscription_type_FromString(char* type){
    int stringToReturn = 0;
    char *typeArray[] =  { "NULL", "subscribe" };
    size_t sizeofArray = sizeof(typeArray) / sizeof(typeArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(type, typeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}
char* stream_subscription_interval_ms_ToString(labbridge_api_stream_subscription_INTERVALMS_e interval_ms) {
    char* interval_msArray[] =  { "NULL", "500", "1000", "2000" };
    return interval_msArray[interval_ms];
}

labbridge_api_stream_subscription_INTERVALMS_e stream_subscription_interval_ms_FromString(char* interval_ms){
    int stringToReturn = 0;
    char *interval_msArray[] =  { "NULL", "500", "1000", "2000" };
    size_t sizeofArray = sizeof(interval_msArray) / sizeof(interval_msArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(interval_ms, interval_msArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static stream_subscription_t *stream_subscription_create_internal(
    labbridge_api_stream_subscription_TYPE_e type,
    char *device_id,
    int interval_ms,
    char *id
    ) {
    stream_subscription_t *stream_subscription_local_var = malloc(sizeof(stream_subscription_t));
    if (!stream_subscription_local_var) {
        return NULL;
    }
    stream_subscription_local_var->type = type;
    stream_subscription_local_var->device_id = device_id;
    stream_subscription_local_var->interval_ms = interval_ms;
    stream_subscription_local_var->id = id;

    stream_subscription_local_var->_library_owned = 1;
    return stream_subscription_local_var;
}

__attribute__((deprecated)) stream_subscription_t *stream_subscription_create(
    labbridge_api_stream_subscription_TYPE_e type,
    char *device_id,
    int interval_ms,
    char *id
    ) {
    return stream_subscription_create_internal (
        type,
        device_id,
        interval_ms,
        id
        );
}

void stream_subscription_free(stream_subscription_t *stream_subscription) {
    if(NULL == stream_subscription){
        return ;
    }
    if(stream_subscription->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "stream_subscription_free");
        return ;
    }
    listEntry_t *listEntry;
    if (stream_subscription->device_id) {
        free(stream_subscription->device_id);
        stream_subscription->device_id = NULL;
    }
    if (stream_subscription->id) {
        free(stream_subscription->id);
        stream_subscription->id = NULL;
    }
    free(stream_subscription);
}

cJSON *stream_subscription_convertToJSON(stream_subscription_t *stream_subscription) {
    cJSON *item = cJSON_CreateObject();

    // stream_subscription->type
    if (labbridge_api_stream_subscription_TYPE_NULL == stream_subscription->type) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "type", stream_subscription_type_ToString(stream_subscription->type)) == NULL)
    {
    goto fail; //Enum
    }


    // stream_subscription->device_id
    if (!stream_subscription->device_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "device_id", stream_subscription->device_id) == NULL) {
    goto fail; //String
    }


    // stream_subscription->interval_ms
    if(stream_subscription->interval_ms != labbridge_api_stream_subscription_INTERVALMS_NULL) {
    if(cJSON_AddNumberToObject(item, "interval_ms", stream_subscription->interval_ms) == NULL) {
    goto fail; //Numeric
    }
    }


    // stream_subscription->id
    if(stream_subscription->id) {
    if(cJSON_AddStringToObject(item, "id", stream_subscription->id) == NULL) {
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

stream_subscription_t *stream_subscription_parseFromJSON(cJSON *stream_subscriptionJSON){

    stream_subscription_t *stream_subscription_local_var = NULL;

    // stream_subscription->type
    cJSON *type = cJSON_GetObjectItemCaseSensitive(stream_subscriptionJSON, "type");
    if (cJSON_IsNull(type)) {
        type = NULL;
    }
    if (!type) {
        goto end;
    }

    labbridge_api_stream_subscription_TYPE_e typeVariable;
    
    if(!cJSON_IsString(type))
    {
    goto end; //Enum
    }
    typeVariable = stream_subscription_type_FromString(type->valuestring);

    // stream_subscription->device_id
    cJSON *device_id = cJSON_GetObjectItemCaseSensitive(stream_subscriptionJSON, "device_id");
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

    // stream_subscription->interval_ms
    cJSON *interval_ms = cJSON_GetObjectItemCaseSensitive(stream_subscriptionJSON, "interval_ms");
    if (cJSON_IsNull(interval_ms)) {
        interval_ms = NULL;
    }
    if (interval_ms) { 
    if(!cJSON_IsNumber(interval_ms))
    {
    goto end; //Numeric
    }
    }

    // stream_subscription->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(stream_subscriptionJSON, "id");
    if (cJSON_IsNull(id)) {
        id = NULL;
    }
    if (id) { 
    if(!cJSON_IsString(id) && !cJSON_IsNull(id))
    {
    goto end; //String
    }
    }


    stream_subscription_local_var = stream_subscription_create_internal (
        typeVariable,
        strdup(device_id->valuestring),
        interval_ms ? interval_ms->valuedouble : 0,
        id && !cJSON_IsNull(id) ? strdup(id->valuestring) : NULL
        );

    return stream_subscription_local_var;
end:
    return NULL;

}
