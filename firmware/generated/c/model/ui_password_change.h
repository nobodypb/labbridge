/*
 * ui_password_change.h
 *
 * Replace the UI password and invalidate all browser sessions. Network requests require the current session, CSRF header and current_password; trusted USB may omit current_password for recovery. API bearer tokens are unchanged.
 */

#ifndef _ui_password_change_H_
#define _ui_password_change_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct ui_password_change_t ui_password_change_t;




typedef struct ui_password_change_t {
    char *password; // string
    char *confirm_password; // string
    char *current_password; // string

    int _library_owned; // Is the library responsible for freeing this object?
} ui_password_change_t;

__attribute__((deprecated)) ui_password_change_t *ui_password_change_create(
    char *password,
    char *confirm_password,
    char *current_password
);

void ui_password_change_free(ui_password_change_t *ui_password_change);

ui_password_change_t *ui_password_change_parseFromJSON(cJSON *ui_password_changeJSON);

cJSON *ui_password_change_convertToJSON(ui_password_change_t *ui_password_change);

#endif /* _ui_password_change_H_ */

