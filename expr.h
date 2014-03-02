#ifndef EXPR_H
#define EXPR_H

#include "token.h"
#include "variables.h"
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
    void Dump(std::ostream& out) const
    { 
	out << "Node=" << reinterpret_cast<const void *>(this) << ": "; 
	DoDump(out); 
	out << std::endl;
    }
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
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
    const std::string& Name() const { return name; }
private:
    std::string name;
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



class CallExprAST : public ExprAST
{
public:
    CallExprAST(const std::string& c, std::vector<ExprAST*> a)
	: callee(c), args(a) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    std::string callee;
    std::vector<ExprAST*> args;
};


class BlockAST: public ExprAST
{
public:
    BlockAST(ExprAST* blockContent) : 
	content(blockContent) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Value* CodeGen();
private:
    ExprAST* content;
};

class PrototypeAST : public ExprAST
{
public:
    PrototypeAST(const std::string& nm, const std::vector<VarDef>& ar) : 
	name(nm), args(ar) { resultType="void"; }
    PrototypeAST(const std::string& nm, const std::vector<VarDef>& ar, const std::string &ty) 
	: name(nm), args(ar), resultType(ty) {}
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Function* CodeGen();
    void CreateArgumentAlloca(llvm::Function* fn);
    std::string ResultType() const { return resultType; }
    std::string Name() const { return name; }
    const std::vector<VarDef>& Args() { return args; }

private:
    std::string name;
    std::vector<VarDef> args;
    std::string resultType;
};

class VarDeclAST: public ExprAST
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
    FunctionAST(PrototypeAST *prot, VarDeclAST* v, ExprAST* b) 
	: proto(prot), varDecls(v), body(b) { assert(body && "Function should have body"); }
    virtual void DoDump(std::ostream& out) const;
    virtual llvm::Function* CodeGen();
private:
    PrototypeAST *proto;
    VarDeclAST   *varDecls;
    ExprAST      *body;
};

class IfExprAST: public ExprAST
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

class ForExprAST: public ExprAST
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


class WriteAST: public ExprAST
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

class ReadAST: public ExprAST
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
llvm::Value *ErrorV(const std::string& msg);

#endif
