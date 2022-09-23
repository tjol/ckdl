#include "compat.h"

#include <stdlib.h>

#if !defined(HAVE_REALLOCF)
void *reallocf(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL) {
        free(ptr);
    }
    return new_ptr;
}
#endif
