/*
 * rd_wifi_candidate.h
 *
 * Read-only identity discovered on an incoming AP connection. Discovery does not authorize writes; explicitly add the serial/model and positive operating limits to configuration.
 */

#ifndef _rd_wifi_candidate_H_
#define _rd_wifi_candidate_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct rd_wifi_candidate_t rd_wifi_candidate_t;




typedef struct rd_wifi_candidate_t {
    char *serial; // string
    char *model; // string
    long model_id; //numeric
    long firmware_version; //numeric
    int connected; //boolean
    int configured; //boolean

    int _library_owned; // Is the library responsible for freeing this object?
} rd_wifi_candidate_t;

__attribute__((deprecated)) rd_wifi_candidate_t *rd_wifi_candidate_create(
    char *serial,
    char *model,
    long model_id,
    long firmware_version,
    int connected,
    int configured
);

void rd_wifi_candidate_free(rd_wifi_candidate_t *rd_wifi_candidate);

rd_wifi_candidate_t *rd_wifi_candidate_parseFromJSON(cJSON *rd_wifi_candidateJSON);

cJSON *rd_wifi_candidate_convertToJSON(rd_wifi_candidate_t *rd_wifi_candidate);

#endif /* _rd_wifi_candidate_H_ */

