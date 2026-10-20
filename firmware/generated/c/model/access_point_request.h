/*
 * access_point_request.h
 *
 * AP security only. Does not change station credentials or configured AP SSID. Empty PSK restores the default open AP; a non-empty printable ASCII passphrase enables WPA2-PSK/CCMP. The secret is persisted locally and never returned.
 */

#ifndef _access_point_request_H_
#define _access_point_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct access_point_request_t access_point_request_t;




typedef struct access_point_request_t {
    char *psk; // string

    int _library_owned; // Is the library responsible for freeing this object?
} access_point_request_t;

__attribute__((deprecated)) access_point_request_t *access_point_request_create(
    char *psk
);

void access_point_request_free(access_point_request_t *access_point_request);

access_point_request_t *access_point_request_parseFromJSON(cJSON *access_point_requestJSON);

cJSON *access_point_request_convertToJSON(access_point_request_t *access_point_request);

#endif /* _access_point_request_H_ */

