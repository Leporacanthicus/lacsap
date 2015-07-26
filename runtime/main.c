#include "runtime.h"

/*******************************************
 * Pascal Starting point
 *******************************************
 */
extern void __PascalMain(void);

char **c_argv;
int c_argc;

static void InitModules()
{
    typedef void (InitFunc)(void);
    extern InitFunc* UnitIniList[];
    
    for(InitFunc** p = UnitIniList; *p; p++)
    {
	(*p)();
    }
}

int main(int argc, char **argv)
{
    c_argv = argv;
    c_argc = argc;
    InitFiles();
    InitModules();
    __PascalMain();
    return 0;
}
