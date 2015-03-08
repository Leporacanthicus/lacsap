#include <string.h>

/* Return >0 if a is greater than b, 
 * Return <0 if a is less than b. 
 * Return 0 if a == b. 
 */ 
int __ArrCompare(char* a, char* b, int size)
{
    return memcmp(a, b, size);
}
