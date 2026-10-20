/*
 * dl24m_registers.h
 *
 * 
 */

#ifndef _dl24m_registers_H_
#define _dl24m_registers_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct dl24m_registers_t dl24m_registers_t;

#include "dl24m_register.h"



typedef struct dl24m_registers_t {
    list_t *registers; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} dl24m_registers_t;

__attribute__((deprecated)) dl24m_registers_t *dl24m_registers_create(
    list_t *registers
);

void dl24m_registers_free(dl24m_registers_t *dl24m_registers);

dl24m_registers_t *dl24m_registers_parseFromJSON(cJSON *dl24m_registersJSON);

cJSON *dl24m_registers_convertToJSON(dl24m_registers_t *dl24m_registers);

#endif /* _dl24m_registers_H_ */

