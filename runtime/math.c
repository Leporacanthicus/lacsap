#include "runtime.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>

/* Use our own random number gemerator, so that it is consistent regardless
 * of what host system is used. Using linear congruent generator.
 */
static unsigned rand_seed = 12341193U;

static const unsigned rand_mul = 1103515245U;
static const unsigned rand_add = 12345;

static unsigned urand()
{
    rand_seed = rand_mul * rand_seed + rand_add;
    return rand_seed;
}
/*******************************************
 * Math and such
 *******************************************
 */
double __random(void)
{
    return urand() / (double)UINT_MAX;
    ;
}

double __arctan2(double x, double y)
{
    return atan2(x, y);
}

double __fmod(double x, double y)
{
    return fmod(x, y);
}
