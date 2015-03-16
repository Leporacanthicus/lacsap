#include <math.h>
#include <stdlib.h>
#include "runtime.h"

/*******************************************
 * Math and such
 *******************************************
 */
double __random(void)
{
    return rand() / (double)RAND_MAX;
}

double __arctan2(double x, double y)
{
    return atan2(x, y);
}

double __fmod(double x, double y)
{
    return fmod(x, y);
}
