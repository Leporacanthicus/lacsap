#ifndef EXPR_H
#define EXPR_H

#include "token.h"
#include "namedobject.h"
#include "types.h"
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/PassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <string>
#include <vector>
#include <iostream>

extern llvm::FunctionPassManager* fpm;
extern llvm::Module* theModule;

class ExprAST
{
public:
    ExprAST() {}
    virtual ~ExprAST() {}
    void Dump(std::ostream& out) const;
    void Dump() const;
    virtual void DoDump(std::ostream& out) const
    { 
	out << "Empty node";
    }
    std::string ToString();
    virtual llvm::Value* CodeGen() = 0;
};

class RealExprAST : public ExprAST
{
public:
    RealExprAST(double v) 
	: val(v) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    double val;
};

class IntegerExprAST : public ExprAST
{
public:
    IntegerExprAST(int v) 
	: val(v) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    int val;
};

class CharExprAST : public ExprAST
{
public:
    CharExprAST(char v) 
	: val(v) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    char val;
};

class StringExprAST : public ExprAST
{
public:
    StringExprAST(const std::string &v) 
	: val(v) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    std::string val;
};

class VariableExprAST : public ExprAST
{
public:
    VariableExprAST(const std::string& nm, Types::TypeDecl* ty) 
	: name(nm), type(ty) {}
    VariableExprAST(const VariableExprAST* v, Types::TypeDecl* ty) 
	: name(v->name), type(ty) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
    const std::string& Name() const { return name; }
    virtual llvm::Value* Address();
    Types::TypeDecl *Type() const { return type; }
protected:
    std::string name;
    Types::TypeDecl* type;
};

class ArrayExprAST : public VariableExprAST
{
public:
    ArrayExprAST(VariableExprAST *v,
		 const std::vector<ExprAST*>& inds, 
		 const std::vector<Types::Range*>& r, 
		 Types::TypeDecl* ty)
	: VariableExprAST(v, ty), expr(v), indices(inds), ranges(r)
    {
	size_t mul = 1;
	for(auto j = ranges.end()-1; j >= ranges.begin(); j--)
	{
	    indexmul.push_back(mul);
	    mul *= (*j)->Size();
	}
	std::reverse(indexmul.begin(), indexmul.end());
    }
    virtual void DoDump(std::ostream& out) const;
    /* Don't need CodeGen, just calculate address and use parent CodeGen */
    virtual llvm::Value* Address();
private:
    VariableExprAST* expr;
    std::vector<ExprAST*> indices;
    std::vector<Types::Range*> ranges;
    std::vector<size_t> indexmul;
};

class PointerExprAST : public VariableExprAST
{
public:
    PointerExprAST(VariableExprAST *p, Types::TypeDecl* ty)
	: VariableExprAST(p, ty), pointer(p) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
    virtual llvm::Value* Address();
private:
    ExprAST* pointer;
};

class FieldExprAST : public VariableExprAST
{
public:
    FieldExprAST(VariableExprAST* base, int elem, Types::TypeDecl* ty)
	: VariableExprAST(base, ty), expr(base), element(elem) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* Address();
private:
    VariableExprAST* expr;
    int element;
};

class FunctionExprAST : public VariableExprAST
{
public:
    FunctionExprAST(const std::string& nm, Types::TypeDecl* ty)
	: VariableExprAST(nm, ty) { }

    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* Address();
    virtual llvm::Value* CodeGen();
};

class BinaryExprAST : public ExprAST
{
public:
    BinaryExprAST(Token op, ExprAST* l, ExprAST* r)
	: oper(op), lhs(l), rhs(r) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    Token oper;
    ExprAST* lhs, *rhs;
};

class UnaryExprAST : public ExprAST
{
public:
    UnaryExprAST(Token op, ExprAST* r)
	: oper(op), rhs(r) {};
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    Token oper;
    ExprAST* rhs;
};

class BlockAST : public ExprAST
{
public:
    BlockAST(std::vector<ExprAST*> block) : 
	content(block) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
    bool IsEmpty() { return content.size() == 0; }
private:
    std::vector<ExprAST*> content;
};

class AssignExprAST : public ExprAST
{
public:
    AssignExprAST(ExprAST* l, ExprAST* r)
	: lhs(l), rhs(r) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    ExprAST* lhs, *rhs;
};

class VarDeclAST : public ExprAST
{
public:
    VarDeclAST(std::vector<VarDef> v)
	: vars(v), func(0) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
    void SetFunction(llvm::Function* f) { func = f; }
private:
    std::vector<VarDef> vars;
    llvm::Function* func;
};

class FunctionAST;

class PrototypeAST : public ExprAST
{
public:
    PrototypeAST(const std::string& nm, const std::vector<VarDef>& ar) 
	: name(nm), args(ar), isForward(false), function(0)
    { 
	resultType = new Types::TypeDecl(Types::Void); 
    }
    PrototypeAST(const std::string& nm, const std::vector<VarDef>& ar, Types::TypeDecl* resTy) 
	: name(nm), args(ar), resultType(resTy), isForward(false), function(0)
    {
	assert(resTy && "Type must not be null!");
    }
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Function* CodeGen();
    virtual llvm::Function* CodeGen(const std::string& namePrefix);
    void CreateArgumentAlloca(llvm::Function* fn);
    Types::TypeDecl* ResultType() const { return resultType; }
    std::string Name() const { return name; }
    const std::vector<VarDef>& Args() const { return args; }
    bool IsForward() { return isForward; }
    void SetIsForward(bool v) { isForward = v; }
    void SetFunction(FunctionAST* fun) { function = fun; }
    FunctionAST* Function() const { return function; }
    void AddExtraArgs(const std::vector<VarDef>& extra);
private:
    std::string         name;
    std::vector<VarDef> args;
    Types::TypeDecl*    resultType;
    bool                isForward;
    FunctionAST*        function;
};

class FunctionAST : public ExprAST
{
public:
    FunctionAST(PrototypeAST *prot, VarDeclAST* v, BlockAST* b) 
	: proto(prot), varDecls(v), body(b)
    { 
	assert((proto->IsForward() || body) && "Function should have body"); 
	if (!proto->IsForward())
	{
	    proto->SetFunction(this);
	}
    }
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Function* CodeGen();
    llvm::Function* CodeGen(const std::string& namePrefix);
    const PrototypeAST* Proto() const { return proto; }
    void AddSubFunctions(const std::vector<FunctionAST *>& subs) { subFunctions = subs; }
    void SetParent(FunctionAST* p) { parent = p; }
    void SetUsedVars(const std::vector<NamedObject*>& varsUsed, 
		     const std::vector<NamedObject*>& localVars);
    const std::vector<VarDef>& UsedVars() { return usedVariables; }
private:
    PrototypeAST*              proto;
    VarDeclAST*                varDecls;
    BlockAST*                  body;
    std::vector<FunctionAST*>  subFunctions;
    std::vector<VarDef>        usedVariables;
    FunctionAST*               parent;
};

class CallExprAST : public ExprAST
{
public:
    CallExprAST(ExprAST *c, std::vector<ExprAST*> a, const PrototypeAST* p)
	: proto(p), callee(c), args(a) 
    {
	assert(proto && "Should have prototype!");
    }
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    const PrototypeAST*   proto;
    ExprAST*              callee;
    std::vector<ExprAST*> args;
};

// Builtin function call
class BuiltinExprAST : public ExprAST
{
public:
    BuiltinExprAST(const std::string& nm, std::vector<ExprAST*> a)
	: name(nm), args(a) 
    {
    }
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    std::string           name;
    std::vector<ExprAST*> args;
};

class IfExprAST : public ExprAST
{
public:
    IfExprAST(ExprAST* c, ExprAST* t, ExprAST* e) 
	: cond(c), then(t), other(e) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    ExprAST* cond;
    ExprAST* then;
    ExprAST* other;
};

class ForExprAST : public ExprAST
{
public:
    ForExprAST(const std::string& var, ExprAST* s, ExprAST* e, bool down, ExprAST* b)
	: varName(var), start(s), stepDown(down), end(e), body(b) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    std::string varName;
    ExprAST* start;
    bool     stepDown;   // true for "downto" 
    ExprAST* end;
    ExprAST* body;
};

class WhileExprAST : public ExprAST
{
public:
    WhileExprAST(ExprAST* c, ExprAST* b)
	: cond(c), body(b) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    ExprAST* cond;
    ExprAST* body;
};

class RepeatExprAST : public ExprAST
{
public:
    RepeatExprAST(ExprAST* c, ExprAST* b)
	: cond(c), body(b) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    ExprAST* cond;
    ExprAST* body;
};

class WriteAST : public ExprAST
{
public:
    struct WriteArg
    {
	WriteArg() 
	    : expr(0), width(0), precision(0)  {}
	ExprAST* expr;
	ExprAST* width;
	ExprAST* precision;
    };

    WriteAST(VariableExprAST* f, const std::vector<WriteArg> &a, bool isLn)
	: file(f), args(a), isWriteln(isLn) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    VariableExprAST*      file;
    std::vector<WriteArg> args;
    bool                  isWriteln;
};

class ReadAST : public ExprAST
{
public:
    ReadAST(VariableExprAST* fi, const std::vector<ExprAST*> &a, bool isLn)
	: file(fi), args(a), isReadln(isLn) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    VariableExprAST*      file;
    std::vector<ExprAST*> args;
    bool                  isReadln;
};


class LabelExprAST : public ExprAST
{
public:
    LabelExprAST(const std::vector<int>& lab, ExprAST* st)
	: labelValues(lab),stmt(st) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen() { assert(0); return 0; }
    llvm::Value* CodeGen(llvm::SwitchInst* inst, llvm::BasicBlock* afterBB, llvm::Type* ty);
private:
    std::vector<int> labelValues;
    ExprAST*         stmt;
};

class CaseExprAST : public ExprAST
{
public:
    CaseExprAST(ExprAST* e, const std::vector<LabelExprAST*>& lab)
	: expr(e), labels(lab) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();

private:
    ExprAST* expr;
    std::vector<LabelExprAST*> labels;
};

/* Useful global functions */
llvm::Value* MakeIntegerConstant(int val);
llvm::Value* MakeConstant(int val, llvm::Type* ty);
llvm::Value* ErrorV(const std::string& msg);
llvm::Value* FileOrNull(VariableExprAST* file);

#endif
