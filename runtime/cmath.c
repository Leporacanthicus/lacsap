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

struct Complex __ctan(struct Complex a)
{
    // tan(x + iy) = sin(2x)/(cos(2x)+cosh(2y)) + i sinh(2y)/(cos(2x)+cosh(2y))
    double         div = cos(2 * a.r) + cosh(2 * a.i);
    double         re = sin(2 * a.r) / div;
    double         im = sinh(2 * a.i) / div;
    struct Complex res = { re, im };
    return res;
}

struct Complex __cexp(struct Complex a)
{
    // exp(x + iy) = exp(x) * cos(y) + i exp(x) * sin(y)
    double         ex = exp(a.r);
    double         re = ex * cos(a.i);
    double         im = ex * sin(a.i);
    struct Complex res = { re, im };
    return res;
}

// Note: clog, becuase the ln function in C library is log.
struct Complex __clog(struct Complex a)
{
    // ln = 0.5 * ln(x^2 + y^2) + i atan2(y, x)
    double         re = 0.5 * log(a.r * a.r + a.i * a.i);
    double         im = atan2(a.i, a.r);
    struct Complex res = { re, im };
    return res;
}

struct Complex __cpolar(double r, double t)
{
    double re = r * cos(t);
    double im = r * sin(t);

    struct Complex res = { re, im };
    return res;
}
