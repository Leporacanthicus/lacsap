#include "namedobject.h"
#include "expr.h"

void FuncDef::dump(std::ostream& out)
{
    out << "Function: Name: " << Name() << " Prototype:";
    proto->dump(out);
    out << std::endl;
}


void WithDef::dump(std::ostream& out) 
{ 
    out << "With: " << Name() << " Actual: "; 
    actual->dump(out);
    out << " Type: ";
    type->dump(out);
    out << std::endl; 
}


