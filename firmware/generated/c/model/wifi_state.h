/*
 * wifi_state.h
 *
 * Station provisioning/connection snapshot. A successful credential-save response does not imply that the station is already connected.
 */

#ifndef _wifi_state_H_
#define _wifi_state_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct wifi_state_t wifi_state_t;


// Enum STATE for wifi_state

typedef enum  { labbridge_api_wifi_state_STATE_NULL = 0, labbridge_api_wifi_state_STATE_setup, labbridge_api_wifi_state_STATE_connecting, labbridge_api_wifi_state_STATE_connected, labbridge_api_wifi_state_STATE_failed } labbridge_api_wifi_state_STATE_e;

char* wifi_state_state_ToString(labbridge_api_wifi_state_STATE_e state);

labbridge_api_wifi_state_STATE_e wifi_state_state_FromString(char* state);



typedef struct wifi_state_t {
    labbridge_api_wifi_state_STATE_e state; //enum
    char *hostname; // string
    char *address; // string

    int _library_owned; // Is the library responsible for freeing this object?
} wifi_state_t;

__attribute__((deprecated)) wifi_state_t *wifi_state_create(
    labbridge_api_wifi_state_STATE_e state,
    char *hostname,
    char *address
);

void wifi_state_free(wifi_state_t *wifi_state);

wifi_state_t *wifi_state_parseFromJSON(cJSON *wifi_stateJSON);

cJSON *wifi_state_convertToJSON(wifi_state_t *wifi_state);

#endif /* _wifi_state_H_ */

