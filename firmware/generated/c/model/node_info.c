#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "node_info.h"



static node_info_t *node_info_create_internal(
    char *id,
    char *boot_id,
    char *firmware_version,
    char *board,
    char *api_version
    ) {
    node_info_t *node_info_local_var = malloc(sizeof(node_info_t));
    if (!node_info_local_var) {
        return NULL;
    }
    node_info_local_var->id = id;
    node_info_local_var->boot_id = boot_id;
    node_info_local_var->firmware_version = firmware_version;
    node_info_local_var->board = board;
    node_info_local_var->api_version = api_version;

    node_info_local_var->_library_owned = 1;
    return node_info_local_var;
}

__attribute__((deprecated)) node_info_t *node_info_create(
    char *id,
    char *boot_id,
    char *firmware_version,
    char *board,
    char *api_version
    ) {
    return node_info_create_internal (
        id,
        boot_id,
        firmware_version,
        board,
        api_version
        );
}

void node_info_free(node_info_t *node_info) {
    if(NULL == node_info){
        return ;
    }
    if(node_info->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "node_info_free");
        return ;
    }
    listEntry_t *listEntry;
    if (node_info->id) {
        free(node_info->id);
        node_info->id = NULL;
    }
    if (node_info->boot_id) {
        free(node_info->boot_id);
        node_info->boot_id = NULL;
    }
    if (node_info->firmware_version) {
        free(node_info->firmware_version);
        node_info->firmware_version = NULL;
    }
    if (node_info->board) {
        free(node_info->board);
        node_info->board = NULL;
    }
    if (node_info->api_version) {
        free(node_info->api_version);
        node_info->api_version = NULL;
    }
    free(node_info);
}

cJSON *node_info_convertToJSON(node_info_t *node_info) {
    cJSON *item = cJSON_CreateObject();

    // node_info->id
    if (!node_info->id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "id", node_info->id) == NULL) {
    goto fail; //String
    }


    // node_info->boot_id
    if (!node_info->boot_id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "boot_id", node_info->boot_id) == NULL) {
    goto fail; //String
    }


    // node_info->firmware_version
    if (!node_info->firmware_version) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "firmware_version", node_info->firmware_version) == NULL) {
    goto fail; //String
    }


    // node_info->board
    if (!node_info->board) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "board", node_info->board) == NULL) {
    goto fail; //String
    }


    // node_info->api_version
    if (!node_info->api_version) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "api_version", node_info->api_version) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

node_info_t *node_info_parseFromJSON(cJSON *node_infoJSON){

    node_info_t *node_info_local_var = NULL;

    // node_info->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(node_infoJSON, "id");
    if (cJSON_IsNull(id)) {
        id = NULL;
    }
    if (!id) {
        goto end;
    }

    
    if(!cJSON_IsString(id))
    {
    goto end; //String
    }

    // node_info->boot_id
    cJSON *boot_id = cJSON_GetObjectItemCaseSensitive(node_infoJSON, "boot_id");
    if (cJSON_IsNull(boot_id)) {
        boot_id = NULL;
    }
    if (!boot_id) {
        goto end;
    }

    
    if(!cJSON_IsString(boot_id))
    {
    goto end; //String
    }

    // node_info->firmware_version
    cJSON *firmware_version = cJSON_GetObjectItemCaseSensitive(node_infoJSON, "firmware_version");
    if (cJSON_IsNull(firmware_version)) {
        firmware_version = NULL;
    }
    if (!firmware_version) {
        goto end;
    }

    
    if(!cJSON_IsString(firmware_version))
    {
    goto end; //String
    }

    // node_info->board
    cJSON *board = cJSON_GetObjectItemCaseSensitive(node_infoJSON, "board");
    if (cJSON_IsNull(board)) {
        board = NULL;
    }
    if (!board) {
        goto end;
    }

    
    if(!cJSON_IsString(board))
    {
    goto end; //String
    }

    // node_info->api_version
    cJSON *api_version = cJSON_GetObjectItemCaseSensitive(node_infoJSON, "api_version");
    if (cJSON_IsNull(api_version)) {
        api_version = NULL;
    }
    if (!api_version) {
        goto end;
    }

    
    if(!cJSON_IsString(api_version))
    {
    goto end; //String
    }


    node_info_local_var = node_info_create_internal (
        strdup(id->valuestring),
        strdup(boot_id->valuestring),
        strdup(firmware_version->valuestring),
        strdup(board->valuestring),
        strdup(api_version->valuestring)
        );

    return node_info_local_var;
end:
    return NULL;

}
