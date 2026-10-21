#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stream_snapshot.h"


char* stream_snapshot_type_ToString(labbridge_api_stream_snapshot_TYPE_e type) {
    char* typeArray[] =  { "NULL", "snapshot" };
    return typeArray[type];
}

labbridge_api_stream_snapshot_TYPE_e stream_snapshot_type_FromString(char* type){
    int stringToReturn = 0;
    char *typeArray[] =  { "NULL", "snapshot" };
    size_t sizeofArray = sizeof(typeArray) / sizeof(typeArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(type, typeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static stream_snapshot_t *stream_snapshot_create_internal(
    labbridge_api_stream_snapshot_TYPE_e type,
    char *boot_id,
    int sequence,
    int gap,
    list_t* body
    ) {
    stream_snapshot_t *stream_snapshot_local_var = malloc(sizeof(stream_snapshot_t));
    if (!stream_snapshot_local_var) {
        return NULL;
    }
    stream_snapshot_local_var->type = type;
    stream_snapshot_local_var->boot_id = boot_id;
    stream_snapshot_local_var->sequence = sequence;
    stream_snapshot_local_var->gap = gap;
    stream_snapshot_local_var->body = body;

    stream_snapshot_local_var->_library_owned = 1;
    return stream_snapshot_local_var;
}

__attribute__((deprecated)) stream_snapshot_t *stream_snapshot_create(
    labbridge_api_stream_snapshot_TYPE_e type,
    char *boot_id,
    int sequence,
    int gap,
    list_t* body
    ) {
    return stream_snapshot_create_internal (
        type,
        boot_id,
        sequence,
        gap,
        body
        );
}

void stream_snapshot_free(stream_snapshot_t *stream_snapshot) {
    if(NULL == stream_snapshot){
        return ;
    }
    if(stream_snapshot->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "stream_snapshot_free");
        return ;
    }
    listEntry_t *listEntry;
    if (stream_snapshot->boot_id) {
        free(stream_snapshot->boot_id);
        stream_snapshot->boot_id = NULL;
    }
    if (stream_snapshot->body) {
        list_ForEach(listEntry, stream_snapshot->body) {
            keyValuePair_t *localKeyValue = listEntry->data;
            free (localKeyValue->key);
            free (localKeyValue->value);
            keyValuePair_free(localKeyValue);
        }
        list_freeList(stream_snapshot->body);
        stream_snapshot->body = NULL;
    }
    free(stream_snapshot);
}

cJSON *stream_snapshot_convertToJSON(stream_snapshot_t *stream_snapshot) {
    cJSON *item = cJSON_CreateObject();

    // stream_snapshot->type
    if (labbridge_api_stream_snapshot_TYPE_NULL == stream_snapshot->type) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "type", stream_snapshot_type_ToString(stream_snapshot->type)) == NULL)
    {
    goto fail; //Enum
    }


    // stream_snapshot->boot_id
    if (!stream_snapshot->boot_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "boot_id", stream_snapshot->boot_id) == NULL) {
    goto fail; //String
    }


    // stream_snapshot->sequence
    if (!stream_snapshot->sequence) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "sequence", stream_snapshot->sequence) == NULL) {
    goto fail; //Numeric
    }


    // stream_snapshot->gap
    if (!stream_snapshot->gap) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "gap", stream_snapshot->gap) == NULL) {
    goto fail; //Bool
    }


    // stream_snapshot->body
    if (!stream_snapshot->body) {
        goto fail;
    }
    cJSON *body = cJSON_AddObjectToObject(item, "body");
    if(body == NULL) {
        goto fail; //primitive map container
    }
    cJSON *localMapObject = body;
    listEntry_t *bodyListEntry;
    if (stream_snapshot->body) {
    list_ForEach(bodyListEntry, stream_snapshot->body) {
        keyValuePair_t *localKeyValue = bodyListEntry->data;
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

stream_snapshot_t *stream_snapshot_parseFromJSON(cJSON *stream_snapshotJSON){

    stream_snapshot_t *stream_snapshot_local_var = NULL;

    // define the local map for stream_snapshot->body
    list_t *bodyList = NULL;

    // stream_snapshot->type
    cJSON *type = cJSON_GetObjectItemCaseSensitive(stream_snapshotJSON, "type");
    if (cJSON_IsNull(type)) {
        type = NULL;
    }
    if (!type) {
        goto end;
    }

    labbridge_api_stream_snapshot_TYPE_e typeVariable;
    
    if(!cJSON_IsString(type))
    {
    goto end; //Enum
    }
    typeVariable = stream_snapshot_type_FromString(type->valuestring);

    // stream_snapshot->boot_id
    cJSON *boot_id = cJSON_GetObjectItemCaseSensitive(stream_snapshotJSON, "boot_id");
    if (cJSON_IsNull(boot_id)) {
        boot_id = NULL;
    }
    if (!boot_id) {
        goto end;
    }

    
    if(!cJSON_IsString(boot_id))
    {
    goto end; //String
    }

    // stream_snapshot->sequence
    cJSON *sequence = cJSON_GetObjectItemCaseSensitive(stream_snapshotJSON, "sequence");
    if (cJSON_IsNull(sequence)) {
        sequence = NULL;
    }
    if (!sequence) {
        goto end;
    }

    
    if(!cJSON_IsNumber(sequence))
    {
    goto end; //Numeric
    }

    // stream_snapshot->gap
    cJSON *gap = cJSON_GetObjectItemCaseSensitive(stream_snapshotJSON, "gap");
    if (cJSON_IsNull(gap)) {
        gap = NULL;
    }
    if (!gap) {
        goto end;
    }

    
    if(!cJSON_IsBool(gap))
    {
    goto end; //Bool
    }

    // stream_snapshot->body
    cJSON *body = cJSON_GetObjectItemCaseSensitive(stream_snapshotJSON, "body");
    if (cJSON_IsNull(body)) {
        body = NULL;
    }
    if (!body) {
        goto end;
    }

    
    cJSON *body_local_map = NULL;
    if(!cJSON_IsObject(body) && !cJSON_IsNull(body))
    {
        goto end;//primitive map container
    }
    if(cJSON_IsObject(body))
    {
        bodyList = list_createList();
        keyValuePair_t *localMapKeyPair;
        cJSON_ArrayForEach(body_local_map, body)
        {
            cJSON *localMapObject = body_local_map;
            list_addElement(bodyList , localMapKeyPair);
        }
    }


    stream_snapshot_local_var = stream_snapshot_create_internal (
        typeVariable,
        strdup(boot_id->valuestring),
        sequence->valuedouble,
        gap->valueint,
        bodyList
        );

    return stream_snapshot_local_var;
end:
    if (bodyList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, bodyList) {
            keyValuePair_t *localKeyValue = listEntry->data;
            free(localKeyValue->key);
            localKeyValue->key = NULL;
            keyValuePair_free(localKeyValue);
            localKeyValue = NULL;
        }
        list_freeList(bodyList);
        bodyList = NULL;
    }
    return NULL;

}
