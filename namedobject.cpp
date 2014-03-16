#include "namedobject.h"
#include "expr.h"

void FuncDef::dump()
{
    std::cerr << "Function: Name: " << Name() << " Prototype:";
    proto->Dump();
    std::cerr << std::endl;
}

