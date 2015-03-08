#include <stdio.h>
#include <stdlib.h>

void range_error(const char *file, int line, int low, int high, int actual)
{
    fprintf(stderr, "%s:%d: Out of range [expected: %d..%d, got %d]\n",
	    file, line, low, high, actual);
    exit(12);
}
