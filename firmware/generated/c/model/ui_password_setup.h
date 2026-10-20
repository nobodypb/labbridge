/*
 * ui_password_setup.h
 *
 * Choose and confirm the first password over the setup AP or trusted USB. No default password.
 */

#ifndef _ui_password_setup_H_
#define _ui_password_setup_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct ui_password_setup_t ui_password_setup_t;




typedef struct ui_password_setup_t {
    char *password; // string
    char *confirm_password; // string

    int _library_owned; // Is the library responsible for freeing this object?
} ui_password_setup_t;

__attribute__((deprecated)) ui_password_setup_t *ui_password_setup_create(
    char *password,
    char *confirm_password
);

void ui_password_setup_free(ui_password_setup_t *ui_password_setup);

ui_password_setup_t *ui_password_setup_parseFromJSON(cJSON *ui_password_setupJSON);

cJSON *ui_password_setup_convertToJSON(ui_password_setup_t *ui_password_setup);

#endif /* _ui_password_setup_H_ */

