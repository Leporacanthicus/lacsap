#include "runtime.h"
#include <string.h>

/*******************************************
 * Set functions
 *******************************************
 */

typedef struct
{
    unsigned int v[1];
} Set;

/*******************************************
 * Set functions
 *******************************************
 */
int __SetEqual(Set* a, Set* b, int setWords)
{
    return !memcmp(a->v, b->v, sizeof(*a) * setWords);
}

void __SetUnion(Set* res, Set* a, Set* b, int setWords)
{
    for (int i = 0; i < setWords; i++)
    {
	res->v[i] = a->v[i] | b->v[i];
    }
}

void __SetDiff(Set* res, Set* a, Set* b, int setWords)
{
    for (int i = 0; i < setWords; i++)
    {
	res->v[i] = a->v[i] & ~b->v[i];
    }
}

void __SetIntersect(Set* res, Set* a, Set* b, int setWords)
{
    for (int i = 0; i < setWords; i++)
    {
	res->v[i] = a->v[i] & b->v[i];
    }
}

/* Check if all values in a are in set b. */
int __SetContains(Set* a, Set* b, int setWords)
{
    for (int i = 0; i < setWords; i++)
    {
	if ((a->v[i] & b->v[i]) != a->v[i])
	    return 0;
    }
    return 1;
}
