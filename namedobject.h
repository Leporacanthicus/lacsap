#ifndef NAMEDOBJECT_H
#define NAMEDOBJECT_H

#include "expr.h"
#include <iostream>

// A set of functions to track different named objects, and their type. 

class NamedObject
{
public:
    NamedObject(const std::string& nm, Types::TypeDecl* ty, PrototypeAST* p = 0) 
	: name(nm), type(ty), proto(p) 
    {
	if (ty->Type() == Types::Function || ty->Type() == Types::Procedure)
	{
	    assert(p && "Prototype should not be NULL for functions and procedures");
	}
    }
    Types::TypeDecl* Type() const { return type; }
    const std::string& Name() const { return name; }
    PrototypeAST* Proto() const { return proto; }
    void dump() { std::cerr << "Name: " << name << " Type:" << type << std::endl; }
private:
    std::string      name;
    Types::TypeDecl* type;
    PrototypeAST*    proto;
};

#endif
