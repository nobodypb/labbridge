/*
 * component.h
 *
 * Discoverable source, sink or scalar sensor within one physical device. Actions describe supported mutations; sensors expose quantity/unit instead. IDs need only be unique within the device.
 */

#ifndef _component_H_
#define _component_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct component_t component_t;


// Enum KIND for component

typedef enum  { labbridge_api_component_KIND_NULL = 0, labbridge_api_component_KIND_source, labbridge_api_component_KIND_sink, labbridge_api_component_KIND_sensor } labbridge_api_component_KIND_e;

char* component_kind_ToString(labbridge_api_component_KIND_e kind);

labbridge_api_component_KIND_e component_kind_FromString(char* kind);

// Enum TEMPERATUREKIND for component

typedef enum  { labbridge_api_component_TEMPERATUREKIND_NULL = 0, labbridge_api_component_TEMPERATUREKIND_internal, labbridge_api_component_TEMPERATUREKIND_external_probe, labbridge_api_component_TEMPERATUREKIND_cell } labbridge_api_component_TEMPERATUREKIND_e;

char* component_temperature_kind_ToString(labbridge_api_component_TEMPERATUREKIND_e temperature_kind);

labbridge_api_component_TEMPERATUREKIND_e component_temperature_kind_FromString(char* temperature_kind);



typedef struct component_t {
    char *id; // string
    labbridge_api_component_KIND_e kind; //enum
    list_t *actions; //primitive container
    char *quantity; // string
    char *unit; // string
    labbridge_api_component_TEMPERATUREKIND_e temperature_kind; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} component_t;

__attribute__((deprecated)) component_t *component_create(
    char *id,
    labbridge_api_component_KIND_e kind,
    list_t *actions,
    char *quantity,
    char *unit,
    labbridge_api_component_TEMPERATUREKIND_e temperature_kind
);

void component_free(component_t *component);

component_t *component_parseFromJSON(cJSON *componentJSON);

cJSON *component_convertToJSON(component_t *component);

#endif /* _component_H_ */

