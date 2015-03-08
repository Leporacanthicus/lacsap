#include <stdlib.h>
#include "runtime.h"

/*******************************************
 * Memory allocation functions
 *******************************************
 */
void* __new(int size)
{
    return malloc(size);
}

void __dispose(void* ptr)
{
    free(ptr);
}
