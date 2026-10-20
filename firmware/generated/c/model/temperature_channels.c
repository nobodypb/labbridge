#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "temperature_channels.h"



static temperature_channels_t *temperature_channels_create_internal(
    list_t *channels
    ) {
    temperature_channels_t *temperature_channels_local_var = malloc(sizeof(temperature_channels_t));
    if (!temperature_channels_local_var) {
        return NULL;
    }
    temperature_channels_local_var->channels = channels;

    temperature_channels_local_var->_library_owned = 1;
    return temperature_channels_local_var;
}

__attribute__((deprecated)) temperature_channels_t *temperature_channels_create(
    list_t *channels
    ) {
    return temperature_channels_create_internal (
        channels
        );
}

void temperature_channels_free(temperature_channels_t *temperature_channels) {
    if(NULL == temperature_channels){
        return ;
    }
    if(temperature_channels->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "temperature_channels_free");
        return ;
    }
    listEntry_t *listEntry;
    if (temperature_channels->channels) {
        list_ForEach(listEntry, temperature_channels->channels) {
            temperature_channel_free(listEntry->data);
        }
        list_freeList(temperature_channels->channels);
        temperature_channels->channels = NULL;
    }
    free(temperature_channels);
}

cJSON *temperature_channels_convertToJSON(temperature_channels_t *temperature_channels) {
    cJSON *item = cJSON_CreateObject();

    // temperature_channels->channels
    if (!temperature_channels->channels) {
        goto fail;
    }
    cJSON *channels = cJSON_AddArrayToObject(item, "channels");
    if(channels == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *channelsListEntry;
    if (temperature_channels->channels) {
    list_ForEach(channelsListEntry, temperature_channels->channels) {
    cJSON *itemLocal = temperature_channel_convertToJSON(channelsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(channels, itemLocal);
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

temperature_channels_t *temperature_channels_parseFromJSON(cJSON *temperature_channelsJSON){

    temperature_channels_t *temperature_channels_local_var = NULL;

    // define the local list for temperature_channels->channels
    list_t *channelsList = NULL;

    // temperature_channels->channels
    cJSON *channels = cJSON_GetObjectItemCaseSensitive(temperature_channelsJSON, "channels");
    if (cJSON_IsNull(channels)) {
        channels = NULL;
    }
    if (!channels) {
        goto end;
    }

    
    cJSON *channels_local_nonprimitive = NULL;
    if(!cJSON_IsArray(channels)){
        goto end; //nonprimitive container
    }

    channelsList = list_createList();

    cJSON_ArrayForEach(channels_local_nonprimitive,channels )
    {
        if(!cJSON_IsObject(channels_local_nonprimitive)){
            goto end;
        }
        temperature_channel_t *channelsItem = temperature_channel_parseFromJSON(channels_local_nonprimitive);

        list_addElement(channelsList, channelsItem);
    }


    temperature_channels_local_var = temperature_channels_create_internal (
        channelsList
        );

    return temperature_channels_local_var;
end:
    if (channelsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, channelsList) {
            temperature_channel_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(channelsList);
        channelsList = NULL;
    }
    return NULL;

}
