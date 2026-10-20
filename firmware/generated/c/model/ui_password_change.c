#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ui_password_change.h"



static ui_password_change_t *ui_password_change_create_internal(
    char *password,
    char *confirm_password,
    char *current_password
    ) {
    ui_password_change_t *ui_password_change_local_var = malloc(sizeof(ui_password_change_t));
    if (!ui_password_change_local_var) {
        return NULL;
    }
    ui_password_change_local_var->password = password;
    ui_password_change_local_var->confirm_password = confirm_password;
    ui_password_change_local_var->current_password = current_password;

    ui_password_change_local_var->_library_owned = 1;
    return ui_password_change_local_var;
}

__attribute__((deprecated)) ui_password_change_t *ui_password_change_create(
    char *password,
    char *confirm_password,
    char *current_password
    ) {
    return ui_password_change_create_internal (
        password,
        confirm_password,
        current_password
        );
}

void ui_password_change_free(ui_password_change_t *ui_password_change) {
    if(NULL == ui_password_change){
        return ;
    }
    if(ui_password_change->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "ui_password_change_free");
        return ;
    }
    listEntry_t *listEntry;
    if (ui_password_change->password) {
        free(ui_password_change->password);
        ui_password_change->password = NULL;
    }
    if (ui_password_change->confirm_password) {
        free(ui_password_change->confirm_password);
        ui_password_change->confirm_password = NULL;
    }
    if (ui_password_change->current_password) {
        free(ui_password_change->current_password);
        ui_password_change->current_password = NULL;
    }
    free(ui_password_change);
}

cJSON *ui_password_change_convertToJSON(ui_password_change_t *ui_password_change) {
    cJSON *item = cJSON_CreateObject();

    // ui_password_change->password
    if (!ui_password_change->password) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "password", ui_password_change->password) == NULL) {
    goto fail; //String
    }


    // ui_password_change->confirm_password
    if (!ui_password_change->confirm_password) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "confirm_password", ui_password_change->confirm_password) == NULL) {
    goto fail; //String
    }


    // ui_password_change->current_password
    if(ui_password_change->current_password) {
    if(cJSON_AddStringToObject(item, "current_password", ui_password_change->current_password) == NULL) {
    goto fail; //String
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

ui_password_change_t *ui_password_change_parseFromJSON(cJSON *ui_password_changeJSON){

    ui_password_change_t *ui_password_change_local_var = NULL;

    // ui_password_change->password
    cJSON *password = cJSON_GetObjectItemCaseSensitive(ui_password_changeJSON, "password");
    if (cJSON_IsNull(password)) {
        password = NULL;
    }
    if (!password) {
        goto end;
    }

    
    if(!cJSON_IsString(password))
    {
    goto end; //String
    }

    // ui_password_change->confirm_password
    cJSON *confirm_password = cJSON_GetObjectItemCaseSensitive(ui_password_changeJSON, "confirm_password");
    if (cJSON_IsNull(confirm_password)) {
        confirm_password = NULL;
    }
    if (!confirm_password) {
        goto end;
    }

    
    if(!cJSON_IsString(confirm_password))
    {
    goto end; //String
    }

    // ui_password_change->current_password
    cJSON *current_password = cJSON_GetObjectItemCaseSensitive(ui_password_changeJSON, "current_password");
    if (cJSON_IsNull(current_password)) {
        current_password = NULL;
    }
    if (current_password) { 
    if(!cJSON_IsString(current_password) && !cJSON_IsNull(current_password))
    {
    goto end; //String
    }
    }


    ui_password_change_local_var = ui_password_change_create_internal (
        strdup(password->valuestring),
        strdup(confirm_password->valuestring),
        current_password && !cJSON_IsNull(current_password) ? strdup(current_password->valuestring) : NULL
        );

    return ui_password_change_local_var;
end:
    return NULL;

}
