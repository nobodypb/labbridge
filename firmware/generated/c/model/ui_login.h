/*
 * ui_login.h
 *
 * Password login on the current node. JSON and same-origin browser requests only.
 */

#ifndef _ui_login_H_
#define _ui_login_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct ui_login_t ui_login_t;




typedef struct ui_login_t {
    char *password; // string

    int _library_owned; // Is the library responsible for freeing this object?
} ui_login_t;

__attribute__((deprecated)) ui_login_t *ui_login_create(
    char *password
);

void ui_login_free(ui_login_t *ui_login);

ui_login_t *ui_login_parseFromJSON(cJSON *ui_loginJSON);

cJSON *ui_login_convertToJSON(ui_login_t *ui_login);

#endif /* _ui_login_H_ */

