#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ui_password_setup.h"



static ui_password_setup_t *ui_password_setup_create_internal(
    char *password,
    char *confirm_password
    ) {
    ui_password_setup_t *ui_password_setup_local_var = malloc(sizeof(ui_password_setup_t));
    if (!ui_password_setup_local_var) {
        return NULL;
    }
    ui_password_setup_local_var->password = password;
    ui_password_setup_local_var->confirm_password = confirm_password;

    ui_password_setup_local_var->_library_owned = 1;
    return ui_password_setup_local_var;
}

__attribute__((deprecated)) ui_password_setup_t *ui_password_setup_create(
    char *password,
    char *confirm_password
    ) {
    return ui_password_setup_create_internal (
        password,
        confirm_password
        );
}

void ui_password_setup_free(ui_password_setup_t *ui_password_setup) {
    if(NULL == ui_password_setup){
        return ;
    }
    if(ui_password_setup->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "ui_password_setup_free");
        return ;
    }
    listEntry_t *listEntry;
    if (ui_password_setup->password) {
        free(ui_password_setup->password);
        ui_password_setup->password = NULL;
    }
    if (ui_password_setup->confirm_password) {
        free(ui_password_setup->confirm_password);
        ui_password_setup->confirm_password = NULL;
    }
    free(ui_password_setup);
}

cJSON *ui_password_setup_convertToJSON(ui_password_setup_t *ui_password_setup) {
    cJSON *item = cJSON_CreateObject();

    // ui_password_setup->password
    if (!ui_password_setup->password) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "password", ui_password_setup->password) == NULL) {
    goto fail; //String
    }


    // ui_password_setup->confirm_password
    if (!ui_password_setup->confirm_password) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "confirm_password", ui_password_setup->confirm_password) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

ui_password_setup_t *ui_password_setup_parseFromJSON(cJSON *ui_password_setupJSON){

    ui_password_setup_t *ui_password_setup_local_var = NULL;

    // ui_password_setup->password
    cJSON *password = cJSON_GetObjectItemCaseSensitive(ui_password_setupJSON, "password");
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

    // ui_password_setup->confirm_password
    cJSON *confirm_password = cJSON_GetObjectItemCaseSensitive(ui_password_setupJSON, "confirm_password");
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


    ui_password_setup_local_var = ui_password_setup_create_internal (
        strdup(password->valuestring),
        strdup(confirm_password->valuestring)
        );

    return ui_password_setup_local_var;
end:
    return NULL;

}
