#include "namedobject.h"
#include "expr.h"

void NamedObject::dump() const
{
    std::cerr << Name() << " Type: ";
    Type()->dump();
    std::cerr << std::endl;
}

void ConstDef::dump() const
{
    std::cerr << "Const: " << Name() << " Value: " << constVal->Translate().ToString() << std::endl;
}

void EnumDef::dump() const
{
    std::cerr << "Enum: " << Name() << " Value: " << enumValue << std::endl;
}

void WithDef::dump() const
{
    std::cerr << "With: " << Name() << " Actual: ";
    actual->dump();
    std::cerr << " Type: ";
    Type()->dump();
    std::cerr << std::endl;
}

void MembFuncDef::dump() const
{
    std::cerr << "Membfunc: " << Name() << " Index:" << Index();
    Type()->dump();
    std::cerr << std::endl;
}

void LabelDef::dump() const
{
    std::cerr << "Label: " << Name() << std::endl;
}
