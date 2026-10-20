/*
 * wifi_network.h
 *
 * One network discovered by the asynchronous station scan. The setup AP itself remains open.
 */

#ifndef _wifi_network_H_
#define _wifi_network_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct wifi_network_t wifi_network_t;




typedef struct wifi_network_t {
    char *ssid; // string
    long rssi_dbm; //numeric
    int secured; //boolean

    int _library_owned; // Is the library responsible for freeing this object?
} wifi_network_t;

__attribute__((deprecated)) wifi_network_t *wifi_network_create(
    char *ssid,
    long rssi_dbm,
    int secured
);

void wifi_network_free(wifi_network_t *wifi_network);

wifi_network_t *wifi_network_parseFromJSON(cJSON *wifi_networkJSON);

cJSON *wifi_network_convertToJSON(wifi_network_t *wifi_network);

#endif /* _wifi_network_H_ */

