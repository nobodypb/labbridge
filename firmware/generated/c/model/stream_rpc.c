#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stream_rpc.h"


char* stream_rpc_method_ToString(labbridge_api_stream_rpc_METHOD_e method) {
    char* methodArray[] =  { "NULL", "GET", "POST", "PUT", "DELETE" };
    return methodArray[method];
}

labbridge_api_stream_rpc_METHOD_e stream_rpc_method_FromString(char* method){
    int stringToReturn = 0;
    char *methodArray[] =  { "NULL", "GET", "POST", "PUT", "DELETE" };
    size_t sizeofArray = sizeof(methodArray) / sizeof(methodArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(method, methodArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static stream_rpc_t *stream_rpc_create_internal(
    char *id,
    labbridge_api_stream_rpc_METHOD_e method,
    char *path,
    list_t* headers,
    list_t* body
    ) {
    stream_rpc_t *stream_rpc_local_var = malloc(sizeof(stream_rpc_t));
    if (!stream_rpc_local_var) {
        return NULL;
    }
    stream_rpc_local_var->id = id;
    stream_rpc_local_var->method = method;
    stream_rpc_local_var->path = path;
    stream_rpc_local_var->headers = headers;
    stream_rpc_local_var->body = body;

    stream_rpc_local_var->_library_owned = 1;
    return stream_rpc_local_var;
}

__attribute__((deprecated)) stream_rpc_t *stream_rpc_create(
    char *id,
    labbridge_api_stream_rpc_METHOD_e method,
    char *path,
    list_t* headers,
    list_t* body
    ) {
    return stream_rpc_create_internal (
        id,
        method,
        path,
        headers,
        body
        );
}

void stream_rpc_free(stream_rpc_t *stream_rpc) {
    if(NULL == stream_rpc){
        return ;
    }
    if(stream_rpc->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "stream_rpc_free");
        return ;
    }
    listEntry_t *listEntry;
    if (stream_rpc->id) {
        free(stream_rpc->id);
        stream_rpc->id = NULL;
    }
    if (stream_rpc->path) {
        free(stream_rpc->path);
        stream_rpc->path = NULL;
    }
    if (stream_rpc->headers) {
        list_ForEach(listEntry, stream_rpc->headers) {
            keyValuePair_t *localKeyValue = listEntry->data;
            free (localKeyValue->key);
            free (localKeyValue->value);
            keyValuePair_free(localKeyValue);
        }
        list_freeList(stream_rpc->headers);
        stream_rpc->headers = NULL;
    }
    if (stream_rpc->body) {
        list_ForEach(listEntry, stream_rpc->body) {
            keyValuePair_t *localKeyValue = listEntry->data;
            free (localKeyValue->key);
            free (localKeyValue->value);
            keyValuePair_free(localKeyValue);
        }
        list_freeList(stream_rpc->body);
        stream_rpc->body = NULL;
    }
    free(stream_rpc);
}

cJSON *stream_rpc_convertToJSON(stream_rpc_t *stream_rpc) {
    cJSON *item = cJSON_CreateObject();

    // stream_rpc->id
    if (!stream_rpc->id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "id", stream_rpc->id) == NULL) {
    goto fail; //String
    }


    // stream_rpc->method
    if (labbridge_api_stream_rpc_METHOD_NULL == stream_rpc->method) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "method", stream_rpc_method_ToString(stream_rpc->method)) == NULL)
    {
    goto fail; //Enum
    }


    // stream_rpc->path
    if (!stream_rpc->path) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "path", stream_rpc->path) == NULL) {
    goto fail; //String
    }


    // stream_rpc->headers
    if(stream_rpc->headers) {
    cJSON *headers = cJSON_AddObjectToObject(item, "headers");
    if(headers == NULL) {
        goto fail; //primitive map container
    }
    cJSON *localMapObject = headers;
    listEntry_t *headersListEntry;
    if (stream_rpc->headers) {
    list_ForEach(headersListEntry, stream_rpc->headers) {
        keyValuePair_t *localKeyValue = headersListEntry->data;
        if(cJSON_AddStringToObject(localMapObject, localKeyValue->key, localKeyValue->value) == NULL)
        {
            goto fail;
        }
    }
    }
    }


    // stream_rpc->body
    if(stream_rpc->body) {
    cJSON *body = cJSON_AddObjectToObject(item, "body");
    if(body == NULL) {
        goto fail; //primitive map container
    }
    cJSON *localMapObject = body;
    listEntry_t *bodyListEntry;
    if (stream_rpc->body) {
    list_ForEach(bodyListEntry, stream_rpc->body) {
        keyValuePair_t *localKeyValue = bodyListEntry->data;
    }
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

stream_rpc_t *stream_rpc_parseFromJSON(cJSON *stream_rpcJSON){

    stream_rpc_t *stream_rpc_local_var = NULL;

    // define the local map for stream_rpc->headers
    list_t *headersList = NULL;

    // define the local map for stream_rpc->body
    list_t *bodyList = NULL;

    // stream_rpc->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(stream_rpcJSON, "id");
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

    // stream_rpc->method
    cJSON *method = cJSON_GetObjectItemCaseSensitive(stream_rpcJSON, "method");
    if (cJSON_IsNull(method)) {
        method = NULL;
    }
    if (!method) {
        goto end;
    }

    labbridge_api_stream_rpc_METHOD_e methodVariable;
    
    if(!cJSON_IsString(method))
    {
    goto end; //Enum
    }
    methodVariable = stream_rpc_method_FromString(method->valuestring);

    // stream_rpc->path
    cJSON *path = cJSON_GetObjectItemCaseSensitive(stream_rpcJSON, "path");
    if (cJSON_IsNull(path)) {
        path = NULL;
    }
    if (!path) {
        goto end;
    }

    
    if(!cJSON_IsString(path))
    {
    goto end; //String
    }

    // stream_rpc->headers
    cJSON *headers = cJSON_GetObjectItemCaseSensitive(stream_rpcJSON, "headers");
    if (cJSON_IsNull(headers)) {
        headers = NULL;
    }
    if (headers) { 
    cJSON *headers_local_map = NULL;
    if(!cJSON_IsObject(headers) && !cJSON_IsNull(headers))
    {
        goto end;//primitive map container
    }
    if(cJSON_IsObject(headers))
    {
        headersList = list_createList();
        keyValuePair_t *localMapKeyPair;
        cJSON_ArrayForEach(headers_local_map, headers)
        {
            cJSON *localMapObject = headers_local_map;
            if(!cJSON_IsString(localMapObject))
            {
                goto end;
            }
            localMapKeyPair = keyValuePair_create(strdup(localMapObject->string),strdup(localMapObject->valuestring));
            list_addElement(headersList , localMapKeyPair);
        }
    }
    }

    // stream_rpc->body
    cJSON *body = cJSON_GetObjectItemCaseSensitive(stream_rpcJSON, "body");
    if (cJSON_IsNull(body)) {
        body = NULL;
    }
    if (body) { 
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
    }


    stream_rpc_local_var = stream_rpc_create_internal (
        strdup(id->valuestring),
        methodVariable,
        strdup(path->valuestring),
        headers ? headersList : NULL,
        body ? bodyList : NULL
        );

    return stream_rpc_local_var;
end:
    if (headersList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, headersList) {
            keyValuePair_t *localKeyValue = listEntry->data;
            free(localKeyValue->key);
            localKeyValue->key = NULL;
            free(localKeyValue->value);
            localKeyValue->value = NULL;
            keyValuePair_free(localKeyValue);
            localKeyValue = NULL;
        }
        list_freeList(headersList);
        headersList = NULL;
    }
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
