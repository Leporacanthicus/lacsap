#ifndef CALLGRAPH_H

#include <vector>

class ExprAST;
class FunctionAST;
class VarDeclAST;

class CallGraphVisitor
{
public:
    virtual ~CallGraphVisitor() {}
    virtual void Caller(FunctionAST* f) {}
    virtual void Process(FunctionAST* f) {}
    virtual void VarDecl(VarDeclAST* v) {}
};

class CallGraphPrinter : public CallGraphVisitor
{
public:
    virtual void Process(FunctionAST* f);
    virtual void Caller(FunctionAST* f);
};

void CallGraph(ExprAST* ast, CallGraphVisitor& visitor);
void BuildClosures(ExprAST* ast);
void AddClosureArg(FunctionAST* fn, std::vector<ExprAST*>& args);

#endif
