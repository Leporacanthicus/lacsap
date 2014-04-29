#include "namedobject.h"
#include "expr.h"

void FuncDef::dump()
{
    std::cerr << "Function: Name: " << Name() << " Prototype:";
    proto->dump();
    std::cerr << std::endl;
}

