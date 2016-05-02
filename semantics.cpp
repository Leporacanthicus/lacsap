#include "semantics.h"
#include "expr.h"
#include "visitor.h"
#include "trace.h"
#include "token.h"
#include "options.h"

class TypeCheckVisitor : public ASTVisitor
{
public:
    TypeCheckVisitor(Semantics* s) : sema(s) {};
    void visit(ExprAST* expr) override;
private:
    Types::TypeDecl* BinarySetUpdate(BinaryExprAST* b);
    void CheckBinExpr(BinaryExprAST* b);
    void CheckAssignExpr(AssignExprAST* a);
    void CheckRangeExpr(RangeExprAST* r);
    void CheckSetExpr(SetExprAST* s);
    void CheckArrayExpr(ArrayExprAST* a);
    void CheckBuiltinExpr(BuiltinExprAST* b);
    void CheckCallExpr(CallExprAST* c);
    void CheckForExpr(ForExprAST* f);
    void CheckReadExpr(ReadAST* f);
    void CheckWriteExpr(WriteAST* f);
    void Error(const ExprAST* e, const std::string& msg) const;
private:
    Semantics* sema;
};

class SemaFixup
{
public:
    SemaFixup() {}
    virtual void DoIt() = 0;
    virtual ~SemaFixup() {}
};

class SetRangeFixup : public SemaFixup
{
public:
    SetRangeFixup(SetExprAST* s, Types::RangeDecl* r) : expr(s), guessRange(r) {}
    void DoIt() override;
private:
    SetExprAST*       expr;
    Types::RangeDecl* guessRange;
};

void SetRangeFixup::DoIt()
{
    if (!expr->Type()->GetRange())
    {
	Types::SetDecl* sd = llvm::dyn_cast<Types::SetDecl>(expr->Type());
	sd->UpdateRange(guessRange);
    }
}

static bool isNumeric(Types::TypeDecl* t)
{
    switch(t->Type())
    {
    case Types::TypeDecl::TK_Integer:
    case Types::TypeDecl::TK_LongInt:
    case Types::TypeDecl::TK_Real:
	return true;
    default:
	return false;
    }
}

static Types::RangeDecl* GetRangeDecl(Types::TypeDecl* ty)
{
    Types::Range* r = ty->GetRange();
    Types::TypeDecl* base;
    if (!r)
    {
	if (!ty->SubType())
	{
	    return 0;
	}
	r = ty->SubType()->GetRange();
    }

    if (ty->IsIntegral())
    {
	base = ty;
    }
    else
    {
	base = ty->SubType();
    }

    if (r->Size() > Types::SetDecl::MaxSetSize)
    {
	r = new Types::Range(0, Types::SetDecl::MaxSetSize-1);
    }

    return new Types::RangeDecl(r, base);
}

void TypeCheckVisitor::Error(const ExprAST* e, const std::string& msg) const
{
    std::cerr << e->Loc() << " Error: " << msg << std::endl;
    sema->AddError();
}

void TypeCheckVisitor::visit(ExprAST* expr)
{
    TRACE();

    if (verbosity > 1)
    {
	expr->dump();
    }

    if (BinaryExprAST* b = llvm::dyn_cast<BinaryExprAST>(expr))
    {
	CheckBinExpr(b);
    }
    else if (AssignExprAST* a = llvm::dyn_cast<AssignExprAST>(expr))
    {
	CheckAssignExpr(a);
    }
    else if (RangeExprAST* r = llvm::dyn_cast<RangeExprAST>(expr))
    {
	CheckRangeExpr(r);
    }
    else if (SetExprAST* s = llvm::dyn_cast<SetExprAST>(expr))
    {
	CheckSetExpr(s);
    }
    else if (ArrayExprAST* a = llvm::dyn_cast<ArrayExprAST>(expr))
    {
	CheckArrayExpr(a);
    }
    else if (BuiltinExprAST* b = llvm::dyn_cast<BuiltinExprAST>(expr))
    {
	CheckBuiltinExpr(b);
    }
    else if (CallExprAST* c = llvm::dyn_cast<CallExprAST>(expr))
    {
	CheckCallExpr(c);
    }
    else if (ForExprAST* f = llvm::dyn_cast<ForExprAST>(expr))
    {
	CheckForExpr(f);
    }
    else if (ReadAST* r = llvm::dyn_cast<ReadAST>(expr))
    {
	CheckReadExpr(r);
    }
    else if (WriteAST* w = llvm::dyn_cast<WriteAST>(expr))
    {
	CheckWriteExpr(w);
    }
}

ExprAST* Recast(ExprAST* a, const Types::TypeDecl* ty)
{
    if (*ty != *a->Type())
    {
	ExprAST* e = a;
	a = new TypeCastAST(e->Loc(), e, ty);
    }
    return a;
}

Types::TypeDecl* TypeCheckVisitor::BinarySetUpdate(BinaryExprAST* b)
{
    Types::TypeDecl* lty = b->lhs->Type();
    Types::TypeDecl* rty = b->rhs->Type();
    if (SetExprAST* s = llvm::dyn_cast<SetExprAST>(b->lhs))
    {
	if (s->values.empty() && rty->SubType())
	{
	    llvm::dyn_cast<Types::SetDecl>(lty)->UpdateSubtype(
		    llvm::dyn_cast<Types::SetDecl>(rty)->SubType());
	}
    }
    if (SetExprAST* s = llvm::dyn_cast<SetExprAST>(b->rhs))
    {
	if (s->values.empty() && lty->SubType())
	{
	    llvm::dyn_cast<Types::SetDecl>(rty)->UpdateSubtype(
		llvm::dyn_cast<Types::SetDecl>(lty)->SubType());
	}
    }
    if (!lty->GetRange() && !rty->GetRange())
    {
	Types::RangeDecl* r = GetRangeDecl(Types::GetIntegerType());
	Types::SetDecl* rs = llvm::dyn_cast<Types::SetDecl>(rty);
	Types::SetDecl* ls = llvm::dyn_cast<Types::SetDecl>(lty);
	
	if (!rs->SubType() && !ls->SubType())
	{
	    rs->UpdateSubtype(r->SubType());
	    ls->UpdateSubtype(r->SubType());
	}
	else
	{
	    r = GetRangeDecl(rs->SubType());
	}
	
	ls->UpdateRange(r);
	rs->UpdateRange(r);
    }
    
    if (!lty->GetRange() && rty->GetRange())
    {
	llvm::dyn_cast<Types::SetDecl>(lty)->UpdateRange(GetRangeDecl(rty));
    }
    if (!rty->GetRange() && lty->GetRange())
    {
	llvm::dyn_cast<Types::SetDecl>(rty)->UpdateRange(GetRangeDecl(lty));
    }

    Types::Range* lr = lty->GetRange();
    Types::Range* rr = rty->GetRange();

    if (*lty->SubType() != *rty->SubType())
    {
	Error(b, "Set type content isn't the same!");
    }
    else if (rr && lr && *rr != *lr)
    {
	Types::Range* range = new Types::Range(std::min(lr->Start(), rr->Start()),
					       std::max(lr->End(), rr->End()));

	Types::RangeDecl* r = new Types::RangeDecl(range, rty->SubType());
	Types::SetDecl* set = new Types::SetDecl(r, rty->SubType());

	b->lhs = Recast(b->lhs, set);
	b->rhs = Recast(b->rhs, set);
    }
    return rty;
}

void TypeCheckVisitor::CheckBinExpr(BinaryExprAST* b)
{
    TRACE();

    Types::TypeDecl* lty = b->lhs->Type();
    Types::TypeDecl* rty = b->rhs->Type();
    Types::TypeDecl* ty = 0;
    Token::TokenType op = b->oper.GetToken();

    assert(rty && lty && "Expect to have types here");

    if (op == Token::In)
    {
	if (!lty->IsIntegral())
	{
	    Error(b, "Left hand of 'in' expression should be integral type.");
	}

	// Empty set always has the "right" type
	if (SetExprAST* s = llvm::dyn_cast<SetExprAST>(b->rhs))
	{
	    if (s->values.empty())
	    {
		llvm::dyn_cast<Types::SetDecl>(rty)->UpdateSubtype(lty);
	    }
	}
	if(Types::SetDecl* sd = llvm::dyn_cast<Types::SetDecl>(rty))
	{
	    assert(sd->SubType() && "Should have a subtype");
	    if (*lty != *sd->SubType())
	    {
		Error(b, "Left hand type does not match constituent parts of set");
	    }
	    if (!sd->GetRange())
	    {
		sd->UpdateRange(GetRangeDecl(lty));
	    }
	}
	else
	{
	    Error(b, "Right hand of 'in' expression should be a set.");
	}
	ty = Types::GetBooleanType();
    }

    if (!ty && b->oper.IsCompare() && 
	(lty->Type() == Types::TypeDecl::TK_String || rty->Type() == Types::TypeDecl::TK_String))
    {
	ty = Types::GetStringType();
	b->rhs = Recast(b->rhs, ty);
	b->lhs = Recast(b->lhs, ty);
	ty = Types::GetBooleanType();
    }

    if (!ty && lty->Type() == Types::TypeDecl::TK_Set && rty->Type() == Types::TypeDecl::TK_Set)
    {
	ty = BinarySetUpdate(b);
    }

    if (!ty && (op == Token::Plus))
    {
	if (lty->Type() == Types::TypeDecl::TK_Char || rty->Type() == Types::TypeDecl::TK_Char)
	{
	    ty = Types::GetStringType();
	}
    }

    if (!ty && (op == Token::Divide))
    {
	if (!isNumeric(lty) || !isNumeric(rty))
	{
	    Error(b, "Invalid (non-numeric) type for divide");
	}
	
	ty = Types::GetRealType();
	if (lty->IsIntegral())
	{
	    b->lhs = Recast(b->lhs, ty);
	    lty = ty;
	}
	if (rty->IsIntegral())
	{
	    b->rhs = Recast(b->rhs, ty);
	    rty = ty;
	}
	if (!lty->CompatibleType(rty))
	{
	    Error(b, "Incompatible type for divide");
	}
    }

    if (!ty && 
	((llvm::isa<Types::PointerDecl>(lty) && llvm::isa<NilExprAST>(b->rhs)) ||
	 (llvm::isa<Types::PointerDecl>(rty) && llvm::isa<NilExprAST>(b->lhs))) &&
	(op == Token::Equal || op == Token::NotEqual))
    {
	if (llvm::isa<NilExprAST>(b->rhs))
	{
	    ty = lty;
	    b->rhs = Recast(b->rhs, ty);
	}
	else
	{
	    ty = rty;
	    b->lhs = Recast(b->lhs, ty);
	}
    }

    if (!ty && llvm::isa<Types::RangeDecl>(lty) && llvm::isa<IntegerExprAST>(b->rhs))
    {
	ty = lty;
    }

    if (llvm::isa<Types::RangeDecl>(rty) && llvm::isa<IntegerExprAST>(b->lhs))
    {
	ty = rty;
    }

    if (!ty)
    {
	if ((ty = const_cast<Types::TypeDecl*>(lty->CompatibleType(rty))))
	{
	    if (!ty->IsCompound())
	    {
		b->lhs = Recast(b->lhs, ty);
		b->rhs = Recast(b->rhs, ty);
	    }
	}
	else
	{
	    Error(b, "Incompatible type in expression");
	}
    }
    b->UpdateType(ty);
}

void TypeCheckVisitor::CheckAssignExpr(AssignExprAST* a)
{
    TRACE();

    Types::TypeDecl* lty = a->lhs->Type();
    Types::TypeDecl* rty = a->rhs->Type();

    if (lty->Type() == Types::TypeDecl::TK_Set && rty->Type() == Types::TypeDecl::TK_Set)
    {
	assert(lty->GetRange() && lty->SubType() && "Expected left type to be well defined.");

	if (!rty->GetRange())
	{
	    llvm::dyn_cast<Types::SetDecl>(rty)->UpdateRange(GetRangeDecl(lty));
	}
	if (!rty->SubType())
	{
	    llvm::dyn_cast<Types::SetDecl>(rty)->UpdateSubtype(lty->SubType());
	}
    }

    // Note difference to binary expression: only allowed on rhs!
    if (llvm::isa<Types::PointerDecl>(lty) && llvm::isa<NilExprAST>(a->rhs))
    {
	a->rhs = Recast(a->rhs, lty);
	return;
    }

    if (llvm::isa<Types::RangeDecl>(lty) && llvm::isa<IntegerExprAST>(a->rhs))
    {
	Types::Range* r = lty->GetRange();
	int64_t v = llvm::dyn_cast<IntegerExprAST>(a->rhs)->Int();
	if (r->Start() > v || v > r->End())
	{
	    Error(a, "Value out of range");
	}
	return;
    }

    if (llvm::isa<Types::ArrayDecl>(lty) && 
	!llvm::isa<Types::StringDecl>(lty) && 
	llvm::isa<StringExprAST>(a->rhs))
    {
	StringExprAST* s = llvm::dyn_cast<StringExprAST>(a->rhs);
	Types::ArrayDecl* aty = llvm::dyn_cast<Types::ArrayDecl>(lty);
	if (aty->SubType()->Type() == Types::TypeDecl::TK_Char && aty->Ranges().size() == 1)
	{
	    if (aty->Ranges()[0]->GetRange()->Size() == s->Str().size())
	    {
		return;
	    }
	}
	Error(a, "String assignment from incompatible string constant");
	return;
    }

    const Types::TypeDecl* ty = lty->AssignableType(rty);
    if (!ty)
    {
	Error(a, "Incompatible type in assignment");
	return;
    }
    a->rhs = Recast(a->rhs, ty);
}

void TypeCheckVisitor::CheckRangeExpr(RangeExprAST* r)
{
    TRACE();

    if (*r->low->Type() != *r->high->Type())
    {
	Error(r, "Range should be same type at both ends");
    }
}

void TypeCheckVisitor::CheckSetExpr(SetExprAST* s)
{
    TRACE();

    Types::Range* r;
    if (!(r = s->Type()->GetRange()))
    {
	Types::RangeDecl* rd = GetRangeDecl(s->Type());
	if (s->Type()->SubType())
	{
	    sema->AddFixup(new SetRangeFixup(s, rd));
	}
    }
}

void TypeCheckVisitor::CheckArrayExpr(ArrayExprAST* a)
{
    TRACE();

    for(size_t i = 0; i < a->indices.size(); i++)
    {
	ExprAST* e = a->indices[i];
	if (!e->Type()->IsIntegral())
	{
	    Error(e, "Index should be an integral type");
	}
	Types::RangeDecl* r = a->ranges[i];
	if(llvm::isa<RangeReduceAST>(e))
	    continue;
	if (r->Type() != e->Type()->Type() && !r->CompatibleType(e->Type()))
	{
	    Error(a, "Incorrect index type");
	}
	if (rangeCheck)
	{
	    a->indices[i] = new RangeCheckAST(e, r);
	}
	else
	{
	    a->indices[i] = new RangeReduceAST(e, r);
	}
    }
}

void TypeCheckVisitor::CheckBuiltinExpr(BuiltinExprAST* b)
{
    TRACE();
    if (!b->bif->Semantics())
    {
	Error(b, "Invalid use of builtin function");
    }
}

void TypeCheckVisitor::CheckCallExpr(CallExprAST* c)
{
    TRACE();
    const PrototypeAST* proto = c->proto;
    if (c->args.size() != proto->args.size())
    {
	Error(c, "Incorrect number of arguments in call to " + c->proto->Name());
	return;
    }
    int idx = 0;
    const std::vector<VarDef>& parg = proto->args;
    for(auto& a : c->args)
    {
	bool bad = true;

	if (const Types::TypeDecl* ty = parg[idx].Type()->CompatibleType(a->Type()))
	{
	    a = Recast(a, ty);
	    bad = false;
	}
	else if (llvm::isa<Types::PointerDecl>(parg[idx].Type()) && llvm::isa<NilExprAST>(a))
	{
	    a = Recast(a, parg[idx].Type());
	    bad = false;
	}
	else if (Types::FuncPtrDecl* argTy = llvm::dyn_cast<Types::FuncPtrDecl>(parg[idx].Type()))
	{
	    FunctionExprAST* fnArg = llvm::dyn_cast<FunctionExprAST>(a);

	    if (fnArg->Proto()->IsMatchWithoutClosure(argTy->Proto()))
	    {
		/* Todo: Make this a function */
		std::vector<VariableExprAST*> vf;
		FunctionAST* fn = fnArg->Proto()->Function();
		Types::TypeDecl* closureTy = fn->ClosureType();
		for(auto u : fn->UsedVars())
		{
		    vf.push_back(new VariableExprAST(fn->Loc(), u.Name(), u.Type()));
		}
		ClosureAST* closure = new ClosureAST(fn->Loc(), closureTy, vf);
		assert(fnArg && "Expected argument to be FunctionExprAST");
		a = new TrampolineAST(fnArg->loc, fnArg, closure, argTy);
		bad = false;
	    }
	    else
	    {
		bad = !(*fnArg->Proto() == *argTy->Proto());
	    }
	}
	if (bad)
	{
	    Error(a, "Incompatible argument type " + std::to_string(idx));
	}
	idx++;
    }
}

void TypeCheckVisitor::CheckForExpr(ForExprAST* f)
{
    // Check start + end and cast if necessary. Fail if incompatible types.
    Types::TypeDecl* vty = f->variable->Type();
    bool bad = !vty->IsIntegral();
    if (bad)
    {
	Error(f->variable, "Loop iteration variable must be integral type");
	return;
    }

    if (const Types::TypeDecl* ty = f->start->Type()->CompatibleType(vty))
    {
	f->start = Recast(f->start, ty);
    }
    else
    {
	bad = true;
    }
    if (const Types::TypeDecl* ty = f->end->Type()->CompatibleType(vty))
    {
	f->end = Recast(f->end, ty);
    }
    else
    {
	bad = true;
    }
    if (bad)
    {
	Error(f, "Bad for loop");
    }
}

void TypeCheckVisitor::CheckReadExpr(ReadAST* r)
{
    bool isText = llvm::isa<Types::TextDecl>(r->file->Type());

    if (isText)
    {
	for(auto arg : r->args)
	{
	    if (!llvm::isa<VariableExprAST>(arg))
	    {
		Error(arg, "Invalid argument for read/readln - must be a variable-expression");
	    }
	    if (arg->Type()->IsCompound())
	    {
		bool bad = true;
		if (Types::ArrayDecl* a = llvm::dyn_cast<Types::ArrayDecl>(arg->Type()))
		{
		    bad = !llvm::isa<Types::CharDecl>(a->SubType());
		}
		else
		{
		    bad = !llvm::isa<Types::StringDecl>(arg->Type());
		}
		if (bad)
		{
		    Error(arg, "Read argument must be simple type [or array of char NYI] or string");
		}
	    }
	}
    }
    else
    {
	if (r->args.size() != 1)
	{
	    Error(r, "Read of binary file must have exactly one argument");
	}
	else
	{
	    ExprAST* arg = r->args[0];
	    if (!llvm::isa<VariableExprAST>(arg))
	    {
		Error(arg, "Invalid argument for read - must be a variable-expression");
	    }
	    else
	    {
		if (!r->file->Type()->SubType()->AssignableType(arg->Type()))
		{
		    Error(arg, "Read argument should match elements of the file");
		}
	    }
	}
    }
}

void TypeCheckVisitor::CheckWriteExpr(WriteAST* w)
{
    bool isText = llvm::isa<Types::TextDecl>(w->file->Type());

    if (isText)
    {
	for(auto arg : w->args)
	{
	    ExprAST* e = arg.expr;
	    if (e->Type()->IsCompound())
	    {
		bool bad = true;
		if (Types::ArrayDecl* a = llvm::dyn_cast<Types::ArrayDecl>(e->Type()))
		{
		    bad = !llvm::isa<Types::CharDecl>(a->SubType());
		}
		else 
		{
		    bad = !llvm::isa<Types::StringDecl>(e->Type());
		}
		if (bad)
		{
		    Error(e, "Write argument must be simple type or array of char or string");
		}
	    }
	}
    }
    else
    {
	if (w->args.size() != 1)
	{
	    Error(w, "Write of binary file must have exactly one argument");
	}
	else
	{
	    ExprAST* arg = w->args[0].expr;
	    if (!llvm::isa<VariableExprAST>(arg))
	    {
		Error(arg, "Invalid argument for binary write - must be a variable-expression");
	    }
	    else
	    {
		if (!w->file->Type()->SubType()->AssignableType(arg->Type()))
		{
		    Error(arg, "Write argument should match elements of the file");
		}
	    }
	}
    }
}

void Semantics::AddFixup(SemaFixup* f)
{
    TRACE();
    fixups.push_back(f);
}

void Semantics::RunFixups()
{
    TRACE();

    for(auto f : fixups)
    {
	f->DoIt();
    }
}

void Semantics::Analyse(ExprAST* ast)
{
    TIME_TRACE();
    TRACE();

    TypeCheckVisitor tc(this);
    ast->accept(tc);
    RunFixups();
}
