#define __USE_ISOC11
#include "runtime.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>

void __csqrt(struct Complex* res, struct Complex a)
{
    res->r = sqrt((a.r + sqrt(a.r * a.r + a.i * a.i)) / 2);
    res->i = copysign(sqrt((-a.r + sqrt(a.r * a.r + a.i * a.i)) / 2), a.i);
}

double __carg(struct Complex a)
{
    return atan2(a.i, a.r);
}

void __csin(struct Complex* res, struct Complex a)
{
    // sin(x + iy) = sin(x) * cosh(y) + i cos(x) * sinh(y)
    res->r = sin(a.r) * cosh(a.i);
    res->i = cos(a.r) * sinh(a.i);
}

void __ccos(struct Complex* res, struct Complex a)
{
    // sin(x + iy) = cos(x) * cosh(y) - i sin(x) * sinh(y)
    res->r = cos(a.r) * cosh(a.i);
    res->i = -sin(a.r) * sinh(a.i);
}

void __ctan(struct Complex* res, struct Complex a)
{
    // tan(x + iy) = sin(2x)/(cos(2x)+cosh(2y)) + i sinh(2y)/(cos(2x)+cosh(2y))
    double         div = cos(2 * a.r) + cosh(2 * a.i);
    res->r = sin(2 * a.r) / div;
    res->i = sinh(2 * a.i) / div;
}

void __cexp(struct Complex* res, struct Complex a)
{
    // exp(x + iy) = exp(x) * cos(y) + i exp(x) * sin(y)
    double         ex = exp(a.r);
    res->r = ex * cos(a.i);
    res->i = ex * sin(a.i);
}

// Note: clog, becuase the ln function in C library is log.
void __clog(struct Complex* res, struct Complex a)
{
    // ln = 0.5 * ln(x^2 + y^2) + i atan2(y, x)
    res->r = 0.5 * log(a.r * a.r + a.i * a.i);
    res->i = atan2(a.i, a.r);
}

void __cpolar(struct Complex* res, double r, double t)
{
    res->r = r * cos(t);
    res->i = r * sin(t);
}

void __catan(struct Complex* res, struct Complex a)
{
    // atan(x + iy) = atan2(2.0*x, 1-x*x-y*y) / 2 + i * ln((x*x + (y+1)*(y+1)) / (x*x + (y-1)*(y-1))) / 4
    res->r = 0.5 * atan2(2 * a.r, 1 - a.r * a.r - a.i * a.i);
    res->i = 0.25 * log((a.r * a.r + (a.i + 1) * (a.i + 1)) / (a.r * a.r + (a.i - 1) * (a.i - 1)));
}

void __cpow(struct Complex* res, struct Complex a, double b)
{
    // a^b = exp(b * ln(a))
    // a = 0 -> 0
    // b = 0 -> 1

    struct Complex zeros = { 0.0, 0.0 };
    *res = zeros;
    if (!a.r && !a.i)
    {
	return;
    }
    if (!b)
    {
	res->r = 1.0;
	return;
    }
    double         re = 0.5 * log(a.r * a.r + a.i * a.i) * b;
    double         im = atan2(a.i, a.r) * b;
    
    struct Complex tmp = { re, im };

    __cexp(res, tmp);
}
