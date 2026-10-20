#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "yocto_watt_configuration.h"



static yocto_watt_configuration_t *yocto_watt_configuration_create_internal(
    char *hub_url
    ) {
    yocto_watt_configuration_t *yocto_watt_configuration_local_var = malloc(sizeof(yocto_watt_configuration_t));
    if (!yocto_watt_configuration_local_var) {
        return NULL;
    }
    yocto_watt_configuration_local_var->hub_url = hub_url;

    yocto_watt_configuration_local_var->_library_owned = 1;
    return yocto_watt_configuration_local_var;
}

__attribute__((deprecated)) yocto_watt_configuration_t *yocto_watt_configuration_create(
    char *hub_url
    ) {
    return yocto_watt_configuration_create_internal (
        hub_url
        );
}

void yocto_watt_configuration_free(yocto_watt_configuration_t *yocto_watt_configuration) {
    if(NULL == yocto_watt_configuration){
        return ;
    }
    if(yocto_watt_configuration->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "yocto_watt_configuration_free");
        return ;
    }
    listEntry_t *listEntry;
    if (yocto_watt_configuration->hub_url) {
        free(yocto_watt_configuration->hub_url);
        yocto_watt_configuration->hub_url = NULL;
    }
    free(yocto_watt_configuration);
}

cJSON *yocto_watt_configuration_convertToJSON(yocto_watt_configuration_t *yocto_watt_configuration) {
    cJSON *item = cJSON_CreateObject();

    // yocto_watt_configuration->hub_url
    if (!yocto_watt_configuration->hub_url) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "hub_url", yocto_watt_configuration->hub_url) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

yocto_watt_configuration_t *yocto_watt_configuration_parseFromJSON(cJSON *yocto_watt_configurationJSON){

    yocto_watt_configuration_t *yocto_watt_configuration_local_var = NULL;

    // yocto_watt_configuration->hub_url
    cJSON *hub_url = cJSON_GetObjectItemCaseSensitive(yocto_watt_configurationJSON, "hub_url");
    if (cJSON_IsNull(hub_url)) {
        hub_url = NULL;
    }
    if (!hub_url) {
        goto end;
    }

    
    if(!cJSON_IsString(hub_url))
    {
    goto end; //String
    }


    yocto_watt_configuration_local_var = yocto_watt_configuration_create_internal (
        strdup(hub_url->valuestring)
        );

    return yocto_watt_configuration_local_var;
end:
    return NULL;

}
