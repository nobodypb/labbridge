/*
 * device_list.h
 *
 * Configured local physical instruments, including currently unavailable devices.
 */

#ifndef _device_list_H_
#define _device_list_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct device_list_t device_list_t;

#include "device.h"



typedef struct device_list_t {
    list_t *devices; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} device_list_t;

__attribute__((deprecated)) device_list_t *device_list_create(
    list_t *devices
);

void device_list_free(device_list_t *device_list);

device_list_t *device_list_parseFromJSON(cJSON *device_listJSON);

cJSON *device_list_convertToJSON(device_list_t *device_list);

#endif /* _device_list_H_ */

