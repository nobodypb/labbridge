/*
 * stream_auth.h
 *
 * First WebSocket text message within three seconds, with exactly one credential: API bearer token, or the UI session CSRF token bound to the HttpOnly upgrade cookie and trusted same Origin. Cookie sessions are revalidated during RPC and pushes; logout, expiry and password replacement terminate access. Secrets never enter URLs or subprotocols.
 */

#ifndef _stream_auth_H_
#define _stream_auth_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct stream_auth_t stream_auth_t;


// Enum TYPE for stream_auth

typedef enum  { labbridge_api_stream_auth_TYPE_NULL = 0, labbridge_api_stream_auth_TYPE_auth } labbridge_api_stream_auth_TYPE_e;

char* stream_auth_type_ToString(labbridge_api_stream_auth_TYPE_e type);

labbridge_api_stream_auth_TYPE_e stream_auth_type_FromString(char* type);



typedef struct stream_auth_t {
    labbridge_api_stream_auth_TYPE_e type; //enum
    char *token; // string
    char *csrf_token; // string

    int _library_owned; // Is the library responsible for freeing this object?
} stream_auth_t;

__attribute__((deprecated)) stream_auth_t *stream_auth_create(
    labbridge_api_stream_auth_TYPE_e type,
    char *token,
    char *csrf_token
);

void stream_auth_free(stream_auth_t *stream_auth);

stream_auth_t *stream_auth_parseFromJSON(cJSON *stream_authJSON);

cJSON *stream_auth_convertToJSON(stream_auth_t *stream_auth);

#endif /* _stream_auth_H_ */

