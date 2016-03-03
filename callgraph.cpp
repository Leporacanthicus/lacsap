#include "callgraph.h"
#include "visitor.h"

class CFGVisitor: public ASTVisitor
{
public:
    CFGVisitor(CallGraphVisitor& cfgv) : visitor(cfgv) {}

    void visit(ExprAST* a) override
    {
	if (CallExprAST* c = llvm::dyn_cast<CallExprAST>(a))
	{
	    if (FunctionExprAST* fe = llvm::dyn_cast<FunctionAST>(c->Callee()))
	    if (FunctionAST* fn = f->)
	    {
		visitor.Process(fn);
	    }
	}
    }
private:
    CallGraphVisitor& visitor;
};

void CallGraph(ExprAST* ast, CallGraphVisitor& visitor)
{
    CFGVisitor v(visitor);
    ast->accept(v);
}
