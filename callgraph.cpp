#include "callgraph.h"
#include "visitor.h"
#include <map>
#include <set>

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

	if (VarDeclAST* v = llvm::dyn_cast<VarDeclAST>(a))
	{
	    visitor.VarDecl(v);
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

void AddClosureArg(FunctionAST* fn, std::vector<ExprAST*>& args)
{
    if (Types::TypeDecl* closureTy = fn->ClosureType())
    {
	std::vector<VariableExprAST*> vf;
	for(auto u : fn->UsedVars())
	{
	    vf.push_back(new VariableExprAST(fn->Loc(), u.Name(), u.Type()));
	}
	ClosureAST* closure = new ClosureAST(fn->Loc(), closureTy, vf);
	args.insert(args.begin(), closure);
    }
}

class UpdateCallVisitor : public ASTVisitor
{
public:
    UpdateCallVisitor(const PrototypeAST *p) : proto(p) {}
    virtual void visit(ExprAST* expr);
private:
    const PrototypeAST* proto;
};

/* This is used to update internal calls within the nest of functions, where we need
 * to pass variables from the outer scope to the inner scope
 */
void UpdateCallVisitor::visit(ExprAST* expr)
{
    if (CallExprAST* call = llvm::dyn_cast<CallExprAST>(expr))
    {
	if (call->Proto()->Name() == proto->Name()
	    && call->Args().size() != proto->Args().size())
	{
	    if (verbosity)
	    {
		std::cerr << "Adding arguments for function" << std::endl;
	    }
	    AddClosureArg(proto->Function(), call->Args());
	}
    }
}

using VarMap = std::map<std::string, VarDef>;
using VarSet = std::set<std::string>;
using CallSet = std::set<const FunctionAST*>;

class CallGraphClosureCollector : public CallGraphVisitor
{
public:
    void Caller(FunctionAST* f) override { CollectUseData(f); }
    void VarDecl(VarDeclAST* v) override { AddVarDecls(v->Vars(), v->Function()); }

    void AddVarDecls(const std::vector<VarDef>& vars, FunctionAST* f);
    void CollectUseData(FunctionAST* f);

    std::map<const FunctionAST*, VarSet> useMap;
    std::map<const FunctionAST*, VarMap> declMap;
    std::map<const FunctionAST*, CallSet> callMap;
};

class CollectUses : public ASTVisitor
{
public:
    void visit(ExprAST* a) override
    {
	if (VariableExprAST* v = llvm::dyn_cast<VariableExprAST>(a))
	{
	    if (TypeCastAST* tc = llvm::dyn_cast<TypeCastAST>(v))
	    {
		if (VariableExprAST* e = llvm::dyn_cast<VariableExprAST>(tc->Expr()))
		{
		    v = e;
		}
		else
		{
		    return;
		}
	    }
	    assert(v->Name() != "");
	    uses.insert(v->Name());
	}
	if (CallExprAST* c = llvm::dyn_cast<CallExprAST>(a))
	{
	    if (FunctionExprAST* fe = llvm::dyn_cast<FunctionExprAST>(c->Callee()))
	    {
		if (FunctionAST* fn = fe->Proto()->Function())
		{
		    calls.insert(fn);
		}
	    }
	}
    }
    CallSet calls;
    VarSet uses;
};

void CallGraphClosureCollector::AddVarDecls(const std::vector<VarDef>& vars, FunctionAST* f)
{
    VarMap &dm = declMap[f];
    for(auto d: vars)
    {
	assert(d.Name() != "");
	dm.insert(std::pair<std::string, VarDef>(d.Name(), d));
    }
}

void CallGraphClosureCollector::CollectUseData(FunctionAST* f)
{
    CollectUses collector;
    f->accept(collector);
    if (!llvm::isa<Types::VoidDecl>(f->Proto()->Type()))
    {
	AddVarDecls({ VarDef(f->Proto()->Name(), f->Proto()->Type()) }, f);
    }
    AddVarDecls(f->Proto()->Args(), f);
    useMap[f] = collector.uses;
    callMap[f] = collector.calls;
}

void RemoveFromUses(VarSet& uses, const VarMap& decls)
{
    for(auto d : decls)
    {
	uses.erase(d.second.Name());
    }
}

void AddToUses(VarSet& uses, VarSet& more)
{
    for(auto v : more)
    {
	uses.insert(v);
    }
}

void BuildClosures(ExprAST* ast)
{
    CallGraphClosureCollector v;
    CallGraph(ast, v);

    for(auto usage : v.useMap)
    {
	VarSet uses = usage.second;
	FunctionAST* func = const_cast<FunctionAST*>(usage.first);

	// Remove local declarations.
	RemoveFromUses(uses, v.declMap[func]);

	// Add uses from subfunctions.
	for(auto sub: func->SubFunctions())
	{
	    VarSet use = v.useMap[sub];
	    RemoveFromUses(use, v.declMap[sub]);
	    AddToUses(uses, use);
	}

	for(auto call: v.callMap[func])
	{
	    VarSet callerUse = v.useMap[call];
	    RemoveFromUses(callerUse, v.declMap[call]);
	    AddToUses(uses, callerUse);
	}
	
	// Now search up the stack until to see if
	// it's local "above" us.
	std::set<VarDef> used;
	for(auto use : uses)
	{
	    for(const FunctionAST* f = func->Parent(); f; f = f->Parent())
	    {
		const VarMap& dm = v.declMap[f];
		auto v = dm.find(use);
		if (v !=  dm.end())
		{
		    used.insert(v->second);
		    break;
		}
	    }
	}

	func->SetUsedVars(used);
	if (Types::TypeDecl *closure = func->ClosureType())
	{
	    func->Proto()->AddExtraArgsFirst({ VarDef(func->ClosureName(), closure) });
	    UpdateCallVisitor updater(func->Proto());
	    ast->accept(updater);
	}
    }
}
