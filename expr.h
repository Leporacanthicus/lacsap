#ifndef EXPR_H
#define EXPR_H

#include "token.h"
#include "namedobject.h"
#include "types.h"
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/PassManager.h>
#include <llvm/IR/Module.h>
#include <string>
#include <vector>
#include <iostream>

extern llvm::FunctionPassManager* fpm;
extern llvm::Module* theModule;

class ExprAST
{
public:
    ExprAST() 
	: next(0) {}
    virtual ~ExprAST() {}
    ExprAST* SetNext(ExprAST* n) { next = n; return next; };
    ExprAST* Next() { return next; }
    void Dump(std::ostream& out) const;
    void Dump() const;
    virtual void DoDump(std::ostream& out) const
    { 
	out << "Empty node";
    }
    std::string ToString();
    virtual llvm::Value* CodeGen() = 0;
private:
    ExprAST* next;
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
    VariableExprAST(const std::string& nm) 
	: name(nm) {}
    VariableExprAST(const VariableExprAST* v) 
	: name(v->name) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
    const std::string& Name() const { return name; }
    virtual llvm::Value* Address();
protected:
    std::string name;
};

class ArrayExprAST : public VariableExprAST
{
public:
    ArrayExprAST(VariableExprAST *v,
		 const std::vector<ExprAST*>& inds, 
		 const std::vector<Types::Range*>& r)
	: VariableExprAST(v), expr(v), indices(inds), ranges(r)
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
    PointerExprAST(VariableExprAST *p)
	: VariableExprAST(p), pointer(p) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
    virtual llvm::Value* Address();
private:
    ExprAST* pointer;
};

class FieldExprAST : public VariableExprAST
{
public:
    FieldExprAST(VariableExprAST* base, int elem)
	: VariableExprAST(base), expr(base), element(elem) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* Address();
private:
    VariableExprAST* expr;
    int element;
};

class FunctionExprAST : public VariableExprAST
{
public:
    FunctionExprAST(const std::string& nm)
	: VariableExprAST(nm) { }

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
    BlockAST(ExprAST* blockContent) : 
	content(blockContent) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
    bool IsEmpty() { return content == NULL; }
private:
    ExprAST* content;
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

class PrototypeAST : public ExprAST
{
public:
    PrototypeAST(const std::string& nm, const std::vector<VarDef>& ar) 
	: name(nm), args(ar), isForward(false)
    { 
	resultType = new Types::TypeDecl(Types::Void); 
    }
    PrototypeAST(const std::string& nm, const std::vector<VarDef>& ar, Types::TypeDecl* ty) 
	: name(nm), args(ar), resultType(ty), isForward(false)
    {
	assert(ty && "Type must not be null!");
    }
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Function* CodeGen();
    void CreateArgumentAlloca(llvm::Function* fn);
    Types::TypeDecl* ResultType() const { return resultType; }
    std::string Name() const { return name; }
    const std::vector<VarDef>& Args() const { return args; }
    bool IsForward() { return isForward; }
    void SetIsForward(bool v) { isForward = v; }
private:
    std::string         name;
    std::vector<VarDef> args;
    Types::TypeDecl*    resultType;
    bool                isForward;
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

class FunctionAST : public ExprAST
{
public:
    FunctionAST(PrototypeAST *prot, VarDeclAST* v, BlockAST* b) 
	: proto(prot), varDecls(v), body(b)
    { 
	assert((proto->IsForward() || body) && "Function should have body"); 
    }
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Function* CodeGen();
    const PrototypeAST* Proto() const { return proto; }
private:
    PrototypeAST *proto;
    VarDeclAST   *varDecls;
    BlockAST     *body;
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

    WriteAST(const std::vector<WriteArg> &a, bool isLn)
	: args(a), isWriteln(isLn) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    // TODO: Add support for file type. 
    std::vector<WriteArg> args;
    bool isWriteln;
};

class ReadAST : public ExprAST
{
public:
    ReadAST(const std::vector<ExprAST*> &a, bool isLn)
	: args(a), isReadln(isLn) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    // TODO: Add support for file type. 
    std::vector<ExprAST*> args;
    bool isReadln;
};

/* Useful global functions */
llvm::Value* MakeIntegerConstant(int val);
llvm::Value* MakeConstant(int val, llvm::Type* ty);
llvm::Value *ErrorV(const std::string& msg);

#endif
