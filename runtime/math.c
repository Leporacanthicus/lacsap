#include "runtime.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/* Use our own random number gemerator, so that it is consistent regardless
 * of what host system is used. Using linear congruent generator.
 */
static uint64_t rand_seed = 8919118912341193UL;

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
    uint64_t r = urand();
    r &= UINT_MAX;
    return r / (double)UINT_MAX;
}

int64_t __random_int(int64_t limit)
{
    return urand() % limit;
}

void __randomize(void)
{
    rand_seed = time(NULL);
}

void __random_set_seed(int64_t seed)
{
    rand_seed = seed;
}

double __frac(double x)
{
    double intpart;
    return modf(x, &intpart);
}
