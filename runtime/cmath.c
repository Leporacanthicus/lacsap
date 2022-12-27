#define __USE_ISOC11
#include "runtime.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>

struct Complex __csqrt(struct Complex a)
{
    double         re = sqrt((a.r + sqrt(a.r * a.r + a.i * a.i)) / 2);
    double         im = copysign(sqrt((-a.r + sqrt(a.r * a.r + a.i * a.i)) / 2), a.i);
    struct Complex res = { re, im };
    return res;
}

double __carg(struct Complex a)
{
    return atan2(a.i, a.r);
}
