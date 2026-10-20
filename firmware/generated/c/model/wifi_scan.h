/*
 * wifi_scan.h
 *
 * 
 */

#ifndef _wifi_scan_H_
#define _wifi_scan_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct wifi_scan_t wifi_scan_t;

#include "wifi_network.h"

// Enum STATE for wifi_scan

typedef enum  { labbridge_api_wifi_scan_STATE_NULL = 0, labbridge_api_wifi_scan_STATE_scanning, labbridge_api_wifi_scan_STATE_complete, labbridge_api_wifi_scan_STATE_failed } labbridge_api_wifi_scan_STATE_e;

char* wifi_scan_state_ToString(labbridge_api_wifi_scan_STATE_e state);

labbridge_api_wifi_scan_STATE_e wifi_scan_state_FromString(char* state);



typedef struct wifi_scan_t {
    list_t *networks; //nonprimitive container
    labbridge_api_wifi_scan_STATE_e state; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} wifi_scan_t;

__attribute__((deprecated)) wifi_scan_t *wifi_scan_create(
    list_t *networks,
    labbridge_api_wifi_scan_STATE_e state
);

void wifi_scan_free(wifi_scan_t *wifi_scan);

wifi_scan_t *wifi_scan_parseFromJSON(cJSON *wifi_scanJSON);

cJSON *wifi_scan_convertToJSON(wifi_scan_t *wifi_scan);

#endif /* _wifi_scan_H_ */

