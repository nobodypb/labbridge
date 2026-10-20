#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dl24m_diagnostics.h"



static dl24m_diagnostics_t *dl24m_diagnostics_create_internal(
    list_t *entries,
    int dropped,
    list_t *registers
    ) {
    dl24m_diagnostics_t *dl24m_diagnostics_local_var = malloc(sizeof(dl24m_diagnostics_t));
    if (!dl24m_diagnostics_local_var) {
        return NULL;
    }
    dl24m_diagnostics_local_var->entries = entries;
    dl24m_diagnostics_local_var->dropped = dropped;
    dl24m_diagnostics_local_var->registers = registers;

    dl24m_diagnostics_local_var->_library_owned = 1;
    return dl24m_diagnostics_local_var;
}

__attribute__((deprecated)) dl24m_diagnostics_t *dl24m_diagnostics_create(
    list_t *entries,
    int dropped,
    list_t *registers
    ) {
    return dl24m_diagnostics_create_internal (
        entries,
        dropped,
        registers
        );
}

void dl24m_diagnostics_free(dl24m_diagnostics_t *dl24m_diagnostics) {
    if(NULL == dl24m_diagnostics){
        return ;
    }
    if(dl24m_diagnostics->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "dl24m_diagnostics_free");
        return ;
    }
    listEntry_t *listEntry;
    if (dl24m_diagnostics->entries) {
        list_ForEach(listEntry, dl24m_diagnostics->entries) {
            dl24m_trace_entry_free(listEntry->data);
        }
        list_freeList(dl24m_diagnostics->entries);
        dl24m_diagnostics->entries = NULL;
    }
    if (dl24m_diagnostics->registers) {
        list_ForEach(listEntry, dl24m_diagnostics->registers) {
            dl24m_register_free(listEntry->data);
        }
        list_freeList(dl24m_diagnostics->registers);
        dl24m_diagnostics->registers = NULL;
    }
    free(dl24m_diagnostics);
}

cJSON *dl24m_diagnostics_convertToJSON(dl24m_diagnostics_t *dl24m_diagnostics) {
    cJSON *item = cJSON_CreateObject();

    // dl24m_diagnostics->entries
    if (!dl24m_diagnostics->entries) {
        goto fail;
    }
    cJSON *entries = cJSON_AddArrayToObject(item, "entries");
    if(entries == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *entriesListEntry;
    if (dl24m_diagnostics->entries) {
    list_ForEach(entriesListEntry, dl24m_diagnostics->entries) {
    cJSON *itemLocal = dl24m_trace_entry_convertToJSON(entriesListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(entries, itemLocal);
    }
    }


    // dl24m_diagnostics->dropped
    if (!dl24m_diagnostics->dropped) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "dropped", dl24m_diagnostics->dropped) == NULL) {
    goto fail; //Numeric
    }


    // dl24m_diagnostics->registers
    if (!dl24m_diagnostics->registers) {
        goto fail;
    }
    cJSON *registers = cJSON_AddArrayToObject(item, "registers");
    if(registers == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *registersListEntry;
    if (dl24m_diagnostics->registers) {
    list_ForEach(registersListEntry, dl24m_diagnostics->registers) {
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

dl24m_diagnostics_t *dl24m_diagnostics_parseFromJSON(cJSON *dl24m_diagnosticsJSON){

    dl24m_diagnostics_t *dl24m_diagnostics_local_var = NULL;

    // define the local list for dl24m_diagnostics->entries
    list_t *entriesList = NULL;

    // define the local list for dl24m_diagnostics->registers
    list_t *registersList = NULL;

    // dl24m_diagnostics->entries
    cJSON *entries = cJSON_GetObjectItemCaseSensitive(dl24m_diagnosticsJSON, "entries");
    if (cJSON_IsNull(entries)) {
        entries = NULL;
    }
    if (!entries) {
        goto end;
    }

    
    cJSON *entries_local_nonprimitive = NULL;
    if(!cJSON_IsArray(entries)){
        goto end; //nonprimitive container
    }

    entriesList = list_createList();

    cJSON_ArrayForEach(entries_local_nonprimitive,entries )
    {
        if(!cJSON_IsObject(entries_local_nonprimitive)){
            goto end;
        }
        dl24m_trace_entry_t *entriesItem = dl24m_trace_entry_parseFromJSON(entries_local_nonprimitive);

        list_addElement(entriesList, entriesItem);
    }

    // dl24m_diagnostics->dropped
    cJSON *dropped = cJSON_GetObjectItemCaseSensitive(dl24m_diagnosticsJSON, "dropped");
    if (cJSON_IsNull(dropped)) {
        dropped = NULL;
    }
    if (!dropped) {
        goto end;
    }

    
    if(!cJSON_IsNumber(dropped))
    {
    goto end; //Numeric
    }

    // dl24m_diagnostics->registers
    cJSON *registers = cJSON_GetObjectItemCaseSensitive(dl24m_diagnosticsJSON, "registers");
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


    dl24m_diagnostics_local_var = dl24m_diagnostics_create_internal (
        entriesList,
        dropped->valuedouble,
        registersList
        );

    return dl24m_diagnostics_local_var;
end:
    if (entriesList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, entriesList) {
            dl24m_trace_entry_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(entriesList);
        entriesList = NULL;
    }
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
