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

struct Complex __csin(struct Complex a)
{
    // sin(x + iy) = sin(x) * cosh(y) + i cos(x) * sinh(y)
    double         re = sin(a.r) * cosh(a.i);
    double         im = cos(a.r) * sinh(a.i);
    struct Complex res = { re, im };
    return res;
}

struct Complex __ccos(struct Complex a)
{
    // sin(x + iy) = cos(x) * cosh(y) - i sin(x) * sinh(y)
    double         re = cos(a.r) * cosh(a.i);
    double         im = -sin(a.r) * sinh(a.i);
    struct Complex res = { re, im };
    return res;
}
