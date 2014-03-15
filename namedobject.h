#ifndef NAMEDOBJECT_H
#define NAMEDOBJECT_H

#include "types.h"
#include <iostream>

class NamedObject
{
public:
    NamedObject(const std::string& nm) 
	: name(nm) 
    {
    }
    virtual Types::TypeDecl* Type() const = 0;
    const std::string& Name() const { return name; }
    virtual void dump() { std::cerr << "Name: " << name << std::endl; }
private:
    std::string      name;
};


class VarDef : public NamedObject
{
public:
    VarDef(const std::string& nm, Types::TypeDecl* ty, bool ref = false) 
	: NamedObject(nm), type(ty), isRef(ref) 
    {
    }
    Types::TypeDecl* Type() const { return type; }
    bool IsRef() const { return isRef; }
    void dump() 
    { 
	std::cerr << "Name: " << Name() << " Type: ";
	type->dump();
	std::cerr << std::endl; 
    }
private:
    Types::TypeDecl* type;
    bool             isRef;   /* "var" arguments are "references" */
};


class FuncDef : public NamedObject
{
public:
    FuncDef(const std::string& nm, Types::TypeDecl* ty, PrototypeAST* p) 
	: NamedObject(nm), type(ty), proto(p)
    {
	assert(p && "Need to pass a prototype for funcdef");
    }
    PrototypeAST* Proto() const { return proto; }
    Types::TypeDecl* Type() const { return type; }
    void dump();
private:
    Types::TypeDecl* type;
    PrototypeAST* proto;
};


#endif
