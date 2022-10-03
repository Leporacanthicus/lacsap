#ifndef NAMEDOBJECT_H
#define NAMEDOBJECT_H

#include "constants.h"
#include "types.h"
#include <iostream>
#include <llvm/Support/Casting.h>

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
    NamedObject(NamedKind k, const std::string& nm, Types::TypeDecl* ty) : kind(k), name(nm), type(ty) {}
    virtual ~NamedObject() {}
    Types::TypeDecl*   Type() const { return type; }
    const std::string& Name() const { return name; }
    virtual void       dump(std::ostream& out) const;
    NamedKind          getKind() const { return kind; }

private:
    NamedKind        kind;
    std::string      name;
    Types::TypeDecl* type;
};

class VarDef : public NamedObject
{
public:
    enum class Flags
    {
	Reference = 1 << 0,
	External = 1 << 1,
	Protected = 1 << 2,
	Closure = 1 << 3,
	None = 0,
	All = Reference | External | Protected | Closure,
    };

    VarDef(const std::string& nm, Types::TypeDecl* ty, Flags f = Flags::None)
        : NamedObject(NK_Var, nm, ty), flags(f)
    {
    }
    bool        IsRef() const;
    bool        IsExternal() const;
    bool        IsProtected() const;
    bool        IsClosure() const;
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Var; }

private:
    Flags flags;
};

constexpr VarDef::Flags operator&(const VarDef::Flags a, const VarDef::Flags b)
{
    return static_cast<VarDef::Flags>(static_cast<const int>(a) & static_cast<const int>(b));
}

constexpr VarDef::Flags operator|(const VarDef::Flags a, const VarDef::Flags b)
{
    return static_cast<VarDef::Flags>(static_cast<const int>(a) | static_cast<const int>(b));
}

constexpr VarDef::Flags operator|=(VarDef::Flags& a, const VarDef::Flags b)
{
    a = a | b;
    return a;
}

inline bool VarDef::IsRef() const
{
    return (flags & VarDef::Flags::Reference) != VarDef::Flags::None;
}
inline bool VarDef::IsExternal() const
{
    return (flags & VarDef::Flags::External) != VarDef::Flags::None;
}
inline bool VarDef::IsProtected() const
{
    return (flags & VarDef::Flags::Protected) != VarDef::Flags::None;
}
inline bool VarDef::IsClosure() const
{
    return (flags & VarDef::Flags::Closure) != VarDef::Flags::None;
}

inline bool operator<(const VarDef& lhs, const VarDef& rhs)
{
    return lhs.Name() < rhs.Name();
}

class FuncDef : public NamedObject
{
public:
    FuncDef(const std::string& nm, Types::TypeDecl* ty, PrototypeAST* p)
        : NamedObject(NK_Func, nm, ty), proto(p)
    {
	assert(p && "Need to pass a prototype for funcdef");
    }
    PrototypeAST* Proto() const { return proto; }
    static bool   classof(const NamedObject* e) { return e->getKind() == NK_Func; }

private:
    PrototypeAST* proto;
};

class TypeDef : public NamedObject
{
public:
    TypeDef(const std::string& nm, Types::TypeDecl* ty, bool restr = false)
        : NamedObject(NK_Type, nm, ty), restricted(restr)
    {
    }
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Type; }
    bool        IsRestricted() { return restricted; }

private:
    bool restricted;
};

class ConstDef : public NamedObject
{
public:
    ConstDef(const std::string& nm, const Constants::ConstDecl* cv)
        : NamedObject(NK_Const, nm, 0), constVal(cv)
    {
    }
    const Constants::ConstDecl* ConstValue() const { return constVal; }
    void                        dump(std::ostream& out) const override;
    static bool                 classof(const NamedObject* e) { return e->getKind() == NK_Const; }

private:
    const Constants::ConstDecl* constVal;
};

class EnumDef : public NamedObject
{
public:
    EnumDef(const std::string& nm, int v, Types::TypeDecl* ty) : NamedObject(NK_Enum, nm, ty), enumValue(v) {}
    int         Value() const { return enumValue; }
    void        dump(std::ostream& out) const override;
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Enum; }

private:
    int enumValue;
};

class WithDef : public NamedObject
{
public:
    WithDef(const std::string& nm, ExprAST* act, Types::TypeDecl* ty)
        : NamedObject(NK_With, nm, ty), actual(act)
    {
    }
    ExprAST*    Actual() const { return actual; }
    void        dump(std::ostream& out) const override;
    static bool classof(const NamedObject* e) { return e->getKind() == NK_With; }

private:
    ExprAST* actual;
};

class MembFuncDef : public NamedObject
{
public:
    MembFuncDef(const std::string& nm, int idx, Types::TypeDecl* ty)
        : NamedObject(NK_MembFunc, nm, ty), index(idx)
    {
    }
    int         Index() const { return index; }
    void        dump(std::ostream& out) const override;
    static bool classof(const NamedObject* e) { return e->getKind() == NK_MembFunc; }

private:
    int index;
};

class LabelDef : public NamedObject
{
public:
    LabelDef(int label) : NamedObject(NK_Label, std::to_string(label), NULL){};
    void        dump(std::ostream& out) const override;
    static bool classof(const NamedObject* e) { return e->getKind() == NK_Label; }
};

#endif
