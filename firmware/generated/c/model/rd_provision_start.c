#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rd_provision_start.h"


char* rd_provision_start_profile_ToString(labbridge_api_rd_provision_start_PROFILE_e profile) {
    char* profileArray[] =  { "NULL", "pc_v22", "pc_v22_target_ssid" };
    return profileArray[profile];
}

labbridge_api_rd_provision_start_PROFILE_e rd_provision_start_profile_FromString(char* profile){
    int stringToReturn = 0;
    char *profileArray[] =  { "NULL", "pc_v22", "pc_v22_target_ssid" };
    size_t sizeofArray = sizeof(profileArray) / sizeof(profileArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(profile, profileArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static rd_provision_start_t *rd_provision_start_create_internal(
    int confirm_outputs_off,
    labbridge_api_rd_provision_start_PROFILE_e profile
    ) {
    rd_provision_start_t *rd_provision_start_local_var = malloc(sizeof(rd_provision_start_t));
    if (!rd_provision_start_local_var) {
        return NULL;
    }
    rd_provision_start_local_var->confirm_outputs_off = confirm_outputs_off;
    rd_provision_start_local_var->profile = profile;

    rd_provision_start_local_var->_library_owned = 1;
    return rd_provision_start_local_var;
}

__attribute__((deprecated)) rd_provision_start_t *rd_provision_start_create(
    int confirm_outputs_off,
    labbridge_api_rd_provision_start_PROFILE_e profile
    ) {
    return rd_provision_start_create_internal (
        confirm_outputs_off,
        profile
        );
}

void rd_provision_start_free(rd_provision_start_t *rd_provision_start) {
    if(NULL == rd_provision_start){
        return ;
    }
    if(rd_provision_start->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "rd_provision_start_free");
        return ;
    }
    listEntry_t *listEntry;
    free(rd_provision_start);
}

cJSON *rd_provision_start_convertToJSON(rd_provision_start_t *rd_provision_start) {
    cJSON *item = cJSON_CreateObject();

    // rd_provision_start->confirm_outputs_off
    if (!rd_provision_start->confirm_outputs_off) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "confirm_outputs_off", rd_provision_start->confirm_outputs_off) == NULL) {
    goto fail; //Bool
    }


    // rd_provision_start->profile
    if(rd_provision_start->profile != labbridge_api_rd_provision_start_PROFILE_NULL) {
    if(cJSON_AddStringToObject(item, "profile", rd_provision_start_profile_ToString(rd_provision_start->profile)) == NULL)
    {
    goto fail; //Enum
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

rd_provision_start_t *rd_provision_start_parseFromJSON(cJSON *rd_provision_startJSON){

    rd_provision_start_t *rd_provision_start_local_var = NULL;

    // rd_provision_start->confirm_outputs_off
    cJSON *confirm_outputs_off = cJSON_GetObjectItemCaseSensitive(rd_provision_startJSON, "confirm_outputs_off");
    if (cJSON_IsNull(confirm_outputs_off)) {
        confirm_outputs_off = NULL;
    }
    if (!confirm_outputs_off) {
        goto end;
    }

    
    if(!cJSON_IsBool(confirm_outputs_off))
    {
    goto end; //Bool
    }

    // rd_provision_start->profile
    cJSON *profile = cJSON_GetObjectItemCaseSensitive(rd_provision_startJSON, "profile");
    if (cJSON_IsNull(profile)) {
        profile = NULL;
    }
    labbridge_api_rd_provision_start_PROFILE_e profileVariable;
    if (profile) { 
    if(!cJSON_IsString(profile))
    {
    goto end; //Enum
    }
    profileVariable = rd_provision_start_profile_FromString(profile->valuestring);
    }


    rd_provision_start_local_var = rd_provision_start_create_internal (
        confirm_outputs_off->valueint,
        profile ? profileVariable : labbridge_api_rd_provision_start_PROFILE_NULL
        );

    return rd_provision_start_local_var;
end:
    return NULL;

}
