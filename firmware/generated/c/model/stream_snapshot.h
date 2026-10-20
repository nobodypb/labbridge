/*
 * stream_snapshot.h
 *
 * Full initial snapshot after subscription, then cached state/settings/operation snapshots with periodic measurements. Node/device catalogue is repeated every five seconds and may be absent from intervening snapshots. sequence increases per offered snapshot; gap reports coalescing or skipped intervals. Each measurement keeps its native sequence/receive time. Boot change invalidates all leases and operations.
 */

#ifndef _stream_snapshot_H_
#define _stream_snapshot_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct stream_snapshot_t stream_snapshot_t;

#include "any_type.h"

// Enum TYPE for stream_snapshot

typedef enum  { labbridge_api_stream_snapshot_TYPE_NULL = 0, labbridge_api_stream_snapshot_TYPE_snapshot } labbridge_api_stream_snapshot_TYPE_e;

char* stream_snapshot_type_ToString(labbridge_api_stream_snapshot_TYPE_e type);

labbridge_api_stream_snapshot_TYPE_e stream_snapshot_type_FromString(char* type);



typedef struct stream_snapshot_t {
    labbridge_api_stream_snapshot_TYPE_e type; //enum
    char *boot_id; // string
    int sequence; //numeric
    int gap; //boolean
    list_t* body; //map

    int _library_owned; // Is the library responsible for freeing this object?
} stream_snapshot_t;

__attribute__((deprecated)) stream_snapshot_t *stream_snapshot_create(
    labbridge_api_stream_snapshot_TYPE_e type,
    char *boot_id,
    int sequence,
    int gap,
    list_t* body
);

void stream_snapshot_free(stream_snapshot_t *stream_snapshot);

stream_snapshot_t *stream_snapshot_parseFromJSON(cJSON *stream_snapshotJSON);

cJSON *stream_snapshot_convertToJSON(stream_snapshot_t *stream_snapshot);

#endif /* _stream_snapshot_H_ */

