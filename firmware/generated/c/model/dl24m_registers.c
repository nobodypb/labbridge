#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dl24m_registers.h"



static dl24m_registers_t *dl24m_registers_create_internal(
    list_t *registers
    ) {
    dl24m_registers_t *dl24m_registers_local_var = malloc(sizeof(dl24m_registers_t));
    if (!dl24m_registers_local_var) {
        return NULL;
    }
    dl24m_registers_local_var->registers = registers;

    dl24m_registers_local_var->_library_owned = 1;
    return dl24m_registers_local_var;
}

__attribute__((deprecated)) dl24m_registers_t *dl24m_registers_create(
    list_t *registers
    ) {
    return dl24m_registers_create_internal (
        registers
        );
}

void dl24m_registers_free(dl24m_registers_t *dl24m_registers) {
    if(NULL == dl24m_registers){
        return ;
    }
    if(dl24m_registers->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "dl24m_registers_free");
        return ;
    }
    listEntry_t *listEntry;
    if (dl24m_registers->registers) {
        list_ForEach(listEntry, dl24m_registers->registers) {
            dl24m_register_free(listEntry->data);
        }
        list_freeList(dl24m_registers->registers);
        dl24m_registers->registers = NULL;
    }
    free(dl24m_registers);
}

cJSON *dl24m_registers_convertToJSON(dl24m_registers_t *dl24m_registers) {
    cJSON *item = cJSON_CreateObject();

    // dl24m_registers->registers
    if (!dl24m_registers->registers) {
        goto fail;
    }
    cJSON *registers = cJSON_AddArrayToObject(item, "registers");
    if(registers == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *registersListEntry;
    if (dl24m_registers->registers) {
    list_ForEach(registersListEntry, dl24m_registers->registers) {
    cJSON *itemLocal = dl24m_register_convertToJSON(registersListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(registers, itemLocal);
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

dl24m_registers_t *dl24m_registers_parseFromJSON(cJSON *dl24m_registersJSON){

    dl24m_registers_t *dl24m_registers_local_var = NULL;

    // define the local list for dl24m_registers->registers
    list_t *registersList = NULL;

    // dl24m_registers->registers
    cJSON *registers = cJSON_GetObjectItemCaseSensitive(dl24m_registersJSON, "registers");
    if (cJSON_IsNull(registers)) {
        registers = NULL;
    }
    if (!registers) {
        goto end;
    }

    
    cJSON *registers_local_nonprimitive = NULL;
    if(!cJSON_IsArray(registers)){
        goto end; //nonprimitive container
    }

    registersList = list_createList();

    cJSON_ArrayForEach(registers_local_nonprimitive,registers )
    {
        if(!cJSON_IsObject(registers_local_nonprimitive)){
            goto end;
        }
        dl24m_register_t *registersItem = dl24m_register_parseFromJSON(registers_local_nonprimitive);

        list_addElement(registersList, registersItem);
    }


    dl24m_registers_local_var = dl24m_registers_create_internal (
        registersList
        );

    return dl24m_registers_local_var;
end:
    if (registersList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, registersList) {
            dl24m_register_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(registersList);
        registersList = NULL;
    }
    return NULL;

}
