/*
 * stream_subscription.h
 *
 * Select one device and measurement interval. Snapshot also contains output, settings and tracked operations. Ping/open socket does not renew leases.
 */

#ifndef _stream_subscription_H_
#define _stream_subscription_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct stream_subscription_t stream_subscription_t;


// Enum TYPE for stream_subscription

typedef enum  { labbridge_api_stream_subscription_TYPE_NULL = 0, labbridge_api_stream_subscription_TYPE_subscribe } labbridge_api_stream_subscription_TYPE_e;

char* stream_subscription_type_ToString(labbridge_api_stream_subscription_TYPE_e type);

labbridge_api_stream_subscription_TYPE_e stream_subscription_type_FromString(char* type);

// Enum INTERVALMS for stream_subscription

typedef enum  { labbridge_api_stream_subscription_INTERVALMS_NULL = 0, labbridge_api_stream_subscription_INTERVALMS__500, labbridge_api_stream_subscription_INTERVALMS__1000, labbridge_api_stream_subscription_INTERVALMS__2000 } labbridge_api_stream_subscription_INTERVALMS_e;

char* stream_subscription_interval_ms_ToString(labbridge_api_stream_subscription_INTERVALMS_e interval_ms);

labbridge_api_stream_subscription_INTERVALMS_e stream_subscription_interval_ms_FromString(char* interval_ms);



typedef struct stream_subscription_t {
    labbridge_api_stream_subscription_TYPE_e type; //enum
    char *device_id; // string
    int interval_ms; //numeric
    char *id; // string

    int _library_owned; // Is the library responsible for freeing this object?
} stream_subscription_t;

__attribute__((deprecated)) stream_subscription_t *stream_subscription_create(
    labbridge_api_stream_subscription_TYPE_e type,
    char *device_id,
    int interval_ms,
    char *id
);

void stream_subscription_free(stream_subscription_t *stream_subscription);

stream_subscription_t *stream_subscription_parseFromJSON(cJSON *stream_subscriptionJSON);

cJSON *stream_subscription_convertToJSON(stream_subscription_t *stream_subscription);

#endif /* _stream_subscription_H_ */

