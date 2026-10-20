#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dl24m_bank_request.h"


char* dl24m_bank_request_bank_ToString(labbridge_api_dl24m_bank_request_BANK_e bank) {
    char* bankArray[] =  { "NULL", "1", "2", "3" };
    return bankArray[bank];
}

labbridge_api_dl24m_bank_request_BANK_e dl24m_bank_request_bank_FromString(char* bank){
    int stringToReturn = 0;
    char *bankArray[] =  { "NULL", "1", "2", "3" };
    size_t sizeofArray = sizeof(bankArray) / sizeof(bankArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(bank, bankArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static dl24m_bank_request_t *dl24m_bank_request_create_internal(
    int bank
    ) {
    dl24m_bank_request_t *dl24m_bank_request_local_var = malloc(sizeof(dl24m_bank_request_t));
    if (!dl24m_bank_request_local_var) {
        return NULL;
    }
    dl24m_bank_request_local_var->bank = bank;

    dl24m_bank_request_local_var->_library_owned = 1;
    return dl24m_bank_request_local_var;
}

__attribute__((deprecated)) dl24m_bank_request_t *dl24m_bank_request_create(
    int bank
    ) {
    return dl24m_bank_request_create_internal (
        bank
        );
}

void dl24m_bank_request_free(dl24m_bank_request_t *dl24m_bank_request) {
    if(NULL == dl24m_bank_request){
        return ;
    }
    if(dl24m_bank_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "dl24m_bank_request_free");
        return ;
    }
    listEntry_t *listEntry;
    free(dl24m_bank_request);
}

cJSON *dl24m_bank_request_convertToJSON(dl24m_bank_request_t *dl24m_bank_request) {
    cJSON *item = cJSON_CreateObject();

    // dl24m_bank_request->bank
    if (labbridge_api_dl24m_bank_request_BANK_NULL == dl24m_bank_request->bank) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "bank", dl24m_bank_request->bank) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

dl24m_bank_request_t *dl24m_bank_request_parseFromJSON(cJSON *dl24m_bank_requestJSON){

    dl24m_bank_request_t *dl24m_bank_request_local_var = NULL;

    // dl24m_bank_request->bank
    cJSON *bank = cJSON_GetObjectItemCaseSensitive(dl24m_bank_requestJSON, "bank");
    if (cJSON_IsNull(bank)) {
        bank = NULL;
    }
    if (!bank) {
        goto end;
    }

    
    if(!cJSON_IsNumber(bank))
    {
    goto end; //Numeric
    }


    dl24m_bank_request_local_var = dl24m_bank_request_create_internal (
        bank->valuedouble
        );

    return dl24m_bank_request_local_var;
end:
    return NULL;

}
