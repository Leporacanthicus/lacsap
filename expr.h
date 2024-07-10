#ifndef EXPR_H
#define EXPR_H

#include "builtin.h"
#include "namedobject.h"
#include "stack.h"
#include "token.h"
#include "types.h"
#include "visitor.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Casting.h>
#include <set>
#include <string>
#include <vector>

extern llvm::LLVMContext theContext;

class ArrayInit
{
public:
    enum class InitKind
    {
	Single,
	Range,
	Otherwise
    };
    ArrayInit(int64_t s, int64_t e, ExprAST* v) : start{ s }, end{ e }, value{ v }, kind{ InitKind::Range } {}
    ArrayInit(int64_t s, ExprAST* v) : start{ s }, end{ 0 }, value{ v }, kind{ InitKind::Single } {}
    ArrayInit(ExprAST* v) : start{ 0 }, end{ 0 }, value{ v }, kind{ InitKind::Otherwise } {}

    InitKind Kind() const { return kind; }
    ExprAST* Value() const { return value; }
    int64_t  Start() const { return start; }
    int64_t  End() const { return end; }

private:
    int64_t  start;
    int64_t  end;
    ExprAST* value;
    InitKind kind;
};

class RecordInit
{
public:
    RecordInit(const std::vector<int>& el, ExprAST* v) : elements(el), value(v) {}
    ExprAST*                Value() const { return value; }
    const std::vector<int>& Elements() const { return elements; }

private:
    std::vector<int> elements;
    ExprAST*         value;
};

const size_t MIN_ALIGN = 4;

class ExprAST : public Visitable<ExprAST>
{
    friend class TypeCheckVisitor;

public:
    enum ExprKind
    {
	EK_Expr,
	EK_RealExpr,
	EK_IntegerExpr,
	EK_CharExpr,
	EK_NilExpr,

	// Addressable types
	EK_AddressableExpr,
	EK_StringExpr,
	EK_SetExpr,
	EK_VariableExpr,
	EK_ArrayExpr,
	EK_DynArrayExpr,
	EK_PointerExpr,
	EK_FilePointerExpr,
	EK_FieldExpr,
	EK_VariantFieldExpr,
	EK_FunctionExpr,
	EK_TypeCastExpr,
	EK_ArraySlice,
	EK_LastAddressable,

	EK_BinaryExpr,
	EK_UnaryExpr,
	EK_RangeExpr,
	EK_Block,
	EK_AssignExpr,
	EK_VarDecl,
	EK_Function,
	EK_Prototype,
	EK_CallExpr,
	EK_BuiltinExpr,
	EK_IfExpr,
	EK_ForExpr,
	EK_WhileExpr,
	EK_RepeatExpr,
	EK_Write,
	EK_Read,
	EK_LabelExpr,
	EK_CaseExpr,
	EK_WithExpr,
	EK_RangeReduceExpr,
	EK_RangeCheckExpr,
	EK_SizeOfExpr,
	EK_VTableExpr,
	EK_VirtFunction,
	EK_Goto,
	EK_Unit,
	EK_Closure,
	EK_Trampoline,

	EK_InitValue,
	EK_InitArray,
	EK_InitRecord,
    };
    ExprAST(const Location& w, ExprKind k, Types::TypeDecl* ty = 0) : loc(w), kind(k), type(ty) {}
    virtual ~ExprAST() {}
    void                 dump() const;
    virtual void         DoDump() const = 0;
    void                 accept(ASTVisitor& v) override { v.visit(this); }
    virtual llvm::Value*     CodeGen() { ICE("CodeGen Not Implemented"); }
    ExprKind                 getKind() const { return kind; }
    void                     SetType(Types::TypeDecl* ty) { type = ty; }
    virtual Types::TypeDecl* Type() const { return type; }
    void                     EnsureSized() const;
    const Location&          Loc() const { return loc; }

private:
    const Location loc;
    const ExprKind kind;

protected:
    Types::TypeDecl* type;
};

class RealExprAST : public ExprAST
{
public:
    RealExprAST(const Location& w, double v) : ExprAST(w, EK_RealExpr, Types::Get<Types::RealDecl>()), val(v)
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_RealExpr; }

private:
    double val;
};

class IntegerExprAST : public ExprAST
{
public:
    IntegerExprAST(const Location& w, uint64_t v, Types::TypeDecl* ty)
        : ExprAST(w, EK_IntegerExpr, ty), val(v)
    {
    }
    IntegerExprAST(const Location& w, ExprKind ek, uint64_t v, Types::TypeDecl* ty)
        : ExprAST(w, ek, ty), val(v)
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    uint64_t     Int() { return val; }
    static bool  classof(const ExprAST* e)
    {
	return e->getKind() == EK_IntegerExpr || e->getKind() == EK_CharExpr;
    }

protected:
    uint64_t val;
};

class CharExprAST : public IntegerExprAST
{
public:
    CharExprAST(const Location& w, char v) : IntegerExprAST(w, EK_CharExpr, v, Types::Get<Types::CharDecl>())
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_CharExpr; }
};

class NilExprAST : public ExprAST
{
public:
    NilExprAST(const Location& w)
        : ExprAST(w, EK_NilExpr, new Types::PointerDecl(Types::Get<Types::VoidDecl>()))
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_NilExpr; }
};

class AddressableAST : public ExprAST
{
public:
    AddressableAST(const Location& w, ExprKind k, Types::TypeDecl* ty) : ExprAST(w, k, ty) {}
    virtual llvm::Value*      Address() { ICE("Address needs implementing"); }
    llvm::Value*              CodeGen() override;
    virtual const std::string Name() const { return ""; }
    static bool               classof(const ExprAST* e)
    {
	return e->getKind() >= EK_AddressableExpr && e->getKind() <= EK_LastAddressable;
    }
};

class StringExprAST : public AddressableAST
{
public:
    StringExprAST(const Location& w, const std::string& v, Types::TypeDecl* ty)
        : AddressableAST(w, EK_StringExpr, ty), val(v)
    {
    }
    void               DoDump() const override;
    llvm::Value*       CodeGen() override;
    llvm::Value*       Address() override;
    const std::string& Str() const { return val; }
    static bool        classof(const ExprAST* e) { return e->getKind() == EK_StringExpr; }

private:
    std::string val;
};

class SetExprAST : public AddressableAST
{
    friend class TypeCheckVisitor;

public:
    SetExprAST(const Location& w, const std::vector<ExprAST*>& v, Types::TypeDecl* ty)
        : AddressableAST(w, EK_SetExpr, ty), values(v)
    {
    }
    void            DoDump() const override;
    llvm::Value*    Address() override;
    llvm::Constant* MakeConstantSetArray();
    llvm::Value*    MakeConstantSet();
    static bool     classof(const ExprAST* e) { return e->getKind() == EK_SetExpr; }

private:
    std::vector<ExprAST*> values;
};

class VariableExprAST : public AddressableAST
{
public:
    VariableExprAST(const Location& w, const std::string& nm, Types::TypeDecl* ty)
        : AddressableAST(w, EK_VariableExpr, ty), name(nm), flags(VarDef::Flags::None)
    {
    }
    VariableExprAST(const Location& w, const NamedObject* obj)
        : AddressableAST{ w, EK_VariableExpr, obj->Type() }, name{ obj->Name() }
    {
	if (auto vd = llvm::dyn_cast<VarDef>(obj))
	{
	    flags = vd->GetFlags();
	}
    }
    void              DoDump() const override;
    const std::string Name() const override { return name; }
    llvm::Value*      Address() override;
    static bool       classof(const ExprAST* e) { return e->getKind() == EK_VariableExpr; }
    bool              IsProtected() { return (flags & VarDef::Flags::Protected) == VarDef::Flags::Protected; }

protected:
    std::string name;
    VarDef::Flags flags;
};

class ArrayExprAST : public AddressableAST
{
    friend class TypeCheckVisitor;

public:
    ArrayExprAST(const Location& w, ExprAST* v, const std::vector<ExprAST*>& inds,
                 const std::vector<Types::RangeBaseDecl*>& r, Types::TypeDecl* ty);
    void DoDump() const override;
    // Don't need CodeGen, just calculate address and use parent CodeGen
    llvm::Value* Address() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_ArrayExpr; }
    void         accept(ASTVisitor& v) override;

private:
    ExprAST*                       expr;
    std::vector<ExprAST*>          indices;
    std::vector<Types::RangeBaseDecl*> ranges;
    std::vector<size_t>            indexmul;
};

class DynArrayExprAST : public AddressableAST
{
    friend class TypeCheckVisitor;

public:
    DynArrayExprAST(const Location& w, ExprAST* v, ExprAST* index, Types::DynRangeDecl* r,
                    Types::TypeDecl* ty);
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_DynArrayExpr; }
    void         DoDump() const override;
    llvm::Value* Address() override;
    void         accept(ASTVisitor& v) override;

private:
    ExprAST*             expr;
    ExprAST*             index;
    Types::DynRangeDecl* range;
};

class PointerExprAST : public AddressableAST
{
public:
    PointerExprAST(const Location& w, ExprAST* p, Types::TypeDecl* ty)
        : AddressableAST(w, EK_PointerExpr, ty), pointer(p)
    {
    }
    void         DoDump() const override;
    llvm::Value* Address() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_PointerExpr; }
    void         accept(ASTVisitor& v) override;

private:
    ExprAST* pointer;
};

class FilePointerExprAST : public AddressableAST
{
public:
    FilePointerExprAST(const Location& w, ExprAST* p, Types::TypeDecl* ty)
        : AddressableAST(w, EK_FilePointerExpr, ty), pointer(p)
    {
    }
    void         DoDump() const override;
    llvm::Value* Address() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_FilePointerExpr; }
    void         accept(ASTVisitor& v) override;

private:
    ExprAST* pointer;
};

class FieldExprAST : public AddressableAST
{
public:
    FieldExprAST(const Location& w, ExprAST* base, int elem, Types::TypeDecl* ty)
        : AddressableAST(w, EK_FieldExpr, ty), expr(base), element(elem)
    {
    }
    void         DoDump() const override;
    llvm::Value* Address() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_FieldExpr; }
    void         accept(ASTVisitor& v) override;

private:
    ExprAST* expr;
    int      element;
};

class VariantFieldExprAST : public AddressableAST
{
public:
    VariantFieldExprAST(const Location& w, ExprAST* base, int elem, Types::TypeDecl* ty)
        : AddressableAST(w, EK_VariantFieldExpr, ty), expr(base), element(elem)
    {
    }
    void         DoDump() const override;
    llvm::Value* Address() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_VariantFieldExpr; }
    void         accept(ASTVisitor& v) override;

private:
    ExprAST* expr;
    int      element;
};

class BinaryExprAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    BinaryExprAST(Token op, ExprAST* l, ExprAST* r)
        : ExprAST(op.Loc(), EK_BinaryExpr), oper(op), lhs(l), rhs(r)
    {
    }
    void             DoDump() const override;
    llvm::Value*     CodeGen() override;
    static bool      classof(const ExprAST* e) { return e->getKind() == EK_BinaryExpr; }
    Types::TypeDecl* Type() const override;
    void             UpdateType(Types::TypeDecl* ty);
    void             accept(ASTVisitor& v) override
    {
	rhs->accept(v);
	lhs->accept(v);
	v.visit(this);
    }

private:
    llvm::Value* SetCodeGen();
    llvm::Value* InlineSetFunc(const std::string& name);
    llvm::Value* CallSetFunc(const std::string& name, bool resTyIsSet);
    llvm::Value* CallStrFunc(const std::string& name);
    llvm::Value* CallArrFunc(const std::string& name, size_t size);

private:
    Token    oper;
    ExprAST* lhs;
    ExprAST* rhs;
};

class UnaryExprAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    UnaryExprAST(const Location& w, Token op, ExprAST* r)
        : ExprAST(w, EK_UnaryExpr, r->Type()), oper(op), rhs(r){};
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_UnaryExpr; }
    void         UpdateType(Types::TypeDecl* ty);
    void         accept(ASTVisitor& v) override
    {
	rhs->accept(v);
	v.visit(this);
    }

private:
    Token    oper;
    ExprAST* rhs;
};

class RangeExprAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    RangeExprAST(const Location& w, ExprAST* l, ExprAST* h)
        : ExprAST(w, EK_RangeExpr, l->Type()), low(l), high(h)
    {
	ICE_IF(*l->Type() != *h->Type(), "Expect same type here");
    }
    void         DoDump() const override;
    llvm::Value* Low() { return low->CodeGen(); }
    llvm::Value* High() { return high->CodeGen(); }
    ExprAST*     LowExpr() { return low; }
    ExprAST*     HighExpr() { return high; }
    bool         IsConstant();
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_RangeExpr; }
    void         accept(ASTVisitor& v) override
    {
	low->accept(v);
	high->accept(v);
	v.visit(this);
    }

private:
    ExprAST* low;
    ExprAST* high;
};

class BlockAST : public ExprAST
{
public:
    BlockAST(const Location& w, const std::vector<ExprAST*>& block) : ExprAST(w, EK_Block), content(block) {}
    void                   DoDump() const override;
    llvm::Value*           CodeGen() override;
    bool                   IsEmpty() { return content.size() == 0; }
    static bool            classof(const ExprAST* e) { return e->getKind() == EK_Block; }
    std::vector<ExprAST*>& Content() { return content; }
    void                   accept(ASTVisitor& v) override;

private:
    std::vector<ExprAST*> content;
};

class AssignExprAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    AssignExprAST(const Location& w, ExprAST* l, ExprAST* r) : ExprAST(w, EK_AssignExpr), lhs(l), rhs(r) {}
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_AssignExpr; }
    void         accept(ASTVisitor& v) override
    {
	lhs->accept(v);
	v.visit(this);
	rhs->accept(v);
    }

private:
    llvm::Value* AssignStr();
    llvm::Value* AssignSet();
    ExprAST*     lhs;
    ExprAST*     rhs;
};

class FunctionAST;

class VarDeclAST : public ExprAST
{
public:
    VarDeclAST(const Location& w, const std::vector<VarDef>& v) : ExprAST(w, EK_VarDecl), vars(v), func(0) {}
    void                       DoDump() const override;
    llvm::Value*               CodeGen() override;
    void                       SetFunction(FunctionAST* f) { func = f; }
    FunctionAST*               Function() { return func; }
    static bool                classof(const ExprAST* e) { return e->getKind() == EK_VarDecl; }
    const std::vector<VarDef>& Vars() { return vars; }

private:
    llvm::Value* CodeGenGlobal(VarDef var);
    llvm::Value* CodeGenLocal(VarDef var);

    std::vector<VarDef> vars;
    FunctionAST*        func;
};

class PrototypeAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    PrototypeAST(const Location& w, const std::string& nm, const std::vector<VarDef>& ar,
                 Types::TypeDecl* resTy, const std::string& resNm, Types::ClassDecl* obj)
        : ExprAST(w, EK_Prototype, resTy)
        , name(nm)
        , resname(resNm)
        , args(ar)
        , function(0)
        , baseobj(obj)
        , isForward(false)
        , hasSelf(false)
        , llvmFunc(0)
    {
	ICE_IF(!resTy, "Type must not be null!");
    }
    void                       DoDump() const override;
    llvm::Function*            Create(const std::string& namePrefix);
    llvm::Function*            LlvmFunction() const { return llvmFunc; }
    void                       CreateArgumentAlloca();
    const std::string&         Name() const { return name; }
    const std::string&         ResName() const { return resname; }
    const std::vector<VarDef>& Args() const { return args; }
    bool                       IsForward() const { return isForward; }
    bool                       HasSelf() const { return hasSelf; }
    void                       SetIsForward(bool v);
    void                       SetHasSelf(bool v) { hasSelf = v; }
    void                       SetFunction(FunctionAST* fun) { function = fun; }
    FunctionAST*               Function() const { return function; }
    void                       AddExtraArgsFirst(const std::vector<VarDef>& extra);
    Types::ClassDecl*          BaseObj() const { return baseobj; }
    void                       SetBaseObj(Types::ClassDecl* obj) { baseobj = obj; }
    bool                       operator==(const PrototypeAST& rhs) const;
    bool                       IsMatchWithoutClosure(const PrototypeAST* rhs) const;
    static bool                classof(const ExprAST* e) { return e->getKind() == EK_Prototype; }

private:
    std::string         name;
    std::string         resname;
    std::vector<VarDef> args;
    FunctionAST*        function;
    Types::ClassDecl*   baseobj;
    bool                isForward;
    bool                hasSelf;
    llvm::Function*     llvmFunc;
};

class FunctionAST : public ExprAST
{
public:
    FunctionAST(const Location& w, PrototypeAST* prot, const std::vector<VarDeclAST*>& v, BlockAST* b);
    void                DoDump() const override;
    llvm::Function*     CodeGen() override;
    llvm::Function*     CodeGen(const std::string& namePrefix);
    const PrototypeAST* Proto() const { return proto; }
    PrototypeAST*       Proto() { return proto; }
    void                AddSubFunctions(const std::vector<FunctionAST*>& subs) { subFunctions = subs; }
    void                SetParent(FunctionAST* p) { parent = p; }
    const FunctionAST*  Parent() const { return parent; }
    const std::vector<FunctionAST*> SubFunctions() const { return subFunctions; }
    void                    SetUsedVars(const std::set<VarDef>& usedvars) { usedVariables = usedvars; }
    const std::set<VarDef>& UsedVars() { return usedVariables; }
    Types::TypeDecl*        ClosureType();
    const std::string       ClosureName() { return "$$CLOSURE"; };
    static bool             classof(const ExprAST* e) { return e->getKind() == EK_Function; }
    void                    accept(ASTVisitor& v) override;
    void                    EndLoc(const Location& loc) { endLoc = loc; }

private:
    PrototypeAST*             proto;
    std::vector<VarDeclAST*>  varDecls;
    BlockAST*                 body;
    std::vector<FunctionAST*> subFunctions;
    std::set<VarDef>          usedVariables;
    FunctionAST*              parent;
    Types::TypeDecl*          closureType;
    Location                  endLoc;
};

class FunctionExprAST : public ExprAST
{
public:
    FunctionExprAST(const Location& w, const PrototypeAST* p)
        : ExprAST(w, EK_FunctionExpr, p->Type()), proto(p)
    {
    }

    void                DoDump() const override;
    llvm::Value*        CodeGen() override;
    const PrototypeAST* Proto() const { return proto; }
    static bool         classof(const ExprAST* e) { return e->getKind() == EK_FunctionExpr; }

private:
    const PrototypeAST* proto;
};

class CallExprAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    CallExprAST(const Location& w, ExprAST* c, const std::vector<ExprAST*>& a, const PrototypeAST* p)
        : ExprAST(w, EK_CallExpr, p->Type()), proto(p), callee(c), args(a)
    {
	ICE_IF(!proto, "Should have prototype!");
    }
    void                   DoDump() const override;
    llvm::Value*           CodeGen() override;
    static bool            classof(const ExprAST* e) { return e->getKind() == EK_CallExpr; }
    const PrototypeAST*    Proto() { return proto; }
    ExprAST*               Callee() const { return callee; }
    std::vector<ExprAST*>& Args() { return args; }
    void                   accept(ASTVisitor& v) override;

private:
    const PrototypeAST*   proto;
    ExprAST*              callee;
    std::vector<ExprAST*> args;
};

// Builtin function call
class BuiltinExprAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    BuiltinExprAST(const Location& w, Builtin::FunctionBase* b)
        : ExprAST(w, EK_BuiltinExpr, b->Type()), bif(b)
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_BuiltinExpr; }
    void         accept(ASTVisitor& v) override;

private:
    Builtin::FunctionBase* bif;
};

class IfExprAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    IfExprAST(const Location& w, ExprAST* c, ExprAST* t, ExprAST* e)
        : ExprAST(w, EK_IfExpr), cond(c), then(t), other(e)
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_IfExpr; }
    void         accept(ASTVisitor& v) override;

private:
    ExprAST* cond;
    ExprAST* then;
    ExprAST* other;
};

class ForExprAST : public ExprAST
{
public:
    friend class TypeCheckVisitor;
    ForExprAST(const Location& w, VariableExprAST* v, ExprAST* s, ExprAST* e, bool down, ExprAST* b)
        : ExprAST(w, EK_ForExpr), variable(v), start(s), stepDown(down), end(e), body(b)
    {
    }
    // for-in-set
    ForExprAST(const Location& w, VariableExprAST* v, ExprAST* s, ExprAST* b)
        : ExprAST(w, EK_ForExpr), variable(v), start(s), stepDown(false), end(nullptr), body(b)
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_ForExpr; }
    void         accept(ASTVisitor& v) override;

private:
    llvm::Value* ForInGen();

private:
    VariableExprAST* variable;
    ExprAST*         start;
    bool             stepDown; // true for "downto"
    ExprAST*         end;
    ExprAST*         body;
};

class WhileExprAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    WhileExprAST(const Location& w, ExprAST* c, ExprAST* b) : ExprAST(w, EK_WhileExpr), cond(c), body(b) {}
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_WhileExpr; }
    void         accept(ASTVisitor& v) override;

private:
    ExprAST* cond;
    ExprAST* body;
};

class RepeatExprAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    RepeatExprAST(const Location& w, ExprAST* c, ExprAST* b) : ExprAST(w, EK_RepeatExpr), cond(c), body(b)
    {
	ICE_IF(!body || !cond, "Expect body");
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_RepeatExpr; }
    void         accept(ASTVisitor& v) override;

private:
    ExprAST* cond;
    ExprAST* body;
};

class WriteAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    enum class WriteKind
    {
	Write,
	WriteLn,
	WriteStr
    };
    struct WriteArg
    {
	WriteArg() : expr(0), width(0), precision(0) {}
	ExprAST* expr;
	ExprAST* width;
	ExprAST* precision;
    };

    WriteAST(const Location& w, AddressableAST* dst, const std::vector<WriteArg>& a, WriteKind knd)
        : ExprAST(w, EK_Write), dest(dst), args(a), kind(knd)
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_Write; }
    void         accept(ASTVisitor& v) override;

private:
    AddressableAST*       dest;
    std::vector<WriteArg> args;
    WriteKind             kind;
};

class ReadAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    enum class ReadKind
    {
	Read,
	ReadLn,
	ReadStr
    };

    ReadAST(const Location& w, AddressableAST* sc, const std::vector<ExprAST*>& a, ReadKind knd)
        : ExprAST(w, EK_Read), src(sc), args(a), kind(knd)
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_Read; }
    void         accept(ASTVisitor& v) override;

private:
    AddressableAST*       src;
    std::vector<ExprAST*> args;
    ReadKind              kind;
};

class LabelExprAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    LabelExprAST(const Location& w, const std::vector<std::pair<int, int>>& lab, ExprAST* st)
        : ExprAST(w, EK_LabelExpr), labelValues(lab), stmt(st)
    {
    }
    void                                    DoDump() const override;
    llvm::Value*                            CodeGen() override;
    llvm::Value*                            CodeGen(llvm::BasicBlock* caseBB, llvm::BasicBlock* afterBB);
    static bool                             classof(const ExprAST* e) { return e->getKind() == EK_LabelExpr; }
    void                                    accept(ASTVisitor& v) override;
    const std::vector<std::pair<int, int>>& LabelValues() { return labelValues; }

private:
    std::vector<std::pair<int, int>> labelValues;
    ExprAST*                         stmt;
};

class CaseExprAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    CaseExprAST(const Location& w, ExprAST* e, const std::vector<LabelExprAST*>& lab, ExprAST* other)
        : ExprAST(w, EK_CaseExpr), expr(e), labels(lab), otherwise(other)
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_CaseExpr; }
    void         accept(ASTVisitor& v) override;

private:
    ExprAST*                   expr;
    std::vector<LabelExprAST*> labels;
    ExprAST*                   otherwise;
};

class WithExprAST : public ExprAST
{
public:
    WithExprAST(const Location& w, ExprAST* b) : ExprAST(w, EK_WithExpr), body(b){};
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_WithExpr; }
    void         accept(ASTVisitor& v) override
    {
	body->accept(v);
	v.visit(this);
    }

private:
    ExprAST* body;
};

class RangeReduceAST : public ExprAST
{
public:
    RangeReduceAST(ExprAST* e, Types::RangeBaseDecl* r)
        : ExprAST(e->Loc(), EK_RangeReduceExpr, e->Type()), expr(e), range(r)
    {
    }
    RangeReduceAST(ExprKind k, ExprAST* e, Types::RangeBaseDecl* r)
        : ExprAST(e->Loc(), k, e->Type()), expr(e), range(r)
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    void         accept(ASTVisitor& v) override
    {
	expr->accept(v);
	v.visit(this);
    }
    static bool classof(const ExprAST* e)
    {
	return (e->getKind() == EK_RangeReduceExpr) || (e->getKind() == EK_RangeCheckExpr);
    }

protected:
    ExprAST*              expr;
    Types::RangeBaseDecl* range;
};

class RangeCheckAST : public RangeReduceAST
{
public:
    RangeCheckAST(ExprAST* e, Types::RangeBaseDecl* r) : RangeReduceAST(EK_RangeCheckExpr, e, r) {}
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_RangeCheckExpr; }
};

class TypeCastAST : public AddressableAST
{
public:
    TypeCastAST(const Location& w, ExprAST* e, const Types::TypeDecl* t)
        : AddressableAST(w, EK_TypeCastExpr, const_cast<Types::TypeDecl*>(t)), expr(e){};
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    llvm::Value* Address() override;
    ExprAST*     Expr() { return expr; }
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_TypeCastExpr; }
    void         accept(ASTVisitor& v) override
    {
	expr->accept(v);
	v.visit(this);
    }

private:
    ExprAST* expr;
};

class SizeOfExprAST : public ExprAST
{
public:
    SizeOfExprAST(const Location& w, Types::TypeDecl* t)
        : ExprAST{ w, EK_SizeOfExpr, Types::Get<Types::IntegerDecl>() }, typeToSize{ t }
    {
    }
    void             DoDump() const override;
    llvm::Value*     CodeGen() override;
    static bool      classof(const ExprAST* e) { return e->getKind() == EK_SizeOfExpr; }

private:
    const Types::TypeDecl* typeToSize;
};

class VTableAST : public ExprAST
{
public:
    VTableAST(const Location& w, Types::ClassDecl* cd)
        : ExprAST(w, EK_VTableExpr, cd), classDecl(cd), vtable(0)
    {
    }
    void                         DoDump() const override;
    llvm::Value*                 CodeGen() override;
    std::vector<llvm::Constant*> GetInitializer();
    void                         Fixup();
    static bool                  classof(const ExprAST* e) { return e->getKind() == EK_VTableExpr; }

private:
    Types::ClassDecl*     classDecl;
    llvm::GlobalVariable* vtable;
};

class VirtFunctionAST : public AddressableAST
{
public:
    VirtFunctionAST(const Location& w, ExprAST* slf, int idx, Types::TypeDecl* ty);
    void         DoDump() const override;
    llvm::Value* Address() override;
    int          Index() const { return index; }
    ExprAST*     Self() { return self; }
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_VirtFunction; }

private:
    int      index;
    ExprAST* self;
};

class GotoAST : public ExprAST
{
public:
    GotoAST(const Location& w, int d) : ExprAST(w, EK_Goto), dest(d) {}
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_Goto; }

private:
    int dest;
};

class UnitAST : public ExprAST
{
public:
    UnitAST(const Location& w, const std::vector<ExprAST*>& c, FunctionAST* init, InterfaceList iList)
        : ExprAST(w, EK_Unit), initFunc(init), code(c), interfaceList(iList){};
    void                 DoDump() const override;
    llvm::Value*         CodeGen() override;
    static bool          classof(const ExprAST* e) { return e->getKind() == EK_Unit; }
    void                 accept(ASTVisitor& v) override;
    const InterfaceList& Interface() { return interfaceList; }

private:
    FunctionAST*          initFunc;
    std::vector<ExprAST*> code;
    InterfaceList         interfaceList;
};

class ClosureAST : public ExprAST
{
public:
    ClosureAST(const Location& w, Types::TypeDecl* ty, const std::vector<VariableExprAST*>& vf)
        : ExprAST(w, EK_Closure, ty), content(vf)
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_Closure; }

private:
    std::vector<VariableExprAST*> content;
};

class TrampolineAST : public FunctionExprAST
{
public:
    TrampolineAST(const Location& w, FunctionExprAST* fn, ClosureAST* c, Types::FuncPtrDecl* fnPtrTy)
        : FunctionExprAST(w, fn->Proto()), func(fn), closure(c), funcPtrTy(fnPtrTy)
    {
    }
    void         DoDump() const override;
    llvm::Value* CodeGen() override;
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_Trampoline; }
    void         accept(ASTVisitor& v) override;

private:
    FunctionExprAST*    func;
    ClosureAST*         closure;
    Types::FuncPtrDecl* funcPtrTy;
};

class InitValueAST : public ExprAST
{
public:
    InitValueAST(const Location& w, Types::TypeDecl* ty, const std::vector<ExprAST*>& v)
        : ExprAST(w, EK_InitValue, ty), values(v)
    {
    }
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_InitValue; }
    llvm::Value* CodeGen() override;
    void         DoDump() const override;

private:
    std::vector<ExprAST*> values;
};

class InitArrayAST : public ExprAST
{
    friend class TypeCheckVisitor;

public:
    InitArrayAST(const Location& w, Types::TypeDecl* ty, const std::vector<ArrayInit>& v)
        : ExprAST(w, EK_InitArray, ty), values(v)
    {
    }
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_InitArray; }
    llvm::Value* CodeGen() override;
    void         DoDump() const override;

private:
    std::vector<ArrayInit> values;
};

class InitRecordAST : public ExprAST
{
public:
    InitRecordAST(const Location& w, Types::TypeDecl* ty, const std::vector<RecordInit>& v)
        : ExprAST(w, EK_InitRecord, ty), values(v)
    {
    }
    static bool  classof(const ExprAST* e) { return e->getKind() == EK_InitRecord; }
    llvm::Value* CodeGen() override;
    void         DoDump() const override;

private:
    std::vector<RecordInit> values;
};

class ArraySliceAST : public AddressableAST
{
public:
    ArraySliceAST(const Location& w, ExprAST* e, RangeExprAST* r, Types::ArrayDecl* ty)
        : AddressableAST(w, EK_ArraySlice, ty), expr(e), range(r)
    {
	Types::TypeDecl* t = nullptr;
	if (range->IsConstant())
	{
	    auto r = new Types::RangeDecl(
	        new Types::Range(llvm::dyn_cast<IntegerExprAST>(range->LowExpr())->Int(),
	                         llvm::dyn_cast<IntegerExprAST>(range->HighExpr())->Int()),
	        range->Type());
	    t = new Types::ArrayDecl(ty->SubType(), { r });
	}
	else
	{
	    auto r = new Types::DynRangeDecl("", "", Types::Get<Types::IntegerDecl>());
	    t = new Types::DynArrayDecl(ty->SubType(), r);
	}
	origType = ty;
	SetType(t);
    }

    static bool  classof(const ExprAST* e) { return e->getKind() == EK_ArraySlice; }
    llvm::Value* Address() override;
    void         DoDump() const override;
    void         accept(ASTVisitor& v) override;
    llvm::Value* Size();

private:
    ExprAST*         expr;
    RangeExprAST*    range;
    Types::TypeDecl* origType;
};

// Useful global functions
llvm::Constant*      MakeIntegerConstant(int val);
llvm::Constant*      MakeBooleanConstant(int val);
llvm::Constant*      MakeConstant(uint64_t val, Types::TypeDecl* ty);
llvm::Value*         MakeAddressable(ExprAST* e);
llvm::Value*         MakeStringFromExpr(ExprAST* e, Types::TypeDecl* ty);
void                 BackPatch();
llvm::FunctionCallee GetFunction(llvm::Type* resTy, const std::vector<llvm::Type*>& args,
                                 const std::string& name);
ExprAST*             Recast(ExprAST* a, const Types::TypeDecl* ty);
size_t               AlignOfType(llvm::Type* ty);
llvm::AllocaInst*    CreateTempAlloca(Types::TypeDecl* ty);
llvm::Value*         MakeStrCompare(Token::TokenType oper, llvm::Value* v);
llvm::Value*         CallStrFunc(const std::string& name, ExprAST* lhs, ExprAST* rhs, Types::TypeDecl* resTy,
                                 const std::string& twine);

#endif
