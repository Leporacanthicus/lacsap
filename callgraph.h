#ifndef CALLGRAPH_H
#include "expr.h"

class CallGraphVisitor
{
public:
    virtual ~CallGraphVisitor() {}
    virtual void Process(FunctionAST* f) = 0;
};

void CallGraph(ExprAST *ast, CallGraphVisitor& visitor);

#endif
