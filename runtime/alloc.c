#include "runtime.h"
#include <stdlib.h>

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
