#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ui_login.h"



static ui_login_t *ui_login_create_internal(
    char *password
    ) {
    ui_login_t *ui_login_local_var = malloc(sizeof(ui_login_t));
    if (!ui_login_local_var) {
        return NULL;
    }
    ui_login_local_var->password = password;

    ui_login_local_var->_library_owned = 1;
    return ui_login_local_var;
}

__attribute__((deprecated)) ui_login_t *ui_login_create(
    char *password
    ) {
    return ui_login_create_internal (
        password
        );
}

void ui_login_free(ui_login_t *ui_login) {
    if(NULL == ui_login){
        return ;
    }
    if(ui_login->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "ui_login_free");
        return ;
    }
    listEntry_t *listEntry;
    if (ui_login->password) {
        free(ui_login->password);
        ui_login->password = NULL;
    }
    free(ui_login);
}

cJSON *ui_login_convertToJSON(ui_login_t *ui_login) {
    cJSON *item = cJSON_CreateObject();

    // ui_login->password
    if (!ui_login->password) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "password", ui_login->password) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

ui_login_t *ui_login_parseFromJSON(cJSON *ui_loginJSON){

    ui_login_t *ui_login_local_var = NULL;

    // ui_login->password
    cJSON *password = cJSON_GetObjectItemCaseSensitive(ui_loginJSON, "password");
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


    ui_login_local_var = ui_login_create_internal (
        strdup(password->valuestring)
        );

    return ui_login_local_var;
end:
    return NULL;

}
