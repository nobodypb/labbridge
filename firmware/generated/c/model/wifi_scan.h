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



typedef struct wifi_scan_t {
    list_t *networks; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} wifi_scan_t;

__attribute__((deprecated)) wifi_scan_t *wifi_scan_create(
    list_t *networks
);

void wifi_scan_free(wifi_scan_t *wifi_scan);

wifi_scan_t *wifi_scan_parseFromJSON(cJSON *wifi_scanJSON);

cJSON *wifi_scan_convertToJSON(wifi_scan_t *wifi_scan);

#endif /* _wifi_scan_H_ */

