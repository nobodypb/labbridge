/*
 * dl24m_diagnostics.h
 *
 * Passive bounded diagnostics, independent of driver parsing.
 */

#ifndef _dl24m_diagnostics_H_
#define _dl24m_diagnostics_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct dl24m_diagnostics_t dl24m_diagnostics_t;

#include "dl24m_register.h"
#include "dl24m_trace_entry.h"



typedef struct dl24m_diagnostics_t {
    list_t *entries; //nonprimitive container
    int dropped; //numeric
    list_t *registers; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} dl24m_diagnostics_t;

__attribute__((deprecated)) dl24m_diagnostics_t *dl24m_diagnostics_create(
    list_t *entries,
    int dropped,
    list_t *registers
);

void dl24m_diagnostics_free(dl24m_diagnostics_t *dl24m_diagnostics);

dl24m_diagnostics_t *dl24m_diagnostics_parseFromJSON(cJSON *dl24m_diagnosticsJSON);

cJSON *dl24m_diagnostics_convertToJSON(dl24m_diagnostics_t *dl24m_diagnostics);

#endif /* _dl24m_diagnostics_H_ */

