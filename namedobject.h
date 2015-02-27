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
    virtual void dump(std::ostream& out) { out << "Name: " << name << std::endl; }
    NamedKind getKind() const { return kind; }
private:
    const NamedKind kind;
    std::string      name;
};

class VarDef : public NamedObject
{
public:
    VarDef(const std::string& nm, Types::TypeDecl* ty, bool ref = false, bool external = false) 
	: NamedObject(NK_Var, nm), type(ty), isRef(ref), isExt(external)
    {
    }
    Types::TypeDecl* Type() const { return type; }
    bool IsRef() const { return isRef; }
    bool IsExternal() const { return isExt; }
    void dump(std::ostream& out)
    { 
	out << "Name: " << Name() << " Type: ";
	type->dump(out);
	std::cerr << std::endl;
    }
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Var; }
private:
    Types::TypeDecl* type;
    bool             isRef;   /* "var" arguments are "references" */
    bool             isExt;   /* global variable defined outside this module */
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
    void dump(std::ostream& out);
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
    void dump(std::ostream& out)
    { 
	out << "Type: " << Name() << " type : ";
	type->dump(out);
	out << std::endl;
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
    void dump(std::ostream& out)
    { 
	out << "Const: " << Name() << " Value: " << constVal->Translate().ToString()
	    << std::endl;
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
    void dump(std::ostream& out)
    { 
	out << "Enum: " << Name() << " Value: " << enumValue << std::endl;
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
	: NamedObject(NK_With, nm), actual(act), type(ty)
    {
    }
    Types::TypeDecl* Type() const { return type; }
    ExprAST* Actual() const { return actual; }
    void dump(std::ostream& out);
    static bool classof(const NamedObject* e) { return e->getKind() == NK_With; }
private:
    ExprAST*         actual;
    Types::TypeDecl* type;
};

#endif
