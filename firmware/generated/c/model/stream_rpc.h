/*
 * stream_rpc.h
 *
 * USB-compatible request envelope. API routes, validation, leases, operation execution and idempotency are shared; no write replay on reconnect.
 */

#ifndef _stream_rpc_H_
#define _stream_rpc_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct stream_rpc_t stream_rpc_t;

#include "any_type.h"

// Enum METHOD for stream_rpc

typedef enum  { labbridge_api_stream_rpc_METHOD_NULL = 0, labbridge_api_stream_rpc_METHOD_GET, labbridge_api_stream_rpc_METHOD_POST, labbridge_api_stream_rpc_METHOD_PUT, labbridge_api_stream_rpc_METHOD__DELETE } labbridge_api_stream_rpc_METHOD_e;

char* stream_rpc_method_ToString(labbridge_api_stream_rpc_METHOD_e method);

labbridge_api_stream_rpc_METHOD_e stream_rpc_method_FromString(char* method);



typedef struct stream_rpc_t {
    char *id; // string
    labbridge_api_stream_rpc_METHOD_e method; //enum
    char *path; // string
    list_t* headers; //map
    list_t* body; //map

    int _library_owned; // Is the library responsible for freeing this object?
} stream_rpc_t;

__attribute__((deprecated)) stream_rpc_t *stream_rpc_create(
    char *id,
    labbridge_api_stream_rpc_METHOD_e method,
    char *path,
    list_t* headers,
    list_t* body
);

void stream_rpc_free(stream_rpc_t *stream_rpc);

stream_rpc_t *stream_rpc_parseFromJSON(cJSON *stream_rpcJSON);

cJSON *stream_rpc_convertToJSON(stream_rpc_t *stream_rpc);

#endif /* _stream_rpc_H_ */

