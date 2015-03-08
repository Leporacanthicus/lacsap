#include <time.h>
#include "runtime.h"

long __Clock(void)
{
    return clock();
}
