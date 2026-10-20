/*
 * component_list.h
 *
 * All actor/sensor descriptors for one physical device.
 */

#ifndef _component_list_H_
#define _component_list_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct component_list_t component_list_t;

#include "component.h"



typedef struct component_list_t {
    list_t *components; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} component_list_t;

__attribute__((deprecated)) component_list_t *component_list_create(
    list_t *components
);

void component_list_free(component_list_t *component_list);

component_list_t *component_list_parseFromJSON(cJSON *component_listJSON);

cJSON *component_list_convertToJSON(component_list_t *component_list);

#endif /* _component_list_H_ */

