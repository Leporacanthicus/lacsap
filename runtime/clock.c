#include "runtime.h"
#include <stdint.h>
#include <time.h>

uint64_t __Clock(void)
{
    return clock();
}
