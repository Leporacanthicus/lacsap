#include <time.h>
#include <stdint.h>
#include "runtime.h"

uint64_t  __Clock(void)
{
    return clock();
}
