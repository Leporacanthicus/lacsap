#ifndef VARIABLES_H
#define VARIABLES_H

#include "types.h"
#include <string>
#include <ostream>

class VarDef
{
public:
    VarDef(const std::string &nm, const std::string &ty) : name(nm), type(ty) {}

    const std::string Type() const { return type; }
    const std::string Name() const { return name; }
    void Dump(std::ostream& out)
    {
	out << "name: " << name << " type:" << type; 
    }
private:
    std::string        name;
    std::string        type;
};

#endif
