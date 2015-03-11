#include "runtime.h"

/*******************************************
 * Pascal Starting point
 *******************************************
 */
extern void __PascalMain(void);

char **c_argv;
int c_argc;

int main(int argc, char **argv)
{
    c_argv = argv;
    c_argc = argc;
    InitFiles();
    __PascalMain();
    return 0;
}
