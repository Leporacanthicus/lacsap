#ifndef VARIABLES_H
#define VARIABLES_H

#include "types.h"
#include <string>
#include <ostream>

class VarDef
{
public:
    VarDef(const std::string &nm, const std::string &ty, bool ref = false) 
	: name(nm), type(ty), isRef(ref) {}

    const std::string Type() const { return type; }
    const std::string Name() const { return name; }
    bool IsRef() const { return isRef; }
    void Dump(std::ostream& out)
    {
	out << "name: " << name << " type:" << type; 
    }
private:
    std::string  name;
    std::string  type;
    bool         isRef;   /* "var" arguments are "references" */
};

#endif
