#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "component.h"


char* component_kind_ToString(labbridge_api_component_KIND_e kind) {
    char* kindArray[] =  { "NULL", "source", "sink", "sensor" };
    return kindArray[kind];
}

labbridge_api_component_KIND_e component_kind_FromString(char* kind){
    int stringToReturn = 0;
    char *kindArray[] =  { "NULL", "source", "sink", "sensor" };
    size_t sizeofArray = sizeof(kindArray) / sizeof(kindArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(kind, kindArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}
char* component_temperature_kind_ToString(labbridge_api_component_TEMPERATUREKIND_e temperature_kind) {
    char* temperature_kindArray[] =  { "NULL", "internal", "external_probe", "cell" };
    return temperature_kindArray[temperature_kind];
}

labbridge_api_component_TEMPERATUREKIND_e component_temperature_kind_FromString(char* temperature_kind){
    int stringToReturn = 0;
    char *temperature_kindArray[] =  { "NULL", "internal", "external_probe", "cell" };
    size_t sizeofArray = sizeof(temperature_kindArray) / sizeof(temperature_kindArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(temperature_kind, temperature_kindArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static component_t *component_create_internal(
    char *id,
    labbridge_api_component_KIND_e kind,
    list_t *actions,
    char *quantity,
    char *unit,
    labbridge_api_component_TEMPERATUREKIND_e temperature_kind
    ) {
    component_t *component_local_var = malloc(sizeof(component_t));
    if (!component_local_var) {
        return NULL;
    }
    component_local_var->id = id;
    component_local_var->kind = kind;
    component_local_var->actions = actions;
    component_local_var->quantity = quantity;
    component_local_var->unit = unit;
    component_local_var->temperature_kind = temperature_kind;

    component_local_var->_library_owned = 1;
    return component_local_var;
}

__attribute__((deprecated)) component_t *component_create(
    char *id,
    labbridge_api_component_KIND_e kind,
    list_t *actions,
    char *quantity,
    char *unit,
    labbridge_api_component_TEMPERATUREKIND_e temperature_kind
    ) {
    return component_create_internal (
        id,
        kind,
        actions,
        quantity,
        unit,
        temperature_kind
        );
}

void component_free(component_t *component) {
    if(NULL == component){
        return ;
    }
    if(component->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "component_free");
        return ;
    }
    listEntry_t *listEntry;
    if (component->id) {
        free(component->id);
        component->id = NULL;
    }
    if (component->actions) {
        list_ForEach(listEntry, component->actions) {
            free(listEntry->data);
        }
        list_freeList(component->actions);
        component->actions = NULL;
    }
    if (component->quantity) {
        free(component->quantity);
        component->quantity = NULL;
    }
    if (component->unit) {
        free(component->unit);
        component->unit = NULL;
    }
    free(component);
}

cJSON *component_convertToJSON(component_t *component) {
    cJSON *item = cJSON_CreateObject();

    // component->id
    if (!component->id) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "id", component->id) == NULL) {
    goto fail; //String
    }


    // component->kind
    if (labbridge_api_component_KIND_NULL == component->kind) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "kind", component_kind_ToString(component->kind)) == NULL)
    {
    goto fail; //Enum
    }


    // component->actions
    if (!component->actions) {
        goto fail;
    }
    cJSON *actions = cJSON_AddArrayToObject(item, "actions");
    if(actions == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *actionsListEntry;
    list_ForEach(actionsListEntry, component->actions) {
    if(cJSON_AddStringToObject(actions, "", actionsListEntry->data) == NULL)
    {
        goto fail;
    }
    }


    // component->quantity
    if(component->quantity) {
    if(cJSON_AddStringToObject(item, "quantity", component->quantity) == NULL) {
    goto fail; //String
    }
    }


    // component->unit
    if(component->unit) {
    if(cJSON_AddStringToObject(item, "unit", component->unit) == NULL) {
    goto fail; //String
    }
    }


    // component->temperature_kind
    if(component->temperature_kind != labbridge_api_component_TEMPERATUREKIND_NULL) {
    if(cJSON_AddStringToObject(item, "temperature_kind", component_temperature_kind_ToString(component->temperature_kind)) == NULL)
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

component_t *component_parseFromJSON(cJSON *componentJSON){

    component_t *component_local_var = NULL;

    // define the local list for component->actions
    list_t *actionsList = NULL;

    // component->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(componentJSON, "id");
    if (cJSON_IsNull(id)) {
        id = NULL;
    }
    if (!id) {
        goto end;
    }

    
    if(!cJSON_IsString(id))
    {
    goto end; //String
    }

    // component->kind
    cJSON *kind = cJSON_GetObjectItemCaseSensitive(componentJSON, "kind");
    if (cJSON_IsNull(kind)) {
        kind = NULL;
    }
    if (!kind) {
        goto end;
    }

    labbridge_api_component_KIND_e kindVariable;
    
    if(!cJSON_IsString(kind))
    {
    goto end; //Enum
    }
    kindVariable = component_kind_FromString(kind->valuestring);

    // component->actions
    cJSON *actions = cJSON_GetObjectItemCaseSensitive(componentJSON, "actions");
    if (cJSON_IsNull(actions)) {
        actions = NULL;
    }
    if (!actions) {
        goto end;
    }

    
    cJSON *actions_local = NULL;
    if(!cJSON_IsArray(actions)) {
        goto end;//primitive container
    }
    actionsList = list_createList();

    cJSON_ArrayForEach(actions_local, actions)
    {
        if(!cJSON_IsString(actions_local))
        {
            goto end;
        }
        list_addElement(actionsList , strdup(actions_local->valuestring));
    }

    // component->quantity
    cJSON *quantity = cJSON_GetObjectItemCaseSensitive(componentJSON, "quantity");
    if (cJSON_IsNull(quantity)) {
        quantity = NULL;
    }
    if (quantity) { 
    if(!cJSON_IsString(quantity) && !cJSON_IsNull(quantity))
    {
    goto end; //String
    }
    }

    // component->unit
    cJSON *unit = cJSON_GetObjectItemCaseSensitive(componentJSON, "unit");
    if (cJSON_IsNull(unit)) {
        unit = NULL;
    }
    if (unit) { 
    if(!cJSON_IsString(unit) && !cJSON_IsNull(unit))
    {
    goto end; //String
    }
    }

    // component->temperature_kind
    cJSON *temperature_kind = cJSON_GetObjectItemCaseSensitive(componentJSON, "temperature_kind");
    if (cJSON_IsNull(temperature_kind)) {
        temperature_kind = NULL;
    }
    labbridge_api_component_TEMPERATUREKIND_e temperature_kindVariable;
    if (temperature_kind) { 
    if(!cJSON_IsString(temperature_kind))
    {
    goto end; //Enum
    }
    temperature_kindVariable = component_temperature_kind_FromString(temperature_kind->valuestring);
    }


    component_local_var = component_create_internal (
        strdup(id->valuestring),
        kindVariable,
        actionsList,
        quantity && !cJSON_IsNull(quantity) ? strdup(quantity->valuestring) : NULL,
        unit && !cJSON_IsNull(unit) ? strdup(unit->valuestring) : NULL,
        temperature_kind ? temperature_kindVariable : labbridge_api_component_TEMPERATUREKIND_NULL
        );

    return component_local_var;
end:
    if (actionsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, actionsList) {
            free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(actionsList);
        actionsList = NULL;
    }
    return NULL;

}
