#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "runtime.h"

void __Val_int(const String* s, int *res)
{
    sscanf((const char*)s->str, "%d", res);
}


void __Val_long(const String* s, int64_t *res)
{
    sscanf((const char*)s->str, "%"PRId64, res);
}
