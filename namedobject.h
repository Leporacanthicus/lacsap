#ifndef NAMEDOBJECT_H
#define NAMEDOBJECT_H

#include "expr.h"
#include <iostream>

// A set of functions to track different named objects, and their type. 

class NamedObject
{
public:
    NamedObject(const std::string& nm, const std::string& ty, const PrototypeAST* p = 0) 
	: name(nm), type(ty), proto(p) 
    {
	if (ty == "function" || ty == "procedure")
	{
	    assert(p && "Prototype should not be NULL for functions and procedures");
	}
    }
    const std::string& Type() const { return type; }
    const std::string& Name() const { return name; }
    const PrototypeAST* Proto() const { return proto; }
    void dump() { std::cerr << "Name: " << name << " Type:" << type << std::endl; }
private:
    std::string         name;
    std::string         type;
    const PrototypeAST* proto;
};

#endif
