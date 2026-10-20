/*
 * rd_wifi_metrics.h
 *
 * Boot-local transport counters for commissioning. Unexpected disconnects/rejections and gaps in fresh measurements must be investigated; these counters do not prove output OFF.
 */

#ifndef _rd_wifi_metrics_H_
#define _rd_wifi_metrics_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct rd_wifi_metrics_t rd_wifi_metrics_t;




typedef struct rd_wifi_metrics_t {
    long accepted; //numeric
    long disconnects; //numeric
    long rejected; //numeric
    long rx_bytes; //numeric
    long tx_bytes; //numeric
    long provision_packets; //numeric
    long provision_tx_completed; //numeric
    long provision_tx_failed; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} rd_wifi_metrics_t;

__attribute__((deprecated)) rd_wifi_metrics_t *rd_wifi_metrics_create(
    long accepted,
    long disconnects,
    long rejected,
    long rx_bytes,
    long tx_bytes,
    long provision_packets,
    long provision_tx_completed,
    long provision_tx_failed
);

void rd_wifi_metrics_free(rd_wifi_metrics_t *rd_wifi_metrics);

rd_wifi_metrics_t *rd_wifi_metrics_parseFromJSON(cJSON *rd_wifi_metricsJSON);

cJSON *rd_wifi_metrics_convertToJSON(rd_wifi_metrics_t *rd_wifi_metrics);

#endif /* _rd_wifi_metrics_H_ */

