/*
 * node_info.h
 *
 * 
 */

#ifndef _node_info_H_
#define _node_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct node_info_t node_info_t;




typedef struct node_info_t {
    char *id; // string
    char *boot_id; // string
    char *firmware_version; // string
    char *board; // string
    char *api_version; // string

    int _library_owned; // Is the library responsible for freeing this object?
} node_info_t;

__attribute__((deprecated)) node_info_t *node_info_create(
    char *id,
    char *boot_id,
    char *firmware_version,
    char *board,
    char *api_version
);

void node_info_free(node_info_t *node_info);

node_info_t *node_info_parseFromJSON(cJSON *node_infoJSON);

cJSON *node_info_convertToJSON(node_info_t *node_info);

#endif /* _node_info_H_ */

