#include "callgraph.h"
#include "visitor.h"

class CFGVisitor: public ASTVisitor
{
public:
    CFGVisitor(CallGraphVisitor& cfgv) : visitor(cfgv) {}

    void visit(ExprAST* a) override
    {
	if (FunctionAST* f = llvm::dyn_cast<FunctionAST>(a))
	{
	    visitor.Caller(f);
	}

	if (CallExprAST* c = llvm::dyn_cast<CallExprAST>(a))
	{
	    if (FunctionExprAST* fe = llvm::dyn_cast<FunctionExprAST>(c->Callee()))
	    {
		if (FunctionAST* fn = fe->Proto()->Function())
		{
		    visitor.Process(fn);
		}
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

static void PrintFunctionName(const FunctionAST* f, int level)
{
    if (level == 0)
    {
	std::cout << "  ";
    }
    if (f->Parent())
    {
	PrintFunctionName(f->Parent(), level+1);
	std::cout << ":";
    }
    std::cout << f->Proto()->Name();
    if (level == 0)
    {
	std::cout << std::endl;
    }
}

void CallGraphPrinter::Process(FunctionAST* f)
{
    PrintFunctionName(f, 0);
}

void CallGraphPrinter::Caller(FunctionAST* f)
{
    std::cout << "Called from: " << f->Proto()->Name() << std::endl;
}

