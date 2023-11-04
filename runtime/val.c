#include "runtime.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

void __Val_int(String* s, int* res)
{
    s->str[s->len] = 0;
    *res = strtol((const char*)&s->str, NULL, 0);
}

void __Val_long(String* s, int64_t* res)
{
    s->str[s->len] = 0;
    *res = strtoll((const char*)&s->str, NULL, 0);
}

void __Val_real(String* s, double* res)
{
    s->str[s->len] = 0;
    *res = strtod((const char*)&s->str, NULL);
}
