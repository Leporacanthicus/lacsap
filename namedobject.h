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
	NK_MembFunc,
	NK_Label,
    };
    NamedObject(NamedKind k, const std::string& nm, Types::TypeDecl* ty) 
	: kind(k), name(nm), type(ty) {}
    virtual ~NamedObject() {}
    Types::TypeDecl* Type() const { return type; }
    const std::string& Name() const { return name; }
    virtual void dump(std::ostream& out) const;
    NamedKind getKind() const { return kind; }
private:
    NamedKind kind;
    std::string name;
    Types::TypeDecl* type;
};


class VarDef : public NamedObject
{
public:
    VarDef(const std::string& nm, Types::TypeDecl* ty, bool ref = false, bool external = false) 
	: NamedObject(NK_Var, nm, ty), isRef(ref), isExt(external) {}
    bool IsRef() const { return isRef; }
    bool IsExternal() const { return isExt; }
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Var; }
private:
    bool isRef;   /* "var" arguments are "references" */
    bool isExt;   /* global variable defined outside this module */
};

inline bool operator<(const VarDef& lhs, const VarDef& rhs) { return lhs.Name() < rhs.Name(); }

class FuncDef : public NamedObject
{
public:
    FuncDef(const std::string& nm, Types::TypeDecl* ty, PrototypeAST* p) 
	: NamedObject(NK_Func, nm, ty), proto(p)
    {
	assert(p && "Need to pass a prototype for funcdef");
    }
    PrototypeAST* Proto() const { return proto; }
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Func; }
private:
    PrototypeAST* proto;
};

class TypeDef : public NamedObject
{
public:
    TypeDef(const std::string& nm, Types::TypeDecl* ty) 
	: NamedObject(NK_Type, nm, ty) {}
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Type; }
};

class ConstDef : public NamedObject
{
public:
    ConstDef(const std::string& nm, const Constants::ConstDecl* cv)
	: NamedObject(NK_Const, nm, 0), constVal(cv) { }
    const Constants::ConstDecl* ConstValue() const { return constVal; }
    void dump(std::ostream& out) const override;
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Const; }
private:
    const Constants::ConstDecl *constVal;
};

class EnumDef : public NamedObject
{
public:
    EnumDef(const std::string& nm, int v, Types::TypeDecl* ty)
	: NamedObject(NK_Enum, nm, ty), enumValue(v) { }
    int Value() const { return enumValue; }
    void dump(std::ostream& out) const override;
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Enum; }
private:
    int enumValue;
};

class WithDef : public NamedObject
{
public:
    WithDef(const std::string& nm, ExprAST* act, Types::TypeDecl* ty) 
	: NamedObject(NK_With, nm, ty), actual(act) {}
    ExprAST* Actual() const { return actual; }
    void dump(std::ostream& out) const override;
    static bool classof(const NamedObject* e) { return e->getKind() == NK_With; }
private:
    ExprAST* actual;
};

class MembFuncDef : public NamedObject
{
public:
    MembFuncDef(const std::string& nm, int idx, Types::TypeDecl* ty)
	: NamedObject(NK_MembFunc, nm, ty), index(idx) {}
    int Index() const { return index; }
    void dump(std::ostream& out) const override;
    static bool classof(const NamedObject* e) { return e->getKind() == NK_MembFunc; }
private:
    int index;
};

class LabelDef : public NamedObject
{
public:
    LabelDef(int label) 
	: NamedObject(NK_Label, std::to_string(label), NULL) {};
    void dump(std::ostream& out) const override;
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Label; }
};


#endif
