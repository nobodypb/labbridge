#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "component_list.h"



static component_list_t *component_list_create_internal(
    list_t *components
    ) {
    component_list_t *component_list_local_var = malloc(sizeof(component_list_t));
    if (!component_list_local_var) {
        return NULL;
    }
    component_list_local_var->components = components;

    component_list_local_var->_library_owned = 1;
    return component_list_local_var;
}

__attribute__((deprecated)) component_list_t *component_list_create(
    list_t *components
    ) {
    return component_list_create_internal (
        components
        );
}

void component_list_free(component_list_t *component_list) {
    if(NULL == component_list){
        return ;
    }
    if(component_list->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "component_list_free");
        return ;
    }
    listEntry_t *listEntry;
    if (component_list->components) {
        list_ForEach(listEntry, component_list->components) {
            component_free(listEntry->data);
        }
        list_freeList(component_list->components);
        component_list->components = NULL;
    }
    free(component_list);
}

cJSON *component_list_convertToJSON(component_list_t *component_list) {
    cJSON *item = cJSON_CreateObject();

    // component_list->components
    if (!component_list->components) {
        goto fail;
    }
    cJSON *components = cJSON_AddArrayToObject(item, "components");
    if(components == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *componentsListEntry;
    if (component_list->components) {
    list_ForEach(componentsListEntry, component_list->components) {
    cJSON *itemLocal = component_convertToJSON(componentsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(components, itemLocal);
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

component_list_t *component_list_parseFromJSON(cJSON *component_listJSON){

    component_list_t *component_list_local_var = NULL;

    // define the local list for component_list->components
    list_t *componentsList = NULL;

    // component_list->components
    cJSON *components = cJSON_GetObjectItemCaseSensitive(component_listJSON, "components");
    if (cJSON_IsNull(components)) {
        components = NULL;
    }
    if (!components) {
        goto end;
    }

    
    cJSON *components_local_nonprimitive = NULL;
    if(!cJSON_IsArray(components)){
        goto end; //nonprimitive container
    }

    componentsList = list_createList();

    cJSON_ArrayForEach(components_local_nonprimitive,components )
    {
        if(!cJSON_IsObject(components_local_nonprimitive)){
            goto end;
        }
        component_t *componentsItem = component_parseFromJSON(components_local_nonprimitive);

        list_addElement(componentsList, componentsItem);
    }


    component_list_local_var = component_list_create_internal (
        componentsList
        );

    return component_list_local_var;
end:
    if (componentsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, componentsList) {
            component_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(componentsList);
        componentsList = NULL;
    }
    return NULL;

}
