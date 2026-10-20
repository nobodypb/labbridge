#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rd_wifi_metrics.h"



static rd_wifi_metrics_t *rd_wifi_metrics_create_internal(
    long accepted,
    long disconnects,
    long rejected,
    long rx_bytes,
    long tx_bytes,
    long provision_packets
    ) {
    rd_wifi_metrics_t *rd_wifi_metrics_local_var = malloc(sizeof(rd_wifi_metrics_t));
    if (!rd_wifi_metrics_local_var) {
        return NULL;
    }
    rd_wifi_metrics_local_var->accepted = accepted;
    rd_wifi_metrics_local_var->disconnects = disconnects;
    rd_wifi_metrics_local_var->rejected = rejected;
    rd_wifi_metrics_local_var->rx_bytes = rx_bytes;
    rd_wifi_metrics_local_var->tx_bytes = tx_bytes;
    rd_wifi_metrics_local_var->provision_packets = provision_packets;

    rd_wifi_metrics_local_var->_library_owned = 1;
    return rd_wifi_metrics_local_var;
}

__attribute__((deprecated)) rd_wifi_metrics_t *rd_wifi_metrics_create(
    long accepted,
    long disconnects,
    long rejected,
    long rx_bytes,
    long tx_bytes,
    long provision_packets
    ) {
    return rd_wifi_metrics_create_internal (
        accepted,
        disconnects,
        rejected,
        rx_bytes,
        tx_bytes,
        provision_packets
        );
}

void rd_wifi_metrics_free(rd_wifi_metrics_t *rd_wifi_metrics) {
    if(NULL == rd_wifi_metrics){
        return ;
    }
    if(rd_wifi_metrics->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "rd_wifi_metrics_free");
        return ;
    }
    listEntry_t *listEntry;
    free(rd_wifi_metrics);
}

cJSON *rd_wifi_metrics_convertToJSON(rd_wifi_metrics_t *rd_wifi_metrics) {
    cJSON *item = cJSON_CreateObject();

    // rd_wifi_metrics->accepted
    if (!rd_wifi_metrics->accepted) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "accepted", rd_wifi_metrics->accepted) == NULL) {
    goto fail; //Numeric
    }


    // rd_wifi_metrics->disconnects
    if (!rd_wifi_metrics->disconnects) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "disconnects", rd_wifi_metrics->disconnects) == NULL) {
    goto fail; //Numeric
    }


    // rd_wifi_metrics->rejected
    if (!rd_wifi_metrics->rejected) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "rejected", rd_wifi_metrics->rejected) == NULL) {
    goto fail; //Numeric
    }


    // rd_wifi_metrics->rx_bytes
    if (!rd_wifi_metrics->rx_bytes) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "rx_bytes", rd_wifi_metrics->rx_bytes) == NULL) {
    goto fail; //Numeric
    }


    // rd_wifi_metrics->tx_bytes
    if (!rd_wifi_metrics->tx_bytes) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "tx_bytes", rd_wifi_metrics->tx_bytes) == NULL) {
    goto fail; //Numeric
    }


    // rd_wifi_metrics->provision_packets
    if (!rd_wifi_metrics->provision_packets) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "provision_packets", rd_wifi_metrics->provision_packets) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

rd_wifi_metrics_t *rd_wifi_metrics_parseFromJSON(cJSON *rd_wifi_metricsJSON){

    rd_wifi_metrics_t *rd_wifi_metrics_local_var = NULL;

    // rd_wifi_metrics->accepted
    cJSON *accepted = cJSON_GetObjectItemCaseSensitive(rd_wifi_metricsJSON, "accepted");
    if (cJSON_IsNull(accepted)) {
        accepted = NULL;
    }
    if (!accepted) {
        goto end;
    }

    
    if(!cJSON_IsNumber(accepted))
    {
    goto end; //Numeric
    }

    // rd_wifi_metrics->disconnects
    cJSON *disconnects = cJSON_GetObjectItemCaseSensitive(rd_wifi_metricsJSON, "disconnects");
    if (cJSON_IsNull(disconnects)) {
        disconnects = NULL;
    }
    if (!disconnects) {
        goto end;
    }

    
    if(!cJSON_IsNumber(disconnects))
    {
    goto end; //Numeric
    }

    // rd_wifi_metrics->rejected
    cJSON *rejected = cJSON_GetObjectItemCaseSensitive(rd_wifi_metricsJSON, "rejected");
    if (cJSON_IsNull(rejected)) {
        rejected = NULL;
    }
    if (!rejected) {
        goto end;
    }

    
    if(!cJSON_IsNumber(rejected))
    {
    goto end; //Numeric
    }

    // rd_wifi_metrics->rx_bytes
    cJSON *rx_bytes = cJSON_GetObjectItemCaseSensitive(rd_wifi_metricsJSON, "rx_bytes");
    if (cJSON_IsNull(rx_bytes)) {
        rx_bytes = NULL;
    }
    if (!rx_bytes) {
        goto end;
    }

    
    if(!cJSON_IsNumber(rx_bytes))
    {
    goto end; //Numeric
    }

    // rd_wifi_metrics->tx_bytes
    cJSON *tx_bytes = cJSON_GetObjectItemCaseSensitive(rd_wifi_metricsJSON, "tx_bytes");
    if (cJSON_IsNull(tx_bytes)) {
        tx_bytes = NULL;
    }
    if (!tx_bytes) {
        goto end;
    }

    
    if(!cJSON_IsNumber(tx_bytes))
    {
    goto end; //Numeric
    }

    // rd_wifi_metrics->provision_packets
    cJSON *provision_packets = cJSON_GetObjectItemCaseSensitive(rd_wifi_metricsJSON, "provision_packets");
    if (cJSON_IsNull(provision_packets)) {
        provision_packets = NULL;
    }
    if (!provision_packets) {
        goto end;
    }

    
    if(!cJSON_IsNumber(provision_packets))
    {
    goto end; //Numeric
    }


    rd_wifi_metrics_local_var = rd_wifi_metrics_create_internal (
        accepted->valuedouble,
        disconnects->valuedouble,
        rejected->valuedouble,
        rx_bytes->valuedouble,
        tx_bytes->valuedouble,
        provision_packets->valuedouble
        );

    return rd_wifi_metrics_local_var;
end:
    return NULL;

}
