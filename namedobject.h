#ifndef NAMEDOBJECT_H
#define NAMEDOBJECT_H

#include "types.h"
#include "constants.h"
#include <llvm/Support/Casting.h>
#include <iostream>

class ExprAST;
class PrototypeAST;

class NamedObject
{
public:
    enum NamedKind
    {
	NK_Var,
	NK_Func,
	NK_Type,
	NK_Const,
	NK_Enum,
	NK_Builtin,
	NK_With,
    };
    NamedObject(NamedKind k, const std::string& nm) 
	: kind(k), name(nm) 
    {
    }
    virtual ~NamedObject() {}
    virtual Types::TypeDecl* Type() const = 0;
    const std::string& Name() const { return name; }
    virtual void dump() { std::cerr << "Name: " << name << std::endl; }
    NamedKind getKind() const { return kind; }
private:
    const NamedKind kind;
    std::string      name;
};

class VarDef : public NamedObject
{
public:
    VarDef(const std::string& nm, Types::TypeDecl* ty, bool ref = false) 
	: NamedObject(NK_Var, nm), type(ty), isRef(ref) 
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
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Var; }
private:
    Types::TypeDecl* type;
    bool             isRef;   /* "var" arguments are "references" */
};

class FuncDef : public NamedObject
{
public:
    FuncDef(const std::string& nm, Types::TypeDecl* ty, PrototypeAST* p) 
	: NamedObject(NK_Func, nm), type(ty), proto(p)
    {
	assert(p && "Need to pass a prototype for funcdef");
    }
    PrototypeAST* Proto() const { return proto; }
    Types::TypeDecl* Type() const { return type; }
    void dump();
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Func; }
private:
    Types::TypeDecl* type;
    PrototypeAST* proto;
};

class TypeDef : public NamedObject
{
public:
    TypeDef(const std::string& nm, Types::TypeDecl* ty) 
	: NamedObject(NK_Type, nm), type(ty) { }
    Types::TypeDecl* Type() const { return type; }
    void dump() 
    { 
	std::cerr << "Type: " << Name() << " type : ";
	type->dump();
	std::cerr << std::endl; 
    }
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Type; }
private:
    Types::TypeDecl* type;
};

class ConstDef : public NamedObject
{
public:
    ConstDef(const std::string& nm, Constants::ConstDecl* cv)
	: NamedObject(NK_Const, nm), constVal(cv) { }

    Constants::ConstDecl* ConstValue() const { return constVal; }
    Types::TypeDecl* Type() const { return 0; }
    void dump() 
    { 
	std::cerr << "Const: " << Name() << " Value: " << constVal->Translate().ToString();
	std::cerr << std::endl; 
    }
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Const; }
private:
    Constants::ConstDecl *constVal;
};

class EnumDef : public NamedObject
{
public:
    EnumDef(const std::string& nm, int v, Types::TypeDecl* ty)
	: NamedObject(NK_Enum, nm), enumValue(v), type(ty) { }
    int Value() const { return enumValue; }
    Types::TypeDecl* Type() const { return type; }
    void dump() 
    { 
	std::cerr << "Enum: " << Name() << " Value: " << enumValue << std::endl; 
    }
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Enum; }
private:
    int              enumValue;
    Types::TypeDecl* type;
};

class WithDef : public NamedObject
{
public:
    WithDef(const std::string& nm, ExprAST* act, Types::TypeDecl* ty) 
	: NamedObject(NK_With, nm), actual(act)
    {
    }
    Types::TypeDecl* Type() const { return type; }
    ExprAST* Actual() const { return actual; }
    void dump();
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Enum; }
private:
    ExprAST*         actual;
    Types::TypeDecl* type;
};

#endif
