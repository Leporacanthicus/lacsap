#include "astvisitor.h"
#include "expr.h"
#include "options.h"
#include <iostream>

void UpdateCallVisitor::visit(ExprAST* expr)
{
    if (verbosity > 1)
    {
	expr->dump();
    }
    CallExprAST* call = llvm::dyn_cast<CallExprAST>(expr);
    if(call)
    {
	if (call->Proto()->Name() == proto->Name()
	    && call->Args().size() != proto->Args().size())
	{
	    if (verbosity)
	    {
		std::cerr << "Adding arguments for function" << std::endl;
	    }
	    auto& args = call->Args();
	    for(auto u : proto->Function()->UsedVars())
	    {
		args.push_back(new VariableExprAST(u.Name(), u.Type()));
	    }
	}
    }
}
