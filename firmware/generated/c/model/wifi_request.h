/*
 * wifi_request.h
 *
 * Target station credentials, independent of AP security. Credentials are persisted locally and never returned by the state API.
 */

#ifndef _wifi_request_H_
#define _wifi_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct wifi_request_t wifi_request_t;




typedef struct wifi_request_t {
    char *ssid; // string
    char *psk; // string

    int _library_owned; // Is the library responsible for freeing this object?
} wifi_request_t;

__attribute__((deprecated)) wifi_request_t *wifi_request_create(
    char *ssid,
    char *psk
);

void wifi_request_free(wifi_request_t *wifi_request);

wifi_request_t *wifi_request_parseFromJSON(cJSON *wifi_requestJSON);

cJSON *wifi_request_convertToJSON(wifi_request_t *wifi_request);

#endif /* _wifi_request_H_ */

