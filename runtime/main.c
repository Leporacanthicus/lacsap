#include "runtime.h"

/*******************************************
 * Pascal Starting point
 *******************************************
 */
extern void __PascalMain(void);

int main()
{
    InitFiles();
    __PascalMain();
    return 0;
}
