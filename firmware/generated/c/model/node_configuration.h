/*
 * node_configuration.h
 *
 * Complete persistent bridge configuration. Replace conditionally using the current ETag; the node assigns the next revision and reboots after accepting a replacement.
 */

#ifndef _node_configuration_H_
#define _node_configuration_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct node_configuration_t node_configuration_t;

#include "device_configuration.h"



typedef struct node_configuration_t {
    long revision; //numeric
    char *node_id; // string
    char *board; // string
    list_t *devices; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} node_configuration_t;

__attribute__((deprecated)) node_configuration_t *node_configuration_create(
    long revision,
    char *node_id,
    char *board,
    list_t *devices
);

void node_configuration_free(node_configuration_t *node_configuration);

node_configuration_t *node_configuration_parseFromJSON(cJSON *node_configurationJSON);

cJSON *node_configuration_convertToJSON(node_configuration_t *node_configuration);

#endif /* _node_configuration_H_ */

