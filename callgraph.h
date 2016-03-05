#ifndef CALLGRAPH_H
#include "expr.h"

class CallGraphVisitor
{
public:
    virtual ~CallGraphVisitor() {}
    virtual void Caller(FunctionAST* f) = 0;
    virtual void Process(FunctionAST* f) = 0;
};

class CallGraphPrinter : public CallGraphVisitor
{
public:
    virtual void Process(FunctionAST* f);
    virtual void Caller(FunctionAST* f);
};

void CallGraph(ExprAST *ast, CallGraphVisitor& visitor);
void BuildClosures(ExprAST* ast);

#endif
