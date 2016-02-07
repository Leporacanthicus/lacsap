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
    void CheckBinExpr(BinaryExprAST* b);
    void CheckAssignExpr(AssignExprAST* a);
    void CheckRangeExpr(RangeExprAST* r);
    void CheckSetExpr(SetExprAST* s);
    void CheckArrayExpr(ArrayExprAST* a);
    void CheckBuiltinExpr(BuiltinExprAST* b);
    void CheckCallExpr(CallExprAST* c);
    void CheckForExpr(ForExprAST* f);
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
    case Types::TypeDecl::TK_Int64:
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

    if (ty->isIntegral())
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

    return new Types::RangeDecl(r, base->Type());
}

void TypeCheckVisitor::Error(const ExprAST* e, const std::string& msg) const
{
    std::cerr << e->Loc() << ": " << msg << std::endl;
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
}

void TypeCheckVisitor::CheckBinExpr(BinaryExprAST* b)
{
    TRACE();

    Types::TypeDecl* lty = b->lhs->Type();
    Types::TypeDecl* rty = b->rhs->Type();
    Types::TypeDecl* ty = 0;
    Token::TokenType op = b->oper.GetToken();

    if (op == Token::In)
    {
	if (!lty->isIntegral())
	{
	    Error(b, "Left hand of 'in' expression should be integral.");
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
	ty = new Types::BoolDecl;
    }

    if (!ty && b->oper.IsCompare() && 
	(lty->Type() == Types::TypeDecl::TK_String || rty->Type() == Types::TypeDecl::TK_String))
    {
	ty = Types::GetStringType();
	if (*rty != *ty)
	{
	    ExprAST* e = b->rhs;
	    b->rhs = new TypeCastAST(e->Loc(), e, ty);
	}
	if (*lty != *ty)
	{
	    ExprAST* e = b->lhs;
	    b->lhs = new TypeCastAST(e->Loc(), e, ty);
	}
	ty = new Types::BoolDecl;
    }

    if (!ty && lty->Type() == Types::TypeDecl::TK_Set && rty->Type() == Types::TypeDecl::TK_Set)
    {
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
	
	if (!lty->GetRange() && rty->GetRange())
	{
	    llvm::dyn_cast<Types::SetDecl>(lty)->UpdateRange(GetRangeDecl(rty));
	}
	if (!rty->GetRange() && lty->GetRange())
	{
	    llvm::dyn_cast<Types::SetDecl>(rty)->UpdateRange(GetRangeDecl(lty));
	}
	if (*lty->SubType() != *rty->SubType())
	{
	    Error(b, "Set type content isn't the same!");
	}
	ty = rty;
    }

    if (!ty && (op == Token::Plus))
    {
	if (lty->Type() == Types::TypeDecl::TK_Char && rty->Type() == Types::TypeDecl::TK_Char)
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
	if (lty->isIntegral())
	{
	    ExprAST* e = b->lhs;
	    ty = new Types::RealDecl;
	    b->lhs = new TypeCastAST(e->Loc(), e, ty);
	    lty = ty;
	}
	if (rty->isIntegral())
	{
	    ExprAST* e = b->rhs;
	    if (!ty)
	    {
		ty = new Types::RealDecl;
	    }
	    b->rhs = new TypeCastAST(e->Loc(), e, ty);
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
	    ExprAST* e = b->rhs;
	    b->rhs = new TypeCastAST(e->Loc(), e, ty);
	}
	else
	{
	    ty = rty;
	    ExprAST* e = b->lhs;
	    b->lhs = new TypeCastAST(e->Loc(), e, ty);
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
	    if (!ty->isCompound())
	    {
		if (lty != ty)
		{
		    ExprAST* e = b->lhs;
		    b->lhs = new TypeCastAST(e->Loc(), e, ty);
		}
		if (rty != ty)
		{
		    ExprAST* e = b->rhs;
		    b->rhs = new TypeCastAST(e->Loc(), e, ty);
		}
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
	assert(lty->GetRange() && lty->SubType() &&
	       "Expected left type to be well defined.");

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
	ExprAST* e = a->rhs;
	a->rhs = new TypeCastAST(e->Loc(), e, lty);
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
    if (*ty != *rty)
    {
	ExprAST* e = a->rhs;
	a->rhs = new TypeCastAST(e->Loc(), e, ty);
    }
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
	Types::RangeDecl* r = a->ranges[i];
	if(llvm::isa<RangeReduceAST>(e))
	    continue;
	if (r->Type() != e->Type()->Type() && !e->Type()->CompatibleType(r))
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
	    if (*ty != *a->Type())
	    {
		ExprAST* e = a;
		a = new TypeCastAST(e->Loc(), e, ty);
	    }
	    bad = false;
	}
	else if (Types::FunctionDecl* fnTy = llvm::dyn_cast<Types::FunctionDecl>(a->Type()))
	{
	    if (Types::FuncPtrDecl* argTy = llvm::dyn_cast<Types::FuncPtrDecl>(parg[idx].Type()))
	    {
		if (fnTy->Proto()->IsMatchWithoutClosure(argTy->Proto()))
		{
		    /* Todo: Make this a function */
		    std::vector<VariableExprAST*> vf;
		    FunctionAST* fn = fnTy->Proto()->Function();
		    Types::TypeDecl* closureTy = fn->ClosureType();
		    for(auto u : fn->UsedVars())
		    {
			vf.push_back(new VariableExprAST(fn->Loc(), u.Name(), u.Type()));
		    }
		    ClosureAST* closure = new ClosureAST(fn->Loc(), closureTy, vf);
		    FunctionExprAST* e = llvm::dyn_cast<FunctionExprAST>(a);
		    assert(e && "Expected argument to be FunctionExprAST");
		    a = new TrampolineAST(e->loc, e, closure, argTy);
		    bad = false;
		}
	    }
	}
	if (bad)
	{
	    Error(a, "Incompatible argument type");
	}
	idx++;
    }
}

void TypeCheckVisitor::CheckForExpr(ForExprAST* f)
{
    // Check start + end and cast if necessary. Fail if incompatible types.
    bool bad = false;
    if (const Types::TypeDecl* ty = f->start->Type()->CompatibleType(f->variable->Type()))
    {
	if (*ty != *f->start->Type())
	{
	    ExprAST* e = f->start;
	    f->start = new TypeCastAST(e->Loc(), e, ty);
	}
    }
    else
    {
	bad = true;
    }
    if (const Types::TypeDecl* ty = f->end->Type()->CompatibleType(f->variable->Type()))
    {
	if (*ty != *f->end->Type())
	{
	    ExprAST* e = f->end;
	    f->end = new TypeCastAST(e->Loc(), e, ty);
	}
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
