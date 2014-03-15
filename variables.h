#ifndef VARIABLES_H
#define VARIABLES_H

#include "types.h"
#include <string>
#include <ostream>

class VarDef
{
public:
    VarDef(const std::string &nm, Types::TypeDecl* ty, bool ref = false) 
	: name(nm), type(ty), isRef(ref) 
    {
    }

    Types::TypeDecl* Type() const { return type; }
    const std::string Name() const { return name; }
    bool IsRef() const { return isRef; }
    void Dump()
    {
	std::cerr << "name: " << name << " type:" << type->to_string(); 
    }
private:
    std::string  name;
    Types::TypeDecl *type;
    bool         isRef;   /* "var" arguments are "references" */
};

#endif
