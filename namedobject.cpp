#include "namedobject.h"
#include "expr.h"

void FuncDef::dump()
{
    std::cerr << "Function: Name: " << Name() << " Prototype:";
    proto->dump();
    std::cerr << std::endl;
}


void WithDef::dump() 
{ 
    std::cerr << "With: " << Name() << " Actual: "; 
    actual->dump();
    std::cerr << " Type: ";
    type->dump();
    std::cerr << std::endl; 
}


