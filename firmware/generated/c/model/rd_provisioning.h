/*
 * rd_provisioning.h
 *
 * Provisioning and AP transport status. One session at a time; each sending phase and connection-wait phase expires after 120 seconds. The AP is open, server address 192.168.4.1, TCP port 8080. The original PSU initiates the connection; payloads are CRC-protected Modbus RTU, not MBAP-framed Modbus TCP. No NAT/Internet access is required.
 */

#ifndef _rd_provisioning_H_
#define _rd_provisioning_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct rd_provisioning_t rd_provisioning_t;

#include "rd_wifi_candidate.h"
#include "rd_wifi_metrics.h"

// Enum STATE for rd_provisioning

typedef enum  { labbridge_api_rd_provisioning_STATE_NULL = 0, labbridge_api_rd_provisioning_STATE_idle, labbridge_api_rd_provisioning_STATE_server_address, labbridge_api_rd_provisioning_STATE_wifi, labbridge_api_rd_provisioning_STATE_waiting_connection, labbridge_api_rd_provisioning_STATE_complete, labbridge_api_rd_provisioning_STATE_cancelled, labbridge_api_rd_provisioning_STATE_timed_out, labbridge_api_rd_provisioning_STATE_failed } labbridge_api_rd_provisioning_STATE_e;

char* rd_provisioning_state_ToString(labbridge_api_rd_provisioning_STATE_e state);

labbridge_api_rd_provisioning_STATE_e rd_provisioning_state_FromString(char* state);



typedef struct rd_provisioning_t {
    labbridge_api_rd_provisioning_STATE_e state; //enum
    char *session_id; // string
    long deadline_ms; //numeric
    char *ap_ssid; // string
    char *ap_address; // string
    long tcp_port; //numeric
    int listener_running; //boolean
    list_t *candidates; //nonprimitive container
    struct rd_wifi_metrics_t *metrics; //model
    char *error; // string

    int _library_owned; // Is the library responsible for freeing this object?
} rd_provisioning_t;

__attribute__((deprecated)) rd_provisioning_t *rd_provisioning_create(
    labbridge_api_rd_provisioning_STATE_e state,
    char *session_id,
    long deadline_ms,
    char *ap_ssid,
    char *ap_address,
    long tcp_port,
    int listener_running,
    list_t *candidates,
    rd_wifi_metrics_t *metrics,
    char *error
);

void rd_provisioning_free(rd_provisioning_t *rd_provisioning);

rd_provisioning_t *rd_provisioning_parseFromJSON(cJSON *rd_provisioningJSON);

cJSON *rd_provisioning_convertToJSON(rd_provisioning_t *rd_provisioning);

#endif /* _rd_provisioning_H_ */

