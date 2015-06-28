#include "namedobject.h"
#include "expr.h"


void NamedObject::dump(std::ostream& out) const
{
    out << Name() << " Type: ";
    Type()->dump(out);
    std::cerr << std::endl;
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

void LabelDef::dump(std::ostream& out) const
{
    out << "Label: " << Name() << std::endl;
}
