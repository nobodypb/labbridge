/*
 * ina228_configuration.h
 *
 * 
 */

#ifndef _ina228_configuration_H_
#define _ina228_configuration_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct ina228_configuration_t ina228_configuration_t;




typedef struct ina228_configuration_t {
    double shunt_ohms; //numeric
    double max_current_a; //numeric
    int adc_range; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} ina228_configuration_t;

__attribute__((deprecated)) ina228_configuration_t *ina228_configuration_create(
    double shunt_ohms,
    double max_current_a,
    int adc_range
);

void ina228_configuration_free(ina228_configuration_t *ina228_configuration);

ina228_configuration_t *ina228_configuration_parseFromJSON(cJSON *ina228_configurationJSON);

cJSON *ina228_configuration_convertToJSON(ina228_configuration_t *ina228_configuration);

#endif /* _ina228_configuration_H_ */

