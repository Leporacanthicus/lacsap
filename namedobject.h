#ifndef NAMEDOBJECT_H
#define NAMEDOBJECT_H

#include "types.h"
#include "constants.h"
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

class TypeDef : public NamedObject
{
public:
    TypeDef(const std::string& nm, Types::TypeDecl* ty) 
	: NamedObject(nm), type(ty) { }
    Types::TypeDecl* Type() const { return type; }
    void dump() 
    { 
	std::cerr << "Type: " << Name() << " type : ";
	type->dump();
	std::cerr << std::endl; 
    }
private:
    Types::TypeDecl* type;
};

class ConstDef : public NamedObject
{
public:
    ConstDef(const std::string& nm, Constants::ConstDecl* cv)
	: NamedObject(nm), constVal(cv) { }

    Constants::ConstDecl* ConstValue() const { return constVal; }
    Types::TypeDecl* Type() const { return 0; }
    void dump() 
    { 
	std::cerr << "Const: " << Name() << " Value: " << constVal->Translate().ToString();
	std::cerr << std::endl; 
    }
private:
    Constants::ConstDecl *constVal;
};

class EnumDef : public NamedObject
{
public:
    EnumDef(const std::string& nm, int v)
	: NamedObject(nm), enumValue(v) { }
    int Value() const { return enumValue; }
    Types::TypeDecl* Type() const { return 0; }
    void dump() 
    { 
	std::cerr << "Enum: " << Name() << " Value: " << enumValue << std::endl; 
    }
private:
    int enumValue;
};

#endif
