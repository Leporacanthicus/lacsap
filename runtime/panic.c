#include <stdio.h>
#include <stdlib.h>

void __Panic(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(11);
}
