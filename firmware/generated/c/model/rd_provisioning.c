#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rd_provisioning.h"


char* rd_provisioning_state_ToString(labbridge_api_rd_provisioning_STATE_e state) {
    char* stateArray[] =  { "NULL", "idle", "server_address", "wifi", "waiting_connection", "complete", "cancelled", "timed_out", "failed" };
    return stateArray[state];
}

labbridge_api_rd_provisioning_STATE_e rd_provisioning_state_FromString(char* state){
    int stringToReturn = 0;
    char *stateArray[] =  { "NULL", "idle", "server_address", "wifi", "waiting_connection", "complete", "cancelled", "timed_out", "failed" };
    size_t sizeofArray = sizeof(stateArray) / sizeof(stateArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(state, stateArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static rd_provisioning_t *rd_provisioning_create_internal(
    labbridge_api_rd_provisioning_STATE_e state,
    char *session_id,
    long deadline_ms,
    char *ap_ssid,
    char *ap_address,
    long tcp_port,
    int listener_running,
    list_t *candidates,
    rd_wifi_metrics_t *metrics,
    char *error,
    long ap_clients,
    long ap_channel
    ) {
    rd_provisioning_t *rd_provisioning_local_var = malloc(sizeof(rd_provisioning_t));
    if (!rd_provisioning_local_var) {
        return NULL;
    }
    rd_provisioning_local_var->state = state;
    rd_provisioning_local_var->session_id = session_id;
    rd_provisioning_local_var->deadline_ms = deadline_ms;
    rd_provisioning_local_var->ap_ssid = ap_ssid;
    rd_provisioning_local_var->ap_address = ap_address;
    rd_provisioning_local_var->tcp_port = tcp_port;
    rd_provisioning_local_var->listener_running = listener_running;
    rd_provisioning_local_var->candidates = candidates;
    rd_provisioning_local_var->metrics = metrics;
    rd_provisioning_local_var->error = error;
    rd_provisioning_local_var->ap_clients = ap_clients;
    rd_provisioning_local_var->ap_channel = ap_channel;

    rd_provisioning_local_var->_library_owned = 1;
    return rd_provisioning_local_var;
}

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
    char *error,
    long ap_clients,
    long ap_channel
    ) {
    return rd_provisioning_create_internal (
        state,
        session_id,
        deadline_ms,
        ap_ssid,
        ap_address,
        tcp_port,
        listener_running,
        candidates,
        metrics,
        error,
        ap_clients,
        ap_channel
        );
}

void rd_provisioning_free(rd_provisioning_t *rd_provisioning) {
    if(NULL == rd_provisioning){
        return ;
    }
    if(rd_provisioning->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "rd_provisioning_free");
        return ;
    }
    listEntry_t *listEntry;
    if (rd_provisioning->session_id) {
        free(rd_provisioning->session_id);
        rd_provisioning->session_id = NULL;
    }
    if (rd_provisioning->ap_ssid) {
        free(rd_provisioning->ap_ssid);
        rd_provisioning->ap_ssid = NULL;
    }
    if (rd_provisioning->ap_address) {
        free(rd_provisioning->ap_address);
        rd_provisioning->ap_address = NULL;
    }
    if (rd_provisioning->candidates) {
        list_ForEach(listEntry, rd_provisioning->candidates) {
            rd_wifi_candidate_free(listEntry->data);
        }
        list_freeList(rd_provisioning->candidates);
        rd_provisioning->candidates = NULL;
    }
    if (rd_provisioning->metrics) {
        rd_wifi_metrics_free(rd_provisioning->metrics);
        rd_provisioning->metrics = NULL;
    }
    if (rd_provisioning->error) {
        free(rd_provisioning->error);
        rd_provisioning->error = NULL;
    }
    free(rd_provisioning);
}

cJSON *rd_provisioning_convertToJSON(rd_provisioning_t *rd_provisioning) {
    cJSON *item = cJSON_CreateObject();

    // rd_provisioning->state
    if (labbridge_api_rd_provisioning_STATE_NULL == rd_provisioning->state) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "state", rd_provisioning_state_ToString(rd_provisioning->state)) == NULL)
    {
    goto fail; //Enum
    }


    // rd_provisioning->session_id
    if(rd_provisioning->session_id) {
    if(cJSON_AddStringToObject(item, "session_id", rd_provisioning->session_id) == NULL) {
    goto fail; //String
    }
    }


    // rd_provisioning->deadline_ms
    if(rd_provisioning->deadline_ms) {
    if(cJSON_AddNumberToObject(item, "deadline_ms", rd_provisioning->deadline_ms) == NULL) {
    goto fail; //Numeric
    }
    }


    // rd_provisioning->ap_ssid
    if (!rd_provisioning->ap_ssid) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "ap_ssid", rd_provisioning->ap_ssid) == NULL) {
    goto fail; //String
    }


    // rd_provisioning->ap_address
    if (!rd_provisioning->ap_address) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "ap_address", rd_provisioning->ap_address) == NULL) {
    goto fail; //String
    }


    // rd_provisioning->tcp_port
    if (!rd_provisioning->tcp_port) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "tcp_port", rd_provisioning->tcp_port) == NULL) {
    goto fail; //Numeric
    }


    // rd_provisioning->listener_running
    if (!rd_provisioning->listener_running) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "listener_running", rd_provisioning->listener_running) == NULL) {
    goto fail; //Bool
    }


    // rd_provisioning->candidates
    if (!rd_provisioning->candidates) {
        goto fail;
    }
    cJSON *candidates = cJSON_AddArrayToObject(item, "candidates");
    if(candidates == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *candidatesListEntry;
    if (rd_provisioning->candidates) {
    list_ForEach(candidatesListEntry, rd_provisioning->candidates) {
    cJSON *itemLocal = rd_wifi_candidate_convertToJSON(candidatesListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(candidates, itemLocal);
    }
    }


    // rd_provisioning->metrics
    if (!rd_provisioning->metrics) {
        goto fail;
    }
    cJSON *metrics_local_JSON = rd_wifi_metrics_convertToJSON(rd_provisioning->metrics);
    if(metrics_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "metrics", metrics_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }


    // rd_provisioning->error
    if(rd_provisioning->error) {
    if(cJSON_AddStringToObject(item, "error", rd_provisioning->error) == NULL) {
    goto fail; //String
    }
    }


    // rd_provisioning->ap_clients
    if(rd_provisioning->ap_clients) {
    if(cJSON_AddNumberToObject(item, "ap_clients", rd_provisioning->ap_clients) == NULL) {
    goto fail; //Numeric
    }
    }


    // rd_provisioning->ap_channel
    if(rd_provisioning->ap_channel) {
    if(cJSON_AddNumberToObject(item, "ap_channel", rd_provisioning->ap_channel) == NULL) {
    goto fail; //Numeric
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

rd_provisioning_t *rd_provisioning_parseFromJSON(cJSON *rd_provisioningJSON){

    rd_provisioning_t *rd_provisioning_local_var = NULL;

    // define the local list for rd_provisioning->candidates
    list_t *candidatesList = NULL;

    // define the local variable for rd_provisioning->metrics
    rd_wifi_metrics_t *metrics_local_nonprim = NULL;

    // rd_provisioning->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    if (!state) {
        goto end;
    }

    labbridge_api_rd_provisioning_STATE_e stateVariable;
    
    if(!cJSON_IsString(state))
    {
    goto end; //Enum
    }
    stateVariable = rd_provisioning_state_FromString(state->valuestring);

    // rd_provisioning->session_id
    cJSON *session_id = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "session_id");
    if (cJSON_IsNull(session_id)) {
        session_id = NULL;
    }
    if (session_id) { 
    if(!cJSON_IsString(session_id) && !cJSON_IsNull(session_id))
    {
    goto end; //String
    }
    }

    // rd_provisioning->deadline_ms
    cJSON *deadline_ms = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "deadline_ms");
    if (cJSON_IsNull(deadline_ms)) {
        deadline_ms = NULL;
    }
    if (deadline_ms) { 
    if(!cJSON_IsNumber(deadline_ms))
    {
    goto end; //Numeric
    }
    }

    // rd_provisioning->ap_ssid
    cJSON *ap_ssid = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "ap_ssid");
    if (cJSON_IsNull(ap_ssid)) {
        ap_ssid = NULL;
    }
    if (!ap_ssid) {
        goto end;
    }

    
    if(!cJSON_IsString(ap_ssid))
    {
    goto end; //String
    }

    // rd_provisioning->ap_address
    cJSON *ap_address = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "ap_address");
    if (cJSON_IsNull(ap_address)) {
        ap_address = NULL;
    }
    if (!ap_address) {
        goto end;
    }

    
    if(!cJSON_IsString(ap_address))
    {
    goto end; //String
    }

    // rd_provisioning->tcp_port
    cJSON *tcp_port = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "tcp_port");
    if (cJSON_IsNull(tcp_port)) {
        tcp_port = NULL;
    }
    if (!tcp_port) {
        goto end;
    }

    
    if(!cJSON_IsNumber(tcp_port))
    {
    goto end; //Numeric
    }

    // rd_provisioning->listener_running
    cJSON *listener_running = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "listener_running");
    if (cJSON_IsNull(listener_running)) {
        listener_running = NULL;
    }
    if (!listener_running) {
        goto end;
    }

    
    if(!cJSON_IsBool(listener_running))
    {
    goto end; //Bool
    }

    // rd_provisioning->candidates
    cJSON *candidates = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "candidates");
    if (cJSON_IsNull(candidates)) {
        candidates = NULL;
    }
    if (!candidates) {
        goto end;
    }

    
    cJSON *candidates_local_nonprimitive = NULL;
    if(!cJSON_IsArray(candidates)){
        goto end; //nonprimitive container
    }

    candidatesList = list_createList();

    cJSON_ArrayForEach(candidates_local_nonprimitive,candidates )
    {
        if(!cJSON_IsObject(candidates_local_nonprimitive)){
            goto end;
        }
        rd_wifi_candidate_t *candidatesItem = rd_wifi_candidate_parseFromJSON(candidates_local_nonprimitive);

        list_addElement(candidatesList, candidatesItem);
    }

    // rd_provisioning->metrics
    cJSON *metrics = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "metrics");
    if (cJSON_IsNull(metrics)) {
        metrics = NULL;
    }
    if (!metrics) {
        goto end;
    }

    
    metrics_local_nonprim = rd_wifi_metrics_parseFromJSON(metrics); //nonprimitive

    // rd_provisioning->error
    cJSON *error = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "error");
    if (cJSON_IsNull(error)) {
        error = NULL;
    }
    if (error) { 
    if(!cJSON_IsString(error) && !cJSON_IsNull(error))
    {
    goto end; //String
    }
    }

    // rd_provisioning->ap_clients
    cJSON *ap_clients = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "ap_clients");
    if (cJSON_IsNull(ap_clients)) {
        ap_clients = NULL;
    }
    if (ap_clients) { 
    if(!cJSON_IsNumber(ap_clients))
    {
    goto end; //Numeric
    }
    }

    // rd_provisioning->ap_channel
    cJSON *ap_channel = cJSON_GetObjectItemCaseSensitive(rd_provisioningJSON, "ap_channel");
    if (cJSON_IsNull(ap_channel)) {
        ap_channel = NULL;
    }
    if (ap_channel) { 
    if(!cJSON_IsNumber(ap_channel))
    {
    goto end; //Numeric
    }
    }


    rd_provisioning_local_var = rd_provisioning_create_internal (
        stateVariable,
        session_id && !cJSON_IsNull(session_id) ? strdup(session_id->valuestring) : NULL,
        deadline_ms ? deadline_ms->valuedouble : 0,
        strdup(ap_ssid->valuestring),
        strdup(ap_address->valuestring),
        tcp_port->valuedouble,
        listener_running->valueint,
        candidatesList,
        metrics_local_nonprim,
        error && !cJSON_IsNull(error) ? strdup(error->valuestring) : NULL,
        ap_clients ? ap_clients->valuedouble : 0,
        ap_channel ? ap_channel->valuedouble : 0
        );

    return rd_provisioning_local_var;
end:
    if (candidatesList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, candidatesList) {
            rd_wifi_candidate_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(candidatesList);
        candidatesList = NULL;
    }
    if (metrics_local_nonprim) {
        rd_wifi_metrics_free(metrics_local_nonprim);
        metrics_local_nonprim = NULL;
    }
    return NULL;

}
