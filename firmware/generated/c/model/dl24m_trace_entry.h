/*
 * dl24m_trace_entry.h
 *
 * One actual BLE transport byte chunk. Receive chunks can contain partial or multiple frames; do not reinterpret as HID.
 */

#ifndef _dl24m_trace_entry_H_
#define _dl24m_trace_entry_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct dl24m_trace_entry_t dl24m_trace_entry_t;


// Enum DIRECTION for dl24m_trace_entry

typedef enum  { labbridge_api_dl24m_trace_entry_DIRECTION_NULL = 0, labbridge_api_dl24m_trace_entry_DIRECTION_tx, labbridge_api_dl24m_trace_entry_DIRECTION_rx } labbridge_api_dl24m_trace_entry_DIRECTION_e;

char* dl24m_trace_entry_direction_ToString(labbridge_api_dl24m_trace_entry_DIRECTION_e direction);

labbridge_api_dl24m_trace_entry_DIRECTION_e dl24m_trace_entry_direction_FromString(char* direction);

// Enum ORIGIN for dl24m_trace_entry

typedef enum  { labbridge_api_dl24m_trace_entry_ORIGIN_NULL = 0, labbridge_api_dl24m_trace_entry_ORIGIN_ble_stack, labbridge_api_dl24m_trace_entry_ORIGIN_endpoint } labbridge_api_dl24m_trace_entry_ORIGIN_e;

char* dl24m_trace_entry_origin_ToString(labbridge_api_dl24m_trace_entry_ORIGIN_e origin);

labbridge_api_dl24m_trace_entry_ORIGIN_e dl24m_trace_entry_origin_FromString(char* origin);



typedef struct dl24m_trace_entry_t {
    int sequence; //numeric
    int received_ms; //numeric
    int generation; //numeric
    labbridge_api_dl24m_trace_entry_DIRECTION_e direction; //enum
    char *hex; // string
    int truncated; //boolean
    labbridge_api_dl24m_trace_entry_ORIGIN_e origin; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} dl24m_trace_entry_t;

__attribute__((deprecated)) dl24m_trace_entry_t *dl24m_trace_entry_create(
    int sequence,
    int received_ms,
    int generation,
    labbridge_api_dl24m_trace_entry_DIRECTION_e direction,
    char *hex,
    int truncated,
    labbridge_api_dl24m_trace_entry_ORIGIN_e origin
);

void dl24m_trace_entry_free(dl24m_trace_entry_t *dl24m_trace_entry);

dl24m_trace_entry_t *dl24m_trace_entry_parseFromJSON(cJSON *dl24m_trace_entryJSON);

cJSON *dl24m_trace_entry_convertToJSON(dl24m_trace_entry_t *dl24m_trace_entry);

#endif /* _dl24m_trace_entry_H_ */

