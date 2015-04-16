#include "namedobject.h"
#include "expr.h"

void VarDef::dump(std::ostream& out) const
{
    out << "Name: " << Name() << " Type: ";
    Type()->dump(out);
    std::cerr << std::endl;
}

void FuncDef::dump(std::ostream& out) const
{
    out << "Function: Name: " << Name() << " Prototype:";
    proto->dump(out);
    out << std::endl;
}

void TypeDef::dump(std::ostream& out) const
{
    out << "Type: " << Name() << " type : ";
    Type()->dump(out);
    out << std::endl;
}

void ConstDef::dump(std::ostream& out) const
{
    out << "Const: " << Name() << " Value: " << constVal->Translate().ToString()
	<< std::endl;
}

void EnumDef::dump(std::ostream& out) const
{
    out << "Enum: " << Name() << " Value: " << enumValue << std::endl;
}

void WithDef::dump(std::ostream& out) const
{
    out << "With: " << Name() << " Actual: ";
    actual->dump(out);
    out << " Type: ";
    Type()->dump(out);
    out << std::endl; 
}

void MembFuncDef::dump(std::ostream& out) const
{
    out << "Membfunc: " << Name() << " Index:" << Index(); 
    Type()->dump(out);
    out << std::endl;
}


