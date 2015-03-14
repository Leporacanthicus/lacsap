#include <string.h>
#include "runtime.h"

extern char **c_argv;
extern int c_argc;

String __ParamStr(int n)
{
    String s = {};
    if (n < c_argc)
    {
	size_t len = strlen(c_argv[n]);
	if (len > 255)
	{
	    len = 255;
	}
	memcpy(s.str, c_argv[n], len);
	s.len = len;
    }
    return s;
}

int __ParamCount()
{
    return c_argc-1;
}
