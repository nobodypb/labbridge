/*
 * yocto_watt_configuration.h
 *
 * 
 */

#ifndef _yocto_watt_configuration_H_
#define _yocto_watt_configuration_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct yocto_watt_configuration_t yocto_watt_configuration_t;




typedef struct yocto_watt_configuration_t {
    char *hub_url; // string

    int _library_owned; // Is the library responsible for freeing this object?
} yocto_watt_configuration_t;

__attribute__((deprecated)) yocto_watt_configuration_t *yocto_watt_configuration_create(
    char *hub_url
);

void yocto_watt_configuration_free(yocto_watt_configuration_t *yocto_watt_configuration);

yocto_watt_configuration_t *yocto_watt_configuration_parseFromJSON(cJSON *yocto_watt_configurationJSON);

cJSON *yocto_watt_configuration_convertToJSON(yocto_watt_configuration_t *yocto_watt_configuration);

#endif /* _yocto_watt_configuration_H_ */

