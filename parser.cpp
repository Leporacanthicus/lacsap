#include "source.h"
#include "lexer.h"
#include "parser.h"
#include "expr.h"
#include "namedobject.h"
#include "stack.h"
#include "builtin.h"
#include "options.h"
#include "trace.h"
#include "utils.h"
#include <iostream>
#include <cassert>
#include <limits>
#include <vector>
#include <algorithm>
#include <cmath>

class UpdateCallVisitor : public ASTVisitor
{
public:
    UpdateCallVisitor(const PrototypeAST *p) : proto(p) {}
    virtual void visit(ExprAST* expr);
private:
    const PrototypeAST* proto;
};

static void AddClosureArg(FunctionAST* fn, std::vector<ExprAST*>& args)
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

/* This is used to update internal calls within the nest of functions, where we need
 * to pass variables from the outer scope to the inner scope
 */
void UpdateCallVisitor::visit(ExprAST* expr)
{
    TRACE();

    if (verbosity > 1)
    {
	expr->dump();
    }

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

ExprAST* Parser::Error(const std::string& msg, const char* file, int line)
{
    if (file)
    {
	std::cerr << file << ":" << line << ": ";
    }
    std::cerr << "Error: " << msg << std::endl;
    errCnt++;
    if (errCnt > 256)
    {
	std::cerr << "Too many errors, quitting..." << std::endl;
	exit(1);
    }
    return 0;
}

PrototypeAST* Parser::ErrorP(const std::string& msg)
{
    return reinterpret_cast<PrototypeAST*>(Error(msg));
}

FunctionAST* Parser::ErrorF(const std::string& msg)
{
    return reinterpret_cast<FunctionAST*>(Error(msg));
}

Types::TypeDecl* Parser::ErrorT(const std::string& msg)
{
    return reinterpret_cast<Types::TypeDecl*>(Error(msg));
}

Types::RangeDecl* Parser::ErrorR(const std::string& msg)
{
    return reinterpret_cast<Types::RangeDecl*>(Error(msg));
}

VariableExprAST* Parser::ErrorV(const std::string& msg)
{
    return reinterpret_cast<VariableExprAST*>(Error(msg));
}

Constants::ConstDecl* Parser::ErrorC(const std::string& msg)
{
    return reinterpret_cast<Constants::ConstDecl*>(Error(msg));
}

int Parser::ErrorI(const std::string& msg)
{
    return reinterpret_cast<uintptr_t>(Error(msg));
}

const Token& Parser::CurrentToken() const
{
    return curToken;
}

const Token& Parser::NextToken(const char* file, int line)
{
    if (nextTokenValid)
    {
	curToken = nextToken;
	nextTokenValid = false;
    }
    else
    {
	curToken = lexer.GetToken();
    }
    if (verbosity)
    {
	curToken.dump(std::cerr, file, line);
    }
    return curToken;
}

const Token& Parser::PeekToken(const char* file, int line)
{
    if (nextTokenValid)
    {
	return nextToken;
    }

    nextTokenValid = true;
    nextToken = lexer.GetToken();

    if (verbosity > 1)
    {
	std::cerr << "peeking: ";
	nextToken.dump(std::cerr, file, line);
    }
    return nextToken;
}

bool Parser::Expect(Token::TokenType type, bool eatIt, const char* file, int line)
{
    if (CurrentToken().GetToken() != type)
    {
	Token t(type, Location("", 0, 0));
	Error(std::string("Expected '") + t.TypeStr() + "', got '" +  CurrentToken().ToString() +
	      "'.", file, line);
	return false;
    }
    if (eatIt)
    {
	NextToken(file, line);
    }
    return true;
}

bool Parser::IsSemicolonOrEnd()
{
    return (CurrentToken().GetToken() == Token::End || CurrentToken().GetToken() == Token::Semicolon);
}

bool Parser::ExpectSemicolonOrEnd(const char* file, int line)
{
    return !(CurrentToken().GetToken() != Token::End && !Expect(Token::Semicolon, true, file, line));
}

// Skip token, and check that it's matching what we expect.
// This is used when we (should) have already checked the token, so asserting is fine.
void Parser::AssertToken(Token::TokenType type, const char *file, int line)
{
    if (CurrentToken().GetToken() != type)
    {
	Token t(type, Location("", 0, 0));
	Error(std::string("Expected '") + t.TypeStr() + "', got '" +  CurrentToken().ToString() +
	      "'.", file, line);
	assert(0 && "Unexpected token");
    }
    NextToken(file, line);
}

// Check if the current token is a particule one.
// Return true and eat token if it is, return false if if it's not (and leave token in place)..
bool Parser::AcceptToken(Token::TokenType type, const char *file, int line)
{
    if (CurrentToken().GetToken() == type)
    {
	if (verbosity > 0)
	{
	    std::cerr << "accepting: ";
	    curToken.dump(std::cerr, file, line);
	}
	NextToken(file, line);
	return true;
    }
    return false;
}

#define NextToken() NextToken(__FILE__, __LINE__)
#define PeekToken() PeekToken(__FILE__, __LINE__)
#define ExpectSemicolonOrEnd() ExpectSemicolonOrEnd(__FILE__, __LINE__)
#define Expect(t, e) Expect(t, e, __FILE__, __LINE__)
#define AssertToken(t) AssertToken(t, __FILE__, __LINE__)
#define AcceptToken(t) AcceptToken(t, __FILE__, __LINE__)

Types::TypeDecl* Parser::GetTypeDecl(const std::string& name)
{
    if (NamedObject* def = nameStack.Find(name))
    {
	if (TypeDef *typeDef = llvm::dyn_cast<TypeDef>(def))
	{
	    return typeDef->Type();
	}
    }
    return 0;
}

ExprAST* Parser::ParseSizeOfExpr()
{
    AssertToken(Token::SizeOf);
    if (!Expect(Token::LeftParen, true))
    {
	return 0;
    }
    ExprAST* expr = 0;
    if (CurrentToken().GetToken() == Token::Identifier)
    {
	if (Types::TypeDecl* ty = GetTypeDecl(CurrentToken().GetIdentName()))
	{
	    expr = new SizeOfExprAST(CurrentToken().Loc(), ty);
	    AssertToken(Token::Identifier);
	}
    }
    if (!expr)
    {
	if (ExprAST* e = ParseExpression())
	{
	    expr = new SizeOfExprAST(CurrentToken().Loc(), e->Type());
	}
    }
    if (!Expect(Token::RightParen, true))
    {
	return 0;
    }
    return expr;
}

ExprAST* Parser::ParseGoto()
{
    AssertToken(Token::Goto);
    Token t = CurrentToken();
    if (!Expect(Token::Integer, true))
    {
	return 0;
    }
    int n = t.GetIntVal();
    if (!nameStack.FindTopLevel(std::to_string(n)))
    {
	return Error("Label and GOTO need to be declared in the same scope");
    }
    return new GotoAST(t.Loc(), n);
}

const Constants::ConstDecl* Parser::GetConstDecl(const std::string& name)
{
    if (ConstDef *constDef = llvm::dyn_cast_or_null<ConstDef>(nameStack.Find(name)))
    {
	return constDef->ConstValue();
    }
    return 0;
}

EnumDef* Parser::GetEnumValue(const std::string& name)
{
    return llvm::dyn_cast_or_null<EnumDef>(nameStack.Find(name));
}

bool Parser::AddType(const std::string& name, Types::TypeDecl* ty)
{
    return nameStack.Add(name, new TypeDef(name, ty));
}

bool Parser::AddConst(const std::string& name, const Constants::ConstDecl* cd)
{
    if (!nameStack.Add(name, new ConstDef(name, cd)))
    {
	return (bool)ErrorI("Name " + name + " is already declared as a constant");
    }
    return true;

}

Types::TypeDecl* Parser::ParseSimpleType()
{
    if (Expect(Token::Identifier, false))
    {
	if (Types::TypeDecl* ty = GetTypeDecl(CurrentToken().GetIdentName()))
	{
	    AssertToken(Token::Identifier);
	    return ty;
	}
	return ErrorT("Identifier '" + CurrentToken().GetIdentName() + "' does not name a type");
    }
    return 0;
}

Token Parser::TranslateToken(const Token& token)
{
    if (token.GetToken() == Token::Identifier)
    {
	if (const Constants::ConstDecl* cd = GetConstDecl(token.GetIdentName()))
	{
	    return cd->Translate();
	}
    }
    return token;
}

int Parser::ParseConstantValue(Token::TokenType& tt, Types::TypeDecl*& type)
{
    bool negative = false;
    if (CurrentToken().GetToken() == Token::Minus)
    {
	negative = true;
	NextToken();
    }

    Token token = TranslateToken(CurrentToken());

    if (tt != Token::Unknown && token.GetToken() != tt)
    {
	tt = Token::Unknown;
	return ErrorI("Expected token to match type");
    }

    tt = token.GetToken();
    int result = 0;

    switch(tt)
    {
    case Token::Integer:
	type = new Types::IntegerDecl;
	result = token.GetIntVal();
	break;

    case Token::Char:
	type = new Types::CharDecl;
	result = token.GetIntVal();
	break;

    case Token::Identifier:
    {
	tt = CurrentToken().GetToken();
	
	if (EnumDef* ed = GetEnumValue(CurrentToken().GetIdentName()))
	{
	    type = ed->Type();
	    result = ed->Value();
	}
	else
	{
	    tt = Token::Unknown;
	    return ErrorI("Invalid constant, expected identifier for enumerated type");
	}
	break;
    }
    default:
	tt = Token::Unknown;
	return ErrorI("Invalid constant value, expected char, integer or enum value");
    }
    NextToken();
    if (negative)
    {
	if (tt != Token::Integer)
	{
	    return ErrorI("Invalid negative constant");
	}
	result = -result;
    }
    return result;
}

Types::RangeDecl* Parser::ParseRange(Types::TypeDecl*& type)
{
    Token::TokenType tt = Token::Unknown;

    int start = ParseConstantValue(tt, type);
    if (tt == Token::Unknown || !Expect(Token::DotDot, true))
    {
	return 0;
    }

    int end = ParseConstantValue(tt, type);

    if (tt == Token::Unknown)
    {
	return 0;
    }

    if (end <= start)
    {
	return ErrorR("Invalid range specification");
    }
    return new Types::RangeDecl(new Types::Range(start, end), type->Type());
}

Types::RangeDecl* Parser::ParseRangeOrTypeRange(Types::TypeDecl*& type)
{
    if (CurrentToken().GetToken() == Token::Identifier)
    {
	if ((type = GetTypeDecl(CurrentToken().GetIdentName())))
	{
	    if (!type->IsIntegral())
	    {
		return ErrorR("Type used as index specification should be integral type");
	    }
	    AssertToken(Token::Identifier);
	    return new Types::RangeDecl(type->GetRange(), type->Type());
	}
    }

    return ParseRange(type);
}

Constants::ConstDecl* Parser::ParseConstEval(const Constants::ConstDecl* lhs,
					     const Token& binOp,
					     const Constants::ConstDecl* rhs)
{
    switch(binOp.GetToken())
    {
    case Token::Plus:
	return *lhs + *rhs;
	break;

    case Token::Minus:
	return *lhs - *rhs;
	break;

    case Token::Multiply:
	return *lhs * *rhs;

    default:
	break;
    }
    return 0;
}

const Constants::ConstDecl* Parser::ParseConstRHS(int exprPrec, const Constants::ConstDecl* lhs)
{
    for(;;)
    {
	Token binOp = CurrentToken();
	int tokPrec = binOp.Precedence();
	// Lower precedence, so don't use up the binOp.
	if (tokPrec < exprPrec)
	{
	    return lhs;
	}

	NextToken();

	const Constants::ConstDecl* rhs = ParseConstExpr();
	if (!rhs)
	{
	    return 0;
	}
	
	int nextPrec = CurrentToken().Precedence();
	if (tokPrec < nextPrec)
	{
	    if (!(rhs = ParseConstRHS(tokPrec + 1, rhs)))
	    {
		return 0;
	    }
	}
	lhs = ParseConstEval(lhs, binOp, rhs);
    }
}

const Constants::ConstDecl* Parser::ParseConstExpr()
{
    Token::TokenType unaryToken = Token::Unknown;
    Location loc = CurrentToken().Loc();
    const Constants::ConstDecl* cd;
    int mul = 1;
    do
    {
	switch(CurrentToken().GetToken())
	{
	case Token::Minus:
	    mul = -1;
	case Token::Plus:
	case Token::Not:
	    unaryToken = CurrentToken().GetToken();
	    break;

	case Token::LeftParen:
	    AssertToken(Token::LeftParen);
	    cd = ParseConstExpr();
	    // We don't eat the right paren here, it gets eaten later.
	    if (!Expect(Token::RightParen, false))
	    {
		return 0;
	    }
	    break;

	case Token::StringLiteral:
	    if (unaryToken != Token::Unknown)
	    {
		return ErrorC("Unary + or - not allowed for string constants");
	    }
	    cd = new Constants::StringConstDecl(loc, CurrentToken().GetStrVal());
	    break;

	case Token::Integer:
	{
	    uint64_t v = CurrentToken().GetIntVal();
	    if (unaryToken == Token::Not)
	    {
		v = ~v;
	    }
	    cd = new Constants::IntConstDecl(loc,  v * mul);
	    break;
	}

	case Token::Real:
	    if (unaryToken == Token::Not)
	    {
		return ErrorC("Unary 'not' is not allowed for real constants");
	    }
	    cd = new Constants::RealConstDecl(loc, CurrentToken().GetRealVal() * mul);
	    break;

	case Token::Char:
	    if (unaryToken != Token::Unknown)
	    {
		return ErrorC("Unary + or - not allowed for char constants");
	    }
	    cd = new Constants::CharConstDecl(loc, (char) CurrentToken().GetIntVal());
	    break;

	case Token::Identifier:
	{
	    if (EnumDef* ed = GetEnumValue(CurrentToken().GetIdentName()))
	    {
		if (ed->Type()->Type() == Types::TypeDecl::TK_Boolean)
		{
		    uint64_t v = ed->Value();
		    if (unaryToken == Token::Not)
		    {
			v = !v;
		    }
		    else if (unaryToken != Token::Unknown)
		    {
			return ErrorC("Unary + or - not allowed for bool constants");
		    }
		    cd = new Constants::BoolConstDecl(loc, v);
		}
		else
		{
		    if (unaryToken != Token::Unknown)
		    {
			return ErrorC("Unary + or - not allowed for enum constants");
		    }
		    cd = new Constants::IntConstDecl(loc, ed->Value());
		}
	    }
	    else
	    {
		cd = GetConstDecl(CurrentToken().GetIdentName());
		assert(cd && "Expected to get an identifier!");
		if (llvm::isa<Constants::BoolConstDecl>(cd) && unaryToken == Token::Not)
		{
		    const Constants::BoolConstDecl* bd =
			llvm::dyn_cast<Constants::BoolConstDecl>(cd);
		    cd = new Constants::BoolConstDecl(loc, !bd->Value());
		}
		
		if (mul == -1)
		{
		    if (llvm::isa<Constants::RealConstDecl>(cd))
		    {
			const Constants::RealConstDecl* rd =
			    llvm::dyn_cast<Constants::RealConstDecl>(cd);
			cd = new Constants::RealConstDecl(loc, -rd->Value());
		    }
		    else if (llvm::isa<Constants::IntConstDecl>(cd))
		    {
			const Constants::IntConstDecl* id =
			    llvm::dyn_cast<Constants::IntConstDecl>(cd);
			cd = new Constants::IntConstDecl(loc, -id->Value());
		    }
		    else
		    {
			return ErrorC("Can't negate the type of " + CurrentToken().GetIdentName() +
				      " only integer and real types can be negated");
		    }
		}
	    }
	    break;
	}
	
	default:
	    return 0;
	}
	NextToken();
	if (CurrentToken().GetToken() != Token::Semicolon &&
	    CurrentToken().GetToken() != Token::RightParen)
	{
	    cd = ParseConstRHS(0, cd);
	}
    } while(CurrentToken().GetToken() != Token::Semicolon &&
	    CurrentToken().GetToken() != Token::RightParen);
    return cd;
}

void Parser::ParseConstDef()
{
    AssertToken(Token::Const);
    do
    {
	if (!Expect(Token::Identifier, false))
	{
	    return;
	}
	std::string nm = CurrentToken().GetIdentName();
	AssertToken(Token::Identifier);
	if (!Expect(Token::Equal, true))
	{
	    return;
	}
	const Constants::ConstDecl *cd = ParseConstExpr();
	if (!cd)
	{
	    Error("Invalid constant value");
	}
	if (!AddConst(nm, cd))
	{
	    return;
	}
	if (!Expect(Token::Semicolon, true))
	{
	    return;
	}
    } while (CurrentToken().GetToken() == Token::Identifier);
}

// Deal with type name = ... defintions
void Parser::ParseTypeDef()
{
    std::vector<Types::PointerDecl*> incomplete;
    AssertToken(Token::Type);
    do
    {
	if (!Expect(Token::Identifier, false))
	{
	    return;
	}
	std::string nm = CurrentToken().GetIdentName();
	AssertToken(Token::Identifier);
	if (!Expect(Token::Equal, true))
	{
	    return;
	}
	if (Types::TypeDecl* ty = ParseType(nm, true))
	{
	    if (!AddType(nm,  ty))
	    {
		Error("Name " + nm + " is already in use.");
		return;
	    }
	    if (ty->Type() == Types::TypeDecl::TK_Pointer &&
		llvm::dyn_cast<Types::PointerDecl>(ty)->IsIncomplete())
	    {
		incomplete.push_back(llvm::dyn_cast<Types::PointerDecl>(ty));
	    }
	    if (!Expect(Token::Semicolon, true))
	    {
		return;
	    }
	}
	else
	{
	    return;
	}
    } while (CurrentToken().GetToken() == Token::Identifier);

    // Now fix up any incomplete types...
    for(auto p : incomplete)
    {
	if (Types::TypeDecl* ty = GetTypeDecl(p->Name()))
	{
	    p->SetSubType(ty);
	}
	else
	{
	    Error("Forward declared pointer type not declared: " + p->Name());
	    return;
	}
    }
}

Types::EnumDecl* Parser::ParseEnumDef()
{
    AssertToken(Token::LeftParen);
    std::vector<std::string> values;
    while(!AcceptToken(Token::RightParen))
    {
	if (!Expect(Token::Identifier, false))
	{
	    return 0;
	}
	values.push_back(CurrentToken().GetIdentName());
	AssertToken(Token::Identifier);
	if (CurrentToken().GetToken() != Token::RightParen)
	{
	    if (!Expect(Token::Comma, true))
	    {
		return 0;
	    }
	}
    }
    Types::EnumDecl* ty = new Types::EnumDecl(values);
    for(auto v : ty->Values())
    {
	if (!nameStack.Add(v.name, new EnumDef(v.name, v.value, ty)))
	{
	    return reinterpret_cast<Types::EnumDecl*>
		(Error("Enumerated value by name " + v.name + " already exists..."));
	}
    }
    return ty;
}

Types::PointerDecl* Parser::ParsePointerType(bool maybeForwarded)
{
    assert((CurrentToken().GetToken() == Token::Uparrow || CurrentToken().GetToken() == Token::At)
	   && "Expected @ or ^ token...");
    NextToken();
    // If the name is an "identifier" then it's a name of a not yet declared type.
    // We need to forward declare it, and backpatch later.
    if (CurrentToken().GetToken() == Token::Identifier)
    {
	std::string name = CurrentToken().GetIdentName();
	AssertToken(Token::Identifier);
	if (!maybeForwarded)
	{
	    // Is it a known type?
	    if (Types::TypeDecl* ty = GetTypeDecl(name))
	    {
		return new Types::PointerDecl(ty);	
	    }
	    else
	    {
		return reinterpret_cast<Types::PointerDecl*>
		    (ErrorI("Unknown type '" + name + "' in pointer declaration"));
	    }
	}
	// Otherwise, forward declare...
	return new Types::PointerDecl(name);
    }

    return new Types::PointerDecl(ParseType("", false));
}

Types::ArrayDecl* Parser::ParseArrayDecl()
{
    AssertToken(Token::Array);
    if (!Expect(Token::LeftSquare, true))
    {
	return 0;
    }
    std::vector<Types::RangeDecl*> rv;
    Types::TypeDecl* type = NULL;
    while(!AcceptToken(Token::RightSquare))
    {
	if (Types::RangeDecl* r = ParseRangeOrTypeRange(type))
	{
	    assert(type && "Uh? Type is supposed to be set now");
	    rv.push_back(r);
	}
	else
	{
	    return 0;
	}
	AcceptToken(Token::Comma);
    }
    if (!Expect(Token::Of, true))
    {
	return 0;
    }
    if (Types::TypeDecl* ty = ParseType("", false))
    {
	return new Types::ArrayDecl(ty, rv);
    }
    return 0;
}

Types::VariantDecl* Parser::ParseVariantDecl(Types::TypeDecl*& type)
{
    Token::TokenType tt = Token::Unknown;
    std::vector<int> variantsSeen;
    std::vector<Types::FieldDecl*> variants;
    do
    {
	do
	{
	    int v = ParseConstantValue(tt, type);
	    if (tt == Token::Unknown)
	    {
		return 0;
	    }
	    if (std::find(variantsSeen.begin(), variantsSeen.end(), v) != variantsSeen.end())
	    {
		return reinterpret_cast<Types::VariantDecl*>
		    (Error("Value already used: " + std::to_string(v) + " in variant declaration"));
	    }
	    variantsSeen.push_back(v);
	    if (CurrentToken().GetToken() != Token::Colon)
	    {
		if (!Expect(Token::Comma, true))
		{
		    return 0;
		}
	    }
	} while (!AcceptToken(Token::Colon));
	if (!Expect(Token::LeftParen, true))
	{
	    return 0;
	}
	std::vector<Types::FieldDecl*> fields;
	do
	{
	    std::vector<std::string> names;
	    if (CurrentToken().GetToken() != Token::RightParen)
	    {
		do
		{
		    if (!Expect(Token::Identifier, false))
		    {
			return 0;
		    }
		    names.push_back(CurrentToken().GetIdentName());
		    AssertToken(Token::Identifier);
		    if (CurrentToken().GetToken() != Token::Colon && !Expect(Token::Comma, true))
		    {
			return 0;
		    }
		} while(!AcceptToken(Token::Colon));
		if (Types::TypeDecl* ty = ParseType("", false))
		{
		    for(auto n : names)
		    {
			for(auto f : fields)
			{
			    if (n == f->Name())
			    {
				return reinterpret_cast<Types::VariantDecl*>
				    (Error("Duplicate field name '" + n + "' in record"));
			    }
			}
			// Variants can't be static, can they?
			fields.push_back(new Types::FieldDecl(n, ty, false));
		    }
		}
		if (CurrentToken().GetToken() != Token::RightParen && !Expect(Token::Semicolon, true))
		{
		    TRACE();
		    return 0;
		}
	    }
	} while(!AcceptToken(Token::RightParen));
	if (!ExpectSemicolonOrEnd())
	{
	    return 0;
	}
	if (fields.size() == 1)
	{
	    variants.push_back(fields[0]);
	}
	else
	{
	    variants.push_back(new Types::FieldDecl("", new Types::RecordDecl(fields, 0), false));
	}
    } while (CurrentToken().GetToken() != Token::End);
    return new Types::VariantDecl(variants);
}


bool Parser::ParseFields(std::vector<Types::FieldDecl*>& fields, Types::VariantDecl*& variant,
			 Token::TokenType type)
{
    bool isClass = type == Token::Class;
    // Different from C++, public is the default access qualifier.
    Types::FieldDecl::Access access = Types::FieldDecl::Public;
    variant = 0;
    do
    {
	if (isClass && AcceptToken(Token::Private))
	{
	    access = Types::FieldDecl::Private;
	}
	if (isClass && AcceptToken(Token::Public))
	{
	    access = Types::FieldDecl::Public;
	}
	if (isClass && AcceptToken(Token::Protected))
	{
	    access = Types::FieldDecl::Protected;
	}

	std::vector<std::string> names;
	// Parse Variant part if we have a "case".
	if (AcceptToken(Token::Case))
	{
	    std::string marker = "";
	    Types::TypeDecl* markerTy;
	    if (CurrentToken().GetToken() == Token::Identifier &&
		PeekToken().GetToken() == Token::Colon)
	    {
		marker = CurrentToken().GetIdentName();
		AssertToken(Token::Identifier);
		AssertToken(Token::Colon);
	    }
	    markerTy = ParseType("", false);
	    if (!markerTy->IsIntegral())
	    {
		return (bool)Error("Expect variant selector to be integral type");
	    }
	    if (marker != "")
	    {
		fields.push_back(new Types::FieldDecl(marker, markerTy, false));
	    }
	    if (!Expect(Token::Of, true))
	    {
		return false;
	    }
	    Types::TypeDecl* type;
	    if (!(variant =  ParseVariantDecl(type)))
	    {
		return false;
	    }
	    if (*markerTy != *type)
	    {
		return (bool)Error("Marker type does not match member variant type");
	    }
	}
	else if (isClass &&
		 (CurrentToken().GetToken() == Token::Function ||
		  CurrentToken().GetToken() == Token::Procedure))
	{
	    PrototypeAST* p = ParsePrototype(false);
	    if (!Expect(Token::Semicolon, true))
	    {
		return false;
	    }
	    int f = 0;
	    if (AcceptToken(Token::Static))
	    {
		f |= Types::MemberFuncDecl::Static;
		if (!Expect(Token::Semicolon, true))
		{
		    return false;
		}
	    }
	    if (AcceptToken(Token::Virtual))
	    {
		f |= Types::MemberFuncDecl::Virtual;
		if (!Expect(Token::Semicolon, true))
		{
		    return false;
		}
	    }
	    if (AcceptToken(Token::Override))
	    {
		f |= Types::MemberFuncDecl::Override;
		if (!Expect(Token::Semicolon, true))
		{
		    return false;
		}
	    }
	    Types::MemberFuncDecl* m = new Types::MemberFuncDecl(p, f);
	    fields.push_back(new Types::FieldDecl(p->Name(), m, false));
	}
	else
	{
	    /* Cope with empty classes - but not empty Record! */
	    if (!isClass || CurrentToken().GetToken() != Token::End)
	    {
		do
		{
		    if (!Expect(Token::Identifier, false))
		    {
			return false;
		    }
		    names.push_back(CurrentToken().GetIdentName());
		    AssertToken(Token::Identifier);
		    if (CurrentToken().GetToken() != Token::Colon && !Expect(Token::Comma, true))
		    {
			return false;
		    }
		} while(!AcceptToken(Token::Colon));
		assert(names.size() != 0 && "Should have some names here...");
		if (Types::TypeDecl* ty = ParseType("", false))
		{
		    for(auto n : names)
		    {
			for(auto f : fields)
			{
			    if (n == f->Name())
			    {
				return (bool)Error("Duplicate field name '" + n + "' in record");
			    }
			}
			bool isStatic = false;
			if (isClass &&
			    CurrentToken().GetToken() == Token::Semicolon &&
			    PeekToken().GetToken() == Token::Static)
			{
			    isStatic = true;
			    AssertToken(Token::Semicolon);
			    AssertToken(Token::Static);
			}
			fields.push_back(new Types::FieldDecl(n, ty, isStatic, access));
		    }
		}
		else
		{
		    return false;
		}
		if (!ExpectSemicolonOrEnd())
		{
		    return false;
		}
	    }
	}
    } while(!AcceptToken(Token::End));
    return true;
}

Types::RecordDecl* Parser::ParseRecordDecl()
{
    AssertToken(Token::Record);
    std::vector<Types::FieldDecl*> fields;
    Types::VariantDecl* variant;
    if (!ParseFields(fields, variant, Token::Record))
    {
	return 0;
    }
    if (fields.size() == 0 && !variant)
    {
	return reinterpret_cast<Types::RecordDecl*>(Error("No elements in record declaration"));
    }
    return new Types::RecordDecl(fields, variant);
}

Types::FileDecl* Parser::ParseFileDecl()
{
    AssertToken(Token::File);
    if (!Expect(Token::Of, true))
    {
	return 0;
    }

    if (Types::TypeDecl* type = ParseType("", false))
    {
	return new Types::FileDecl(type);
    }
    return 0;
}

Types::SetDecl* Parser::ParseSetDecl()
{
    TRACE();
    AssertToken(Token::Set);
    if (!Expect(Token::Of, true))
    {
	return 0;
    }

    Types::TypeDecl* type;
    if (Types::RangeDecl* r = ParseRangeOrTypeRange(type))
    {
	assert(type && "Uh? Type is supposed to be set");
	return new Types::SetDecl(r, type);
    }
    return 0;
}

unsigned Parser::ParseStringSize()
{
	Token token = TranslateToken(CurrentToken());

	if (token.GetToken() != Token::Integer)
	{
	    return reinterpret_cast<uintptr_t>(Error("Expected integer value!"));
	}
	NextToken();
	return token.GetIntVal();
}


Types::StringDecl* Parser::ParseStringDecl()
{
    TRACE();
    AssertToken(Token::String);
    unsigned size = 255;

    if (AcceptToken(Token::LeftSquare))
    {
	size = ParseStringSize();
	if (!Expect(Token::RightSquare, true))
	{
	    return 0;
	}
    }
    else if (AcceptToken(Token::LeftParen))
    {
	size = ParseStringSize();
	if (!Expect(Token::RightParen, true))
	{
	    return 0;
	}
    }
    if (size == 0)
    {
	return 0;
    }
    return new Types::StringDecl(size);
}

Types::ClassDecl* Parser::ParseClassDecl(const std::string &name)
{
    TRACE();
    Location loc = CurrentToken().Loc();
    AssertToken(Token::Class);
    Types::ClassDecl* base = 0;
    // Find derived class, if available.
    if (AcceptToken(Token::LeftParen))
    {
	if (!Expect(Token::Identifier, false))
	{
	    return 0;
	}
	std::string baseName = CurrentToken().GetIdentName();
	if (!(base = llvm::dyn_cast_or_null<Types::ClassDecl>(GetTypeDecl(baseName))))
	{
	    return reinterpret_cast<Types::ClassDecl*>(Error("Expected class as base"));
	}
	AssertToken(Token::Identifier);
	if (!Expect(Token::RightParen, true))
	{
	    return 0;
	}
    }

    std::vector<Types::FieldDecl*> fields;
    Types::VariantDecl* variant;
    if (!ParseFields(fields, variant, Token::Class))
    {
	return 0;
    }

    std::vector<Types::MemberFuncDecl*> mf;
    std::vector<VarDef> vars;
    bool needVtable = false;
    for(auto f = fields.begin(); f != fields.end(); )
    {
	if (Types::MemberFuncDecl* m = llvm::dyn_cast<Types::MemberFuncDecl>((*f)->FieldType()))
	{
	    mf.push_back(m);
	    if (m->IsVirtual() || m->IsOverride())
	    {
		needVtable = true;
	    }
	    f = fields.erase(f);
	}
	else
	{
	    if ((*f)->IsStatic())
	    {
		std::string vname = name + "$" + (*f)->Name();
		vars.push_back(VarDef(vname, (*f)->FieldType()));
	    }
	    f++;
	}
    }
    if (vars.size())
    {
	ast.push_back(new VarDeclAST(loc, vars));
    }

    auto cd = new Types::ClassDecl(name, fields, mf, variant, base);
    // For now, we generate vtable whether we need it or not.
    if (needVtable)
    {
	ast.push_back(new VTableAST(loc, cd));
    }
    return cd;
}

Types::TypeDecl* Parser::ParseType(const std::string& name, bool maybeForwarded)
{
    TRACE();
    Token::TokenType tt = CurrentToken().GetToken();
    if (tt == Token::Packed)
    {
	tt = NextToken().GetToken();
	if (tt != Token::Array && tt != Token::Record && tt != Token::Set && tt != Token::File)
	{
	    return ErrorT("Expected 'array', 'record', 'file' or 'set' after 'packed'");
	}
    }

    Token token = TranslateToken(CurrentToken());
    tt = token.GetToken();

    switch(tt)
    {
    case Token::Identifier:
    {
	if (!GetEnumValue(CurrentToken().GetIdentName()))
	{
	    return ParseSimpleType();
	}
    }
    // Fall through:
    case Token::Integer:
    case Token::Char:
    case Token::Minus:
    {
	Types::TypeDecl*  type = NULL;
	if (Types::RangeDecl* r = ParseRange(type))
	{
	    return r;
	}
	return 0;
    }

    case Token::Array:
	return ParseArrayDecl();

    case Token::Record:
	return ParseRecordDecl();

    case Token::Class:
	return ParseClassDecl(name);

    case Token::File:
	return ParseFileDecl();

    case Token::Set:
	return ParseSetDecl();

    case Token::LeftParen:
	return ParseEnumDef();

    case Token::Uparrow:
    case Token::At:
	return ParsePointerType(maybeForwarded);

    case Token::String:
	return ParseStringDecl();

    case Token::Procedure:
    case Token::Function:
    {
	PrototypeAST* proto = ParsePrototype(true);
	return new Types::FuncPtrDecl(proto);
    }

    default:
	return ErrorT("Can't understand type");
    }
}

ExprAST* Parser::ParseIntegerExpr(Token token)
{
    int64_t val = token.GetIntVal();
    Location loc = token.Loc();
    const char* type = "integer";

    if (val > std::numeric_limits<unsigned int>::max())
    {
	type = "longint";
    }
    NextToken();
    return new IntegerExprAST(loc, val, GetTypeDecl(type));
}

ExprAST* Parser::ParseIntegerOrLabel(Token token)
{
    if (PeekToken().GetToken() == Token::Colon)
    {
	AssertToken(Token::Integer);
	AssertToken(Token::Colon);
	int n = token.GetIntVal();
	if (!nameStack.FindTopLevel(std::to_string(n)))
	{
	    return Error("Can't use label in a different scope than the declaration");
	}
	return new LabelExprAST(token.Loc(), { (int)token.GetIntVal() }, NULL);
    }
    return ParseIntegerExpr(token);
}

ExprAST* Parser::ParseStringExpr(Token token)
{
    int len =  std::max(1, (int)(token.GetStrVal().length()-1));
    std::vector<Types::RangeDecl*> rv = {new Types::RangeDecl(new Types::Range(0, len), 
							      Types::TypeDecl::TK_Integer)};
    Types::ArrayDecl *ty = new Types::ArrayDecl(GetTypeDecl("char"), rv);
    NextToken();
    return new StringExprAST(token.Loc(), token.GetStrVal(), ty);
}

void Parser::ParseLabels()
{
    AssertToken(Token::Label);
    std::vector<int> labels;
    do
    {
	if (CurrentToken().GetToken() == Token::Integer)
	{
	    int n = CurrentToken().GetIntVal();
	    if (!nameStack.Add(std::to_string(n), new LabelDef(n)))
	    {
		Error("Multiple label defintions?");
		return;
	    }
	    NextToken();
	}
	AcceptToken(Token::Comma);
    } while(!AcceptToken(Token::Semicolon));
}

ExprAST* Parser::ParseBinOpRHS(int exprPrec, ExprAST* lhs)
{
    for(;;)
    {
	Token binOp = CurrentToken();
	int tokPrec = binOp.Precedence();
	if (tokPrec < exprPrec)
	{
	    return lhs;
	}

	NextToken();
	
	ExprAST* rhs = ParsePrimary();
	if (!rhs)
	{
	    return 0;
	}

	// If the new operator binds less tightly, take it as LHS of
	// the next operator.
	int nextPrec = CurrentToken().Precedence();
	if (tokPrec < nextPrec)
	{
	    if (!(rhs = ParseBinOpRHS(tokPrec+1, rhs)))
	    {
		return 0;
	    }
	}

	// Now combine to new lhs.
	lhs = new BinaryExprAST(binOp, lhs, rhs);
    }
}

ExprAST* Parser::ParseUnaryOp()
{
    assert((CurrentToken().GetToken() == Token::Minus ||
	    CurrentToken().GetToken() == Token::Plus ||
	    CurrentToken().GetToken() == Token::Not) &&
	   "Expected only minus at this time as a unary operator");

    Token oper = CurrentToken();
    NextToken();

    if (ExprAST* rhs = ParsePrimary())
    {
	// unary + = no change, so just return the expression.
	if (oper.GetToken() == Token::Plus)
	{
	    return rhs;
	}
	return new UnaryExprAST(oper.Loc(), oper, rhs);
    }
    return 0;
}

ExprAST* Parser::ParseExpression()
{
    if (ExprAST* lhs = ParsePrimary())
    {
	return ParseBinOpRHS(0, lhs);
    }
    return 0;
}

VariableExprAST* Parser::ParseArrayExpr(VariableExprAST* expr, Types::TypeDecl*& type)
{
    Types::ArrayDecl* adecl = llvm::dyn_cast<Types::ArrayDecl>(type);
    if (!adecl)
    {
	return ErrorV("Expected variable of array type when using index");
    }
    NextToken();
    std::vector<ExprAST*> indices;
    while(!AcceptToken(Token::RightSquare))
    {
	ExprAST* index = ParseExpression();
	if (!index)
	{
	    return ErrorV("Expected index expression");
	}
	indices.push_back(index);
	if (indices.size() == adecl->Ranges().size())
	{
	    expr = new ArrayExprAST(CurrentToken().Loc(), expr, indices, adecl->Ranges(), adecl->SubType());
	    indices.clear();
	    type = adecl->SubType();
	    adecl = llvm::dyn_cast<Types::ArrayDecl>(type);
	}
	if (CurrentToken().GetToken() == Token::RightSquare && PeekToken().GetToken() == Token::LeftSquare)
	{
	    AssertToken(Token::RightSquare);
	    AssertToken(Token::LeftSquare);
	}
	else if (!AcceptToken(Token::Comma))
	{
	    if (CurrentToken().GetToken() != Token::RightSquare && !adecl)
	    {
		return 0;
	    }
	}
    }
    if (indices.size())
    {
	expr = new ArrayExprAST(CurrentToken().Loc(), expr, indices, adecl->Ranges(), adecl->SubType());
	type = adecl->SubType();
    }
    return expr;
}

ExprAST* Parser::MakeCallExpr(VariableExprAST* self, NamedObject* def, const std::string& funcName,
			      std::vector<ExprAST*>& args)
{
    TRACE();

    const PrototypeAST* proto = 0;
    ExprAST* expr = 0;
    if (const FuncDef *funcDef = llvm::dyn_cast_or_null<const FuncDef>(def))
    {
	proto = funcDef->Proto();
	expr = new FunctionExprAST(CurrentToken().Loc(), funcName, funcDef->Type());
    }
    else if (llvm::dyn_cast_or_null<const VarDef>(def))
    {
	if (def->Type()->Type() == Types::TypeDecl::TK_FuncPtr)
	{
	    if (Types::FuncPtrDecl* fp = llvm::dyn_cast<Types::FuncPtrDecl>(def->Type()))
	    {
		proto = fp->Proto();
		expr = new VariableExprAST(CurrentToken().Loc(), funcName, def->Type());
	    }
	    else
	    {
		return Error("Expected function pointer");;
	    }
	}
    }
    else if (MembFuncDef* m = llvm::dyn_cast_or_null<MembFuncDef>(def))
    {
	Types::ClassDecl* cd = llvm::dyn_cast<Types::ClassDecl>(m->Type());
	Types::MemberFuncDecl* mf = cd->GetMembFunc(m->Index());
	proto = mf->Proto();
	// Make sure we enumerate the index of virtual functions
	if (!self && proto->HasSelf())
	{
	    self = new VariableExprAST(CurrentToken().Loc(), "self", cd);
	}
	(void) cd->VTableType(true);
	if (mf->IsVirtual() || mf->IsOverride())
	{
	    int index = mf->VirtIndex();
	    expr = new VirtFunctionAST(CurrentToken().Loc(), self, index, mf->Proto()->Type());
	}
	else
	{
	    std::string fname = mf->LongName();
	    expr = new FunctionExprAST(CurrentToken().Loc(), fname, mf->Proto()->Type());
	}
    }
    if (expr)
    {
	if (proto->HasSelf())
	{
	    assert(self && "Should have a 'self' expression here");
	    args.insert(args.begin(), self);
	}
	if (FunctionAST* fn = proto->Function())
	{
	    AddClosureArg(fn, args);
	}
	return new CallExprAST(CurrentToken().Loc(), expr, args, proto);
    }
    return 0;
}

ExprAST* Parser::ParseFieldExpr(VariableExprAST* expr, Types::TypeDecl*& type)
{
    AssertToken(Token::Period);
    if (!Expect(Token::Identifier, false))
    {
	return 0;
    }

    std::string typedesc;
    std::string name = CurrentToken().GetIdentName();
    VariableExprAST* e = 0;
    Types::VariantDecl* v = 0;
    unsigned fc = 0;
    if (Types::ClassDecl* cd = llvm::dyn_cast<Types::ClassDecl>(type))
    {
	int elem = cd->Element(name);
	typedesc = "object";
	if (elem >= 0)
	{
	    std::string objname;
	    const Types::FieldDecl* fd = cd->GetElement(elem, objname);
	    
	    type = fd->FieldType();
	    if (fd->IsStatic())
	    {
		std::string vname = objname + "$" + fd->Name();
		e = new VariableExprAST(CurrentToken().Loc(), vname, type);
	    }
	    else
	    {
		e = new FieldExprAST(CurrentToken().Loc(), expr, elem, type);
	    }
	}
	else
	{
 	    if ((elem = cd->MembFunc(name)) >= 0)
	    {
		Types::MemberFuncDecl* membfunc = cd->GetMembFunc(elem);
		NamedObject* def = nameStack.Find(membfunc->LongName());

		std::vector<ExprAST* > args;
		NextToken();

		if (!ParseArgs(def, args))
		{
		    return 0;
		}

		if (ExprAST* call = MakeCallExpr(expr, def, membfunc->LongName(), args))
		{
		    return call;
		}
		return 0;
	    }
	    else
	    {
		fc = cd->FieldCount();
		v = cd->Variant();
	    }
	}
    }
    else if (Types::RecordDecl* rd = llvm::dyn_cast<Types::RecordDecl>(type))
    {
	typedesc = "record";
	int elem = rd->Element(name);
	if (elem >= 0)
	{
	    type = rd->GetElement(elem)->FieldType();
	    e = new FieldExprAST(CurrentToken().Loc(), expr, elem, type);
	}
	else
	{
	    fc = rd->FieldCount();
	    v = rd->Variant();
	}
    }
    else
    {
	return ErrorV("Attempt to use filed of variable that hasn't got fields");
    }
    if (!e && v)
    {
	int elem = v->Element(name);
	if (elem >= 0)
	{
	    const Types::FieldDecl* fd = v->GetElement(elem);
	    type = fd->FieldType();
	    e = new VariantFieldExprAST(CurrentToken().Loc(), expr, fc, type);
	    // If name is empty, we have a made up struct. Dig another level down.
	    if (fd->Name() == "")
	    {
		Types::RecordDecl* r = llvm::dyn_cast<Types::RecordDecl>(fd->FieldType());
		assert(r && "Expect record declarataion");
		elem = r->Element(name);
		if (elem >= 0)
		{
		    type = r->GetElement(elem)->FieldType();
		    e = new FieldExprAST(CurrentToken().Loc(), e, elem, type);
		}
		else
		{
		    e = 0;
		}
	    }
	}
    }
    if (!e)
    {
	return ErrorV("Can't find element " + name + " in " + typedesc);
    }
    NextToken();
    return e;
}

VariableExprAST* Parser::ParsePointerExpr(VariableExprAST* expr, Types::TypeDecl*& type)
{
    assert((CurrentToken().GetToken() == Token::Uparrow || CurrentToken().GetToken() == Token::At) 
	   && "Expected @ or ^ token...");
    NextToken();
    if (llvm::isa<Types::FileDecl>(type))
    {
	type = type->SubType();
	return new FilePointerExprAST(CurrentToken().Loc(), expr, type);
    }
    type = type->SubType();
    return new PointerExprAST(CurrentToken().Loc(), expr, type);
}

bool Parser::IsCall(const NamedObject* def)
{
    assert(def && "Expected def to be non-NULL");

    Types::TypeDecl* type = def->Type();
    Types::TypeDecl::TypeKind ty = type->Type();
    if (ty == Types::TypeDecl::TK_FuncPtr)
    {
	return true;
    }
    if (ty == Types::TypeDecl::TK_Class && llvm::isa<MembFuncDef>(def))
    {
	if (CurrentToken().GetToken() != Token::Assign)
	{
	    return true;
	}
    }
    if ((ty == Types::TypeDecl::TK_Function || ty == Types::TypeDecl::TK_MemberFunc) &&
	CurrentToken().GetToken() != Token::Assign)
    {
	return true;
    }
    return false;
}

bool Parser::ParseArgs(const NamedObject* def, std::vector<ExprAST*>& args)
{
    TRACE();

    if (AcceptToken(Token::LeftParen))
    {
	unsigned argNo = 0;
	const PrototypeAST* proto = 0;
	if (const FuncDef* funcDef = llvm::dyn_cast_or_null<FuncDef>(def))
	{
	    proto = funcDef->Proto();
	}
	else if (const VarDef* varDef = llvm::dyn_cast_or_null<VarDef>(def))
	{
	    if (const Types::FuncPtrDecl* fp = llvm::dyn_cast<Types::FuncPtrDecl>(varDef->Type()))
	    {
		proto = fp->Proto();
	    }
	}
	while (!AcceptToken(Token::RightParen))
	{
	    bool isFuncArg = false;
	    if (proto)
	    {
		auto& funcArgs = proto->Args();
		if (argNo >= funcArgs.size())
		{
		    return (bool)Error("Too many arguments");
		}
		Types::TypeDecl* td = funcArgs[argNo].Type();
		if (td->Type() == Types::TypeDecl::TK_FuncPtr)
		{
		    isFuncArg = true;
		}
	    }
	    ExprAST* arg = 0;
	    if (isFuncArg)
	    {
		Token token = CurrentToken();
		std::string idName = token.GetIdentName();
		AssertToken(Token::Identifier);
		if (NamedObject* argDef = nameStack.Find(idName))
		{
		    if (FuncDef *fd = llvm::dyn_cast<FuncDef>(argDef))
		    {
			arg = new FunctionExprAST(CurrentToken().Loc(), idName, fd->Type());
		    }
		    else if (VarDef* vd = llvm::dyn_cast<VarDef>(argDef))
		    {
			if (vd->Type()->Type() == Types::TypeDecl::TK_FuncPtr)
			{
			    arg = new VariableExprAST(CurrentToken().Loc(), idName, argDef->Type());
			}
		    }
		}
		if (!arg)
		{
		    return false;
		}
	    }
	    else
	    {
		arg = ParseExpression();
	    }
	    if (!arg)
	    {
		return false;
	    }
	    args.push_back(arg);
	    if (!AcceptToken(Token::Comma) && !Expect(Token::RightParen, false))
	    {
		return false;
	    }
	    argNo++;
	}
    }
    return true;
}

VariableExprAST* Parser::ParseStaticMember(TypeDef* def, Types::TypeDecl*& type)
{
    if (!Expect(Token::Period, true) && !Expect(Token::Identifier, false))
    {
	return 0;
    }
    std::string field = CurrentToken().GetIdentName();
    AssertToken(Token::Identifier);

    Types::ClassDecl* od = llvm::dyn_cast<Types::ClassDecl>(def->Type());
    int elem;
    if ((elem = od->Element(field)) >= 0)
    {
	std::string objname;
	const Types::FieldDecl* fd = od->GetElement(elem, objname);
	if (fd->IsStatic())
	{
	    type = fd->FieldType();
	    std::string name = objname + "$" + field;
	    return new VariableExprAST(CurrentToken().Loc(), name, type);
	}
	return ErrorV("Expected static variable '" + field + "'");
    }
    return ErrorV("Expected member variabe name '" + field + "'");
}

ExprAST* Parser::ParseIdentifierExpr()
{
    TRACE();

    Token token = TranslateToken(CurrentToken());
    std::string idName = token.GetIdentName();
    AssertToken(Token::Identifier);
    NamedObject* def = nameStack.Find(idName);
    if (EnumDef *enumDef = llvm::dyn_cast_or_null<EnumDef>(def))
    {
	return new IntegerExprAST(token.Loc(), enumDef->Value(), enumDef->Type());
    }
    bool isBuiltin = false;
    if (!def && !(isBuiltin = Builtin::IsBuiltin(idName)))
    {
	return Error("Undefined name '" + idName + "'");
    }
    if (def)
    {
	Types::TypeDecl* type = def->Type();
	assert(type && "Expect type here...");

	if (!IsCall(def))
	{
	    VariableExprAST* expr = 0;
	    if (WithDef *w = llvm::dyn_cast<WithDef>(def))
	    {
		expr = llvm::dyn_cast<VariableExprAST>(w->Actual());
	    }
	    else
	    {
		if (MembFuncDef* m = llvm::dyn_cast<MembFuncDef>(def))
		{
		    Types::ClassDecl* od = llvm::dyn_cast<Types::ClassDecl>(type);
		    Types::MemberFuncDecl* mf = od->GetMembFunc(m->Index());
		    type = mf->Proto()->Type();
		}
		else if (TypeDef* ty = llvm::dyn_cast<TypeDef>(def))
		{
		    if ((ty->Type()->Type() == Types::TypeDecl::TK_Class))
		    {
			expr = ParseStaticMember(ty, type);
		    }
		}
		if (!expr)
		{
		    expr = new VariableExprAST(CurrentToken().Loc(), idName, type);
		    // Only add defined variables.
		    // Ignore result - we may be adding the same variable
		    // several times, but we don't really care.
		    usedVariables.Add(idName, def);
		}
	    }
	    assert(expr && "Expected expression here");
	    assert(type && "Type is supposed to be set here");

	    Token::TokenType tt = CurrentToken().GetToken();
	    while(tt == Token::LeftSquare || tt == Token::Uparrow || tt == Token::At || tt == Token::Period)
	    {
		assert(type && "Expect to have a type here...");
		switch(tt)
		{
		case Token::LeftSquare:
		    expr = ParseArrayExpr(expr, type);
		    break;

		case Token::Uparrow:
		case Token::At:
		    expr = ParsePointerExpr(expr, type);
		    break;

		case Token::Period:
		    if (ExprAST* tmp = ParseFieldExpr(expr, type))
		    {
			if (VariableExprAST* v = llvm::dyn_cast<VariableExprAST>(tmp))
			{
			    expr = v;
			}
			else
			{
			    return tmp;
			}
		    }
		    break;

		default:
		    assert(0);
		}
		tt = CurrentToken().GetToken();
	    }
	    return expr;	
	}
    }

    std::vector<ExprAST* > args;
    if (!ParseArgs(def, args))
    {
	return 0;
    }

    if (ExprAST* expr = MakeCallExpr(NULL, def, idName, args))
    {
	return expr;
    }

    assert(isBuiltin && "Should be a builtin function if we get here");

    if (Builtin::BuiltinFunctionBase* bif = Builtin::CreateBuiltinFunction(idName, args))
    {
	return new BuiltinExprAST(CurrentToken().Loc(), bif);
    }
    assert(0 && "Should not get here");
    return 0;
}

ExprAST* Parser::ParseParenExpr()
{
    AssertToken(Token::LeftParen);
    ExprAST* v;
    if ((v = ParseExpression()) && Expect(Token::RightParen, true))
    {
	return v;
    }
    return 0;
}

ExprAST* Parser::ParseSetExpr()
{
    TRACE();
    AssertToken(Token::LeftSquare);

    Location loc = CurrentToken().Loc();
    std::vector<ExprAST*> values;
    while(!AcceptToken(Token::RightSquare))
    {
	ExprAST* v = ParseExpression();
	if (!v)
	{
	    return 0;
	}
	if (AcceptToken(Token::DotDot))
	{
	    ExprAST* vEnd = ParseExpression();
	    v = new RangeExprAST(loc, v, vEnd);
	}
	values.push_back(v);
	if (CurrentToken().GetToken() != Token::RightSquare && !Expect(Token::Comma, true))
	{
	    return 0;
	}
    } 
    Types::TypeDecl* type = NULL;
    if (!values.empty())
    {
	type = values[0]->Type();
    }
    return new SetExprAST(loc, values, new Types::SetDecl(NULL, type));
}

VarDeclAST* Parser::ParseVarDecls()
{
    TRACE();
    AssertToken(Token::Var);

    std::vector<VarDef> varList;
    std::vector<std::string> names;
    do
    {
	// Don't move forward here.
	if (!Expect(Token::Identifier, false))
	{
	    return 0;
	}
	names.push_back(CurrentToken().GetIdentName());
	AssertToken(Token::Identifier);
	if (AcceptToken( Token::Colon))
	{
	    if (Types::TypeDecl* type = ParseType("", false))
	    {
		for(auto n : names)
		{
		    VarDef v(n, type);
		    varList.push_back(v);
		    if (!nameStack.Add(n, new VarDef(n, type)))
		    {
			return reinterpret_cast<VarDeclAST*>(Error("Name '"+ n +"' is already defined"));
		    }
		}
		if (!Expect(Token::Semicolon, true))
		{
		    return 0;
		}
		names.clear();
	    }
	    else
	    {
		return 0;
	    }
	}
	else
	{
	    if (!Expect(Token::Comma, true))
	    {
		return 0;
	    }
	}
    } while(CurrentToken().GetToken() == Token::Identifier);

    return new VarDeclAST(CurrentToken().Loc(), varList);
}

// functon name( { [var] name1, [,name2 ...]: type [; ...] } ) : type
// procedure name ( { [var] name1 [,name2 ...]: type [; ...] } )
// member function/procedure:
//   function classname.name{( args... )}: type;
//   procedure classname.name{ args... }
PrototypeAST* Parser::ParsePrototype(bool unnamed)
{
    assert((CurrentToken().GetToken() == Token::Procedure ||
	    CurrentToken().GetToken() == Token::Function) &&
	   "Expected function or procedure token");

    bool isFunction = CurrentToken().GetToken() == Token::Function;
    PrototypeAST* fwdProto = 0;
    Types::ClassDecl* od = 0;
    Types::MemberFuncDecl* membfunc = 0;

    // Consume "function" or "procedure"
    NextToken();
    std::string funcName = "noname";
    if (!unnamed)
    {

	if (!Expect(Token::Identifier, false))
	{
	    return 0;
	}
	// Get function name.
	funcName = CurrentToken().GetIdentName();
	AssertToken(Token::Identifier);

	// Is it a member function?
	// FIXME: Nested classes, should we do this again?
	if (AcceptToken(Token::Period))
	{
	    if (Types::TypeDecl* ty = GetTypeDecl(funcName))
	    {
		if ((od = llvm::dyn_cast<Types::ClassDecl>(ty)))
		{
		    if (!Expect(Token::Identifier, false))
		    {
			return 0;
		    }
		    std::string m = CurrentToken().GetIdentName();
		    AssertToken(Token::Identifier);
		    int elem;

		    if ((elem = od->MembFunc(m)) >= 0)
		    {
			membfunc = od->GetMembFunc(elem);
		    }
		    if (!membfunc)
		    {
			return ErrorP("Member function '" + m + "' not found in '"  + funcName + "'.");
		    }
		    funcName = membfunc->LongName();
		}
	    }
	    if (!od)
	    {
		return ErrorP("Expected object name");
	    }
	}
	// See if it's a "forward declaration"?
	if (const NamedObject* def = nameStack.Find(funcName))
	{
	    const FuncDef *fnDef = llvm::dyn_cast_or_null<const FuncDef>(def);
	    if (fnDef && fnDef->Proto() && fnDef->Proto()->IsForward())
	    {
		fwdProto = fnDef->Proto();
		fwdProto->SetIsForward(false);
		// If this is a semicolon, then return current prototype, and be done with it.
		if (CurrentToken().GetToken() == Token::Semicolon)
		{
		    return fwdProto;
		}
	    }
	}
    }

    std::vector<VarDef> args;
    if (AcceptToken(Token::LeftParen))
    {
	std::vector<std::string> names;
	bool isRef = false;
	while(!AcceptToken(Token::RightParen))
	{
	    if (CurrentToken().GetToken() == Token::Function ||
		CurrentToken().GetToken() == Token::Procedure)
	    {
		PrototypeAST* proto = ParsePrototype(false);
		Types::TypeDecl* type = new Types::FuncPtrDecl(proto);
		VarDef v(proto->Name(), type, false);
		args.push_back(v);
		if (CurrentToken().GetToken() != Token::RightParen &&
		    !Expect(Token::Semicolon, true))
		{
		    return 0;
		}
	    }
	    else
	    {
		if (AcceptToken(Token::Var))
		{
		    isRef = true;
		}
		if (!Expect(Token::Identifier, false))
		{
		    return 0;
		}
		std::string arg = CurrentToken().GetIdentName();
		NextToken();

		names.push_back(arg);
		if (AcceptToken(Token::Colon))
		{
		    if (Types::TypeDecl* type = ParseType("", false))
		    {
			for(auto n : names)
			{
			    VarDef v(n, type, isRef);
			    args.push_back(v);
			}
			isRef = false;
			names.clear();
			if (CurrentToken().GetToken() != Token::RightParen &&
			    !Expect(Token::Semicolon, true))
			{
			    return 0;
			}
		    }
		    else
		    {
			return 0;
		    }
		}
		else
		{
		    if (!Expect(Token::Comma, true))
		    {
			return 0;
		    }
		}
	    }
	}
    }

    Types::TypeDecl* resultType;
    // If we have a function, expect ": type".
    if (isFunction)
    {
	if (!Expect(Token::Colon, true))
	{
	    return 0;
	}
	if (!(resultType = ParseSimpleType()))
	{
	    return 0;
	}
    }
    else
    {
	resultType = Types::GetVoidType();
    }

    if (fwdProto)
    {
	if (*resultType != *fwdProto->Type())
	{
	    return ErrorP("Forward declared function should have same return type as definition");
	}
	// Todo: Check argument types...
	return fwdProto;
    }

    PrototypeAST* proto = new PrototypeAST(CurrentToken().Loc(), funcName, args, resultType, od);
    if (od && !membfunc->IsStatic())
    {
	std::vector<VarDef> v{VarDef("self", od, true)};
	proto->AddExtraArgsFirst(v);
	proto->SetHasSelf(true);
	
    }
    return proto;
}

ExprAST* Parser::ParseStatement()
{
    switch(CurrentToken().GetToken())
    {
    case Token::Begin:
    {
	Location dummy;
	return ParseBlock(dummy);
    }

    case Token::Semicolon:
    case Token::End:
	// Empty block.
	return new BlockAST(CurrentToken().Loc(), { });

    default:
	if (ExprAST* expr = ParsePrimary())
	{
	    if (AcceptToken(Token::Assign))
	    {
		Location loc = CurrentToken().Loc();
		ExprAST* rhs = ParseExpression();
		if (rhs)
		{
		    expr = new AssignExprAST(loc, expr, rhs);
		}
		else
		{
		    // TODO: Error recovery.
		    return 0;
		}
	    }
	    return expr;
	}
	break;
    }
    // TODO: Error recovery.
    return 0;
}

BlockAST* Parser::ParseBlock(Location& endLoc)
{
    TRACE();
    AssertToken(Token::Begin);

    std::vector<ExprAST*> v;
    // Build ast of the content of the block.
    Location loc = CurrentToken().Loc();
    while(!AcceptToken(Token::End))
    {
	/* Superfluous semicolons are discarded here */
	AcceptToken(Token::Semicolon);
	if (CurrentToken().GetToken() == Token::Integer &&
	    PeekToken().GetToken() == Token::Colon)
	{
	    Token token = CurrentToken();
	    AcceptToken(Token::Integer);
	    AcceptToken(Token::Colon);
	    int n = token.GetIntVal();
	    if (!nameStack.FindTopLevel(std::to_string(n)))
	    {
		return reinterpret_cast<BlockAST*>
		    (Error("Can't use label in a different scope than the declaration"));
	    }
	    v.push_back(new LabelExprAST(token.Loc(), { n }, 0));
	}
	else if (ExprAST* ast = ParseStatement())
	{
	    v.push_back(ast);
	    if (!ExpectSemicolonOrEnd())
	    {
		return 0;
	    }
	}
	else
	{
	    return 0;
	}
	endLoc = CurrentToken().Loc();
    }
    return new BlockAST(loc, v);
}

FunctionAST* Parser::ParseDefinition(int level)
{
    Location loc = CurrentToken().Loc();
    PrototypeAST* proto = ParsePrototype(false);
    if (!proto || !Expect(Token::Semicolon, true))
    {
	return 0;
    }

    std::string name = proto->Name();
    NamedObject* nmObj = 0;

    const NamedObject* def = nameStack.Find(name);
    const FuncDef *fnDef = llvm::dyn_cast_or_null<const FuncDef>(def);
    std::string shortname;
    if (!(fnDef && fnDef->Proto() && fnDef->Proto() == proto))
    {
	shortname = ShortName(name);
	if (Types::ClassDecl* cd = proto->BaseObj())
	{
	    int elem = cd->MembFunc(shortname);
	    if (elem < 0)
	    {
		return ErrorF("Name '" + shortname + "' doesn't appear to be a member function...");
	    }
	    nmObj = new MembFuncDef(shortname, elem, proto->BaseObj());
	}
	else
	{
	    Types::TypeDecl* ty = new Types::FunctionDecl(proto);
	    nmObj = new FuncDef(name, ty, proto);
	    if (llvm::isa<Types::VoidDecl>(proto->Type()))
	    {
		shortname = "";
	    }
	}
	if (!nameStack.Add(name, nmObj))
	{
	    return ErrorF("Name '" + name + "' already exists...");
	}
	if (AcceptToken(Token::Forward))
	{
	    proto->SetIsForward(true);
	    return new FunctionAST(CurrentToken().Loc(), proto, {}, 0);
	}
    }

    NameWrapper wrapper(nameStack);
    NameWrapper usedWrapper(usedVariables);
    if (proto->HasSelf())
    {
	VariableExprAST* v = new VariableExprAST(Location("", 0, 0), "self", proto->BaseObj());
	ExpandWithNames(proto->BaseObj(), v, 0);
    }

    if (shortname != "")
    {
	assert(nmObj);
	nameStack.Add(shortname, nmObj);
    }

    for(auto v : proto->Args())
    {
	if (!nameStack.Add(v.Name(), new VarDef(v.Name(), v.Type())))
	{
	    return ErrorF("Duplicate name " + v.Name() + " (1)");
	}
    }

    std::vector<VarDeclAST*> varDecls;
    BlockAST* body = 0;
    std::vector<FunctionAST*> subFunctions;
    for(;;)
    {
	switch(CurrentToken().GetToken())
	{
	case Token::Var:
	    if (VarDeclAST* v = ParseVarDecls())
	    {
		varDecls.push_back(v);
	    }
	    else
	    {
		return 0;
	    }
	    break;

	case Token::Label:
	    ParseLabels();
	    break;

	case Token::Type:
	    ParseTypeDef();
	    break;

	case Token::Const:
	    ParseConstDef();
	    break;

	case Token::Function:
	case Token::Procedure:
	{
	    if (FunctionAST* fn = ParseDefinition(level+1))
	    {
		subFunctions.push_back(fn);
	    }
	    break;
	}

	case Token::Begin:
	{
	    Location endLoc; 
	    assert(!body && "Multiple body declarations for function?");

	    if (!(body = ParseBlock(endLoc)) || !Expect(Token::Semicolon, true))
	    {
		return 0;
	    }

	    FunctionAST* fn = new FunctionAST(loc, proto, varDecls, body);
	    if (!proto->Function())
	    {
		proto->SetFunction(fn);
	    }
	    for(auto s : subFunctions)
	    {
		s->SetParent(fn);
	    }
	    // Need to add subFunctions before setting used vars!
	    fn->AddSubFunctions(subFunctions);
	    fn->SetUsedVars(usedVariables.GetLevel(), nameStack);
	    if (Types::TypeDecl *closure = fn->ClosureType())
	    {
		proto->AddExtraArgsFirst({ VarDef(fn->ClosureName(), closure) });
	    }
	    UpdateCallVisitor updater(proto);
	    fn->accept(updater);
	    fn->EndLoc(endLoc);
	    return fn;
	}

	default:
	    assert(0 && "Unexpected token");
	    return ErrorF("Unexpected token");
	}
    }
    return 0;
}

ExprAST* Parser::ParseIfExpr()
{
    TRACE();
    Location loc = CurrentToken().Loc();
    AssertToken(Token::If);
    ExprAST* cond = ParseExpression();
    if (!cond || !Expect(Token::Then, true))
    {
	return 0;
    }

    ExprAST* then = 0;
    if (CurrentToken().GetToken() != Token::Else)
    {
	then = ParseStatement();
	if (!then)
	{
	    return 0;
	}
    }

    ExprAST* elseExpr = 0;
    if (AcceptToken(Token::Else))
    {
	if (!(elseExpr = ParseStatement()))
	{
	    return 0;
	}
    }
    return new IfExprAST(loc, cond, then, elseExpr);
}

ExprAST* Parser::ParseForExpr()
{
    Location loc = CurrentToken().Loc();
    AssertToken(Token::For);
    if (CurrentToken().GetToken() != Token::Identifier)
    {
	return Error("Expected identifier name, got " + CurrentToken().ToString());
    }
    std::string varName = CurrentToken().GetIdentName();
    AssertToken(Token::Identifier);
    NamedObject* def = nameStack.Find(varName);
    assert(def && "Expected name to be found");
    VariableExprAST* varExpr = new VariableExprAST(CurrentToken().Loc(), varName, def->Type());
    if (!Expect(Token::Assign, true))
    {
	return 0;
    }
    ExprAST* start = ParseExpression();
    if (!start)
    {
	return 0;
    }
    bool down = false;
    Token::TokenType tt = CurrentToken().GetToken();
    if (tt == Token::Downto || tt == Token::To)
    {
	down = (tt == Token::Downto);
	NextToken();
    }
    else
    {
	return Error("Expected 'to' or 'downto', got " + CurrentToken().ToString());
    }

    ExprAST* end = ParseExpression();
    if (!end || !Expect(Token::Do, true))
    {
	return 0;
    }
    if (ExprAST* body = ParseStatement())
    {
	return new ForExprAST(loc, varExpr, start, end, down, body);
    }
    return 0;
}

ExprAST* Parser::ParseWhile()
{
    TRACE();
    Location loc = CurrentToken().Loc();
    AssertToken(Token::While);
    ExprAST* cond = ParseExpression();
    if (!cond || !Expect(Token::Do, true))
    {
	return 0;
    }

    return new WhileExprAST(loc, cond, ParseStatement());
}

ExprAST* Parser::ParseRepeat()
{
    TRACE();
    Location loc = CurrentToken().Loc();
    AssertToken(Token::Repeat);
    std::vector<ExprAST*> v;
    Location loc2 = CurrentToken().Loc();
    while(!AcceptToken(Token::Until))
    {
	if (ExprAST* stmt = ParseStatement())
	{
	    v.push_back(stmt);
	    AcceptToken(Token::Semicolon);
	}
	else
	{
	    return 0;
	}
    }
    ExprAST* cond = ParseExpression();
    return new RepeatExprAST(loc, cond, new BlockAST(loc2, v));
}

ExprAST* Parser::ParseCaseExpr()
{
    TRACE();
    Location loc = CurrentToken().Loc();
    AssertToken(Token::Case);
    ExprAST* expr = ParseExpression();
    if (!expr  || !Expect(Token::Of, true))
    {
	return 0;
    }
    std::vector<LabelExprAST*> labels;
    std::vector<int> lab;
    bool isFirst = true;
    Token::TokenType prevTT;
    ExprAST* otherwise = 0;
    do
    {
	bool isOtherwise = false;
	if (isFirst)
	{
	    prevTT = CurrentToken().GetToken();
	    isFirst = false;
	}
	else if (CurrentToken().GetToken() != Token::Otherwise &&
		 CurrentToken().GetToken() != Token::Else && 
		 prevTT != CurrentToken().GetToken())
	{
	    return Error("Type of case labels must not change type");
	}
	switch(CurrentToken().GetToken())
	{
	case Token::Char:
	case Token::Integer:
	    lab.push_back(CurrentToken().GetIntVal());
	    break;

	case Token::Identifier:
	    if (EnumDef* ed = GetEnumValue(CurrentToken().GetIdentName()))
	    {
		lab.push_back(ed->Value());
		break;
	    }
	    return Error("Expected enumerated type value");

	case Token::Else:
	case Token::Otherwise:
	    if (otherwise)
	    {
		return Error("An 'otherwise' or 'else' already used in this case block");
	    }
	    isOtherwise = true;
	    break;

	default:
	    return Error("Syntax error, expected case label");
	}
	if (CurrentToken().GetToken() != Token::Else && CurrentToken().GetToken() != Token::Otherwise)
	{
	    NextToken();
	}
	switch(CurrentToken().GetToken())
	{
	case Token::Comma:
	    if (isOtherwise)
	    {
		return Error("Can't have multiple case labels with 'otherwise' or else 'case' label");
	    }
	    AssertToken(Token::Comma);
	    break;

	case Token::Colon:
	case Token::Otherwise:
	case Token::Else:
	{
	    Location locColon = NextToken().Loc();
	    ExprAST* s = ParseStatement();
	    if (isOtherwise)
	    {
		otherwise = s;
		if (lab.size())
		{
		    return Error("Can't have multiple case labels with 'otherwise' or 'else'"
				 " case label");
		}
	    }
	    else
	    {
		labels.push_back(new LabelExprAST(locColon, lab, s));
		lab.clear();
	    }
	    if (!ExpectSemicolonOrEnd())
	    {
		return 0;
	    }
	    break;
	}
	
	default:
	    return Error("Syntax error: Expected ',' or ':' in case-statement.");
	}
    } while(!AcceptToken(Token::End));
    return new CaseExprAST(loc, expr, labels, otherwise);
}

void Parser::ExpandWithNames(const Types::FieldCollection* fields, VariableExprAST* v, int parentCount)
{
    TRACE();
    int vtableoffset = 0;
    if (const Types::ClassDecl* cd = llvm::dyn_cast<Types::ClassDecl>(fields))
    {
	vtableoffset = !!cd->VTableType(true);
    }
    for(int i = 0; i < fields->FieldCount(); i++)
    {
	const Types::FieldDecl* f = fields->GetElement(i);
	Types::TypeDecl* ty = f->FieldType();
	if (f->Name() == "")
	{
	    Types::RecordDecl* rd = llvm::dyn_cast<Types::RecordDecl>(ty);
	    assert(rd && "Expected record declarataion here!");
	    ExpandWithNames(rd, new VariantFieldExprAST(CurrentToken().Loc(), v, parentCount, ty), 0);
	}
	else
	{
	    ExprAST* e;
	    if (llvm::isa<Types::RecordDecl>(fields) || llvm::isa<Types::ClassDecl>(fields))
	    {
		e = new FieldExprAST(CurrentToken().Loc(), v, i + vtableoffset, ty);
	    }
	    else
	    {
		e = new VariantFieldExprAST(CurrentToken().Loc(), v, parentCount, ty);
	    }
	    nameStack.Add(f->Name(), new WithDef(f->Name(), e, f->FieldType()));
	}
    }
    if (Types::ClassDecl* od = const_cast<Types::ClassDecl*>(llvm::dyn_cast<Types::ClassDecl>(fields)))
    {
	int count = od->MembFuncCount();
	for(int i = 0; i < count; i++)
	{
	    Types::MemberFuncDecl* mf = const_cast<Types::MemberFuncDecl*>(od->GetMembFunc(i));
	    std::string name = mf->Proto()->Name();
	    nameStack.Add(name, new MembFuncDef(name, i, od));
	}
    }
}

ExprAST* Parser::ParseWithBlock()
{
    TRACE();
    Location loc = CurrentToken().Loc();
    AssertToken(Token::With);
    int levels = 0;
    bool error = false;
    do
    {
	nameStack.NewLevel();
	levels++;
	ExprAST* e = ParseIdentifierExpr();
	if (VariableExprAST* v = llvm::dyn_cast_or_null<VariableExprAST>(e))
	{
	    if (Types::RecordDecl* rd = llvm::dyn_cast<Types::RecordDecl>(v->Type()))
	    {
		ExpandWithNames(rd, v, 0);
		if (Types::VariantDecl* variant = rd->Variant())
		{
		    ExpandWithNames(variant, v, rd->FieldCount());
		}
	    }
	    else
	    {
		Error("Type for with statement should be a record type");
		error = true;
	    }
	    if (CurrentToken().GetToken() != Token::Do && !Expect(Token::Comma, true))
	    {
		error = true;
	    }
	}
	else
	{
	    Error("With statement must contain only variable expression");
	    error = true;
	}
    } while(!AcceptToken(Token::Do) && !error);

    ExprAST* body = 0;
    if (!error)
    {
	body = ParseStatement();
    }

    for(int i = 0; i < levels; i++)
    {
	nameStack.DropLevel();
    }

    if (error)
    {
	return 0;
    }

    return new WithExprAST(loc, body);
}

ExprAST* Parser::ParseWrite()
{
    Location loc = CurrentToken().Loc();
    bool isWriteln = CurrentToken().GetToken() == Token::Writeln;

    assert((CurrentToken().GetToken() == Token::Write ||
	    CurrentToken().GetToken() == Token::Writeln) &&
	   "Expected write or writeln keyword here");
    NextToken();

    VariableExprAST* file = 0;
    std::vector<WriteAST::WriteArg> args;
    if (IsSemicolonOrEnd())
    {
	if (!isWriteln)
	{
	    return Error("Write must have arguments.");
	}
	file = new VariableExprAST(loc, "output", Types::GetTextType());
    }
    else
    {
	if (!Expect(Token::LeftParen, true))
	{
	    return 0;
	}
	
	while(!AcceptToken(Token::RightParen))
	{
	    WriteAST::WriteArg wa;
	    if (!(wa.expr = ParseExpression()))
	    {
		return 0;
	    }
	    if (args.size() == 0)
	    {
		if (VariableExprAST* vexpr = llvm::dyn_cast<VariableExprAST>(wa.expr))
		{
		    if (llvm::isa<Types::FileDecl>(vexpr->Type()))
		    {
			file = vexpr;
			wa.expr = 0;
		    }
		}
		if (file == 0)
		{
			file = new VariableExprAST(loc, "output", Types::GetTextType());
		}
	    }
	    if (wa.expr)
	    {
		if (AcceptToken(Token::Colon))
		{
		    wa.width = ParseExpression();
		    if (!wa.width)
		    {
			return Error("Invalid width expression");
		    }
		}
		if (AcceptToken(Token::Colon))
		{
		    wa.precision = ParseExpression();
		    if (!wa.precision)
		    {
			return Error("Invalid precision expression");
		    }
		}
		args.push_back(wa);
	    }
	    if (CurrentToken().GetToken() != Token::RightParen && !Expect(Token::Comma, true))
	    {
		return 0;
	    }
	}
	assert((args.size() >= 1 || isWriteln) && "Expected at least one expression for output in write");
    }
    return new WriteAST(loc, file, args, isWriteln);
}

ExprAST* Parser::ParseRead()
{
    Location loc = CurrentToken().Loc();
    bool isReadln = CurrentToken().GetToken() == Token::Readln;

    assert((CurrentToken().GetToken() == Token::Read ||
	    CurrentToken().GetToken() == Token::Readln) &&
	   "Expected read or readln keyword here");
    NextToken();

    std::vector<ExprAST*> args;
    VariableExprAST* file = 0;
    if (IsSemicolonOrEnd())
    {
	if (!isReadln)
	{
	    return Error("Read must have arguments.");
	}
	file = new VariableExprAST(loc, "input", Types::GetTextType());
    }
    else
    {
	if (!Expect(Token::LeftParen, true))
	{
	    return 0;
	}
	while(!AcceptToken(Token::RightParen))
	{
	    ExprAST* expr = ParseExpression();
	    if (!expr)
	    {
		return 0;
	    }
	    if (args.size() == 0)
	    {
		if (VariableExprAST* vexpr = llvm::dyn_cast<VariableExprAST>(expr))
		{
		    if (llvm::isa<Types::FileDecl>(vexpr->Type()))
		    {
			file = vexpr;
			expr = 0;
		    }
		}
		if (file == 0)
		{
		    file = new VariableExprAST(loc, "input", Types::GetTextType());
		}
	    }
	    if (expr)
	    {
		args.push_back(expr);
	    }
	    if (CurrentToken().GetToken() != Token::RightParen && !Expect(Token::Comma, true))
	    {
		return 0;
	    }
	}
	assert((args.size() >= 1 || isReadln) && "Expected at least one variable in read statement");
    }
    return new ReadAST(loc, file, args, isReadln);
}

ExprAST* Parser::ParsePrimary()
{
    Token token = TranslateToken(CurrentToken());

    switch(token.GetToken())
    {
    case Token::Nil:
	AssertToken(Token::Nil);
	return new NilExprAST(CurrentToken().Loc());

    case Token::Real:
	NextToken();
	return new RealExprAST(token.Loc(), token.GetRealVal(), GetTypeDecl("real"));

    case Token::Integer:
	return ParseIntegerExpr(token);

    case Token::Char:
	NextToken();
	return new CharExprAST(token.Loc(), token.GetIntVal(), GetTypeDecl("char"));

    case Token::StringLiteral:
	return ParseStringExpr(token);

    case Token::LeftParen:
	return ParseParenExpr();

    case Token::LeftSquare:
	return ParseSetExpr();
	
    case Token::Identifier:
	return ParseIdentifierExpr();

    case Token::If:
	return ParseIfExpr();

    case Token::For:
	return ParseForExpr();

    case Token::While:
	return ParseWhile();

    case Token::Repeat:
	return ParseRepeat();

    case Token::Case:
	return ParseCaseExpr();

    case Token::With:
	return ParseWithBlock();

    case Token::Write:
    case Token::Writeln:
	return ParseWrite();

    case Token::Read:
    case Token::Readln:
	return ParseRead();

    case Token::Minus:
    case Token::Plus:
    case Token::Not:
	return ParseUnaryOp();

    case Token::SizeOf:
	return ParseSizeOfExpr();

    case Token::Goto:
	return ParseGoto();

    default:
	CurrentToken().dump(std::cerr);
	assert(0 && "Unexpected token");
	return 0;
    }
}

bool Parser::ParseProgram(ParserType type)
{
    Token::TokenType t  = Token::Program;
    if (type == Unit)
    {
	t = Token::Unit;
    }
    if (!Expect(t, true))
    {
	return false;
    }
    if (!Expect(Token::Identifier, false))
    {
	return false;
    }
    moduleName = CurrentToken().GetIdentName();
    AssertToken(Token::Identifier);
    if (type == Program && AcceptToken(Token::LeftParen))
    {
	// We completely ignore "file" specifications on the program.
	do
	{
	    if (!Expect(Token::Identifier, true))
	    {
		return false;
	    }
	    if (CurrentToken().GetToken() != Token::RightParen && !Expect(Token::Comma, true))
	    {
		return false;
	    }
	} while(!AcceptToken(Token::RightParen));
    }
    return true;
}

ExprAST* Parser::ParseUses()
{
    AssertToken(Token::Uses);
    if (Expect(Token::Identifier, false))
    {
	std::string unitname = CurrentToken().GetIdentName();
	AssertToken(Token::Identifier);
	if (unitname == "math")
	{
	    /* Math unit is "fake", so nothing inside it for now */
	    return new UnitAST(CurrentToken().Loc(), { }, 0, { });
	}
	else
	{
	    /* TODO: Loop over commaseparated list */
	    strlower(unitname);
	    std::string fileName = unitname + ".pas";
	    FileSource source(fileName);
	    if (!source)
	    {
		std::cerr << "Could not open " << fileName << std::endl;
		return 0;
	    }
	    Parser p(source);
	    ExprAST* e = p.Parse(Unit);
	    if (!Expect(Token::Semicolon, true))
	    {
		return 0;
	    }
	    return e;
	}
    }
    return 0;
}

bool Parser::ParseInterface(InterfaceList &iList)
{
    NameWrapper wrapper(nameStack);
    AssertToken(Token::Interface);
    do
    {
	switch(CurrentToken().GetToken())
	{
	case Token::Type:
	    ParseTypeDef();
	    break;

	case Token::Procedure:
	case Token::Function:
	{
	    PrototypeAST* proto = ParsePrototype(false);
	    if(!proto || !Expect(Token::Semicolon, true))
	    {
		return false;
	    }
	    proto->SetIsForward(true);
	    std::string name = proto->Name();
	    Types::TypeDecl* ty = new Types::FunctionDecl(proto);
	    FuncDef* nmObj = new FuncDef(name, ty, proto);
	    if (!nameStack.Add(name, nmObj))
	    {
		return ErrorF("Interface name '" + name + "' already exists in...");
	    }
	}
	    break;

	case Token::Var:
	{
	    VarDeclAST* v = ParseVarDecls();
	    ast.push_back(v);
	}   
	    break;

	default:
	    assert(0);
	    Error("Unexpected token");
	    break;
	}
    } while(CurrentToken().GetToken() != Token::Implementation);
    for(auto i : nameStack.GetLevel())
    {
	iList.Add(i->Name(), i);
    }
    return true;
}

ExprAST* Parser::ParseUnit(ParserType type)
{
    Location unitloc = CurrentToken().Loc();
    if (!ParseProgram(type) || !Expect(Token::Semicolon, true))
    {
	return 0;
    }

    // The "main" of the program - we call that "__PascalMain" so we can call it from C-code.
    std::string initName = "__PascalMain";
    // In a unit, we use the moduleName to form the "init functioin" name.
    if (type == Unit)
    {
	initName = moduleName + ".init";
    }

    FunctionAST* initFunction = 0;
    bool finished = false;
    InterfaceList interfaceList;
    do
    {
	ExprAST* curAst = 0;
	switch(CurrentToken().GetToken())
	{
	case Token::EndOfFile:
	    return Error("Unexpected end of file");

	case Token::Uses:
	{
	    curAst = ParseUses();
	    if (UnitAST *ua = llvm::dyn_cast_or_null<UnitAST>(curAst))
	    {
		for(auto i : ua->Interface().List())
		{
		    if (!nameStack.Add(i.first, i.second))
		    {
			return 0;
		    }
		}
	    }
	}
	    break;

	case Token::Semicolon:
	    NextToken();
	    break;

	case Token::Label:
	    ParseLabels();
	    break;

	case Token::Function:
	case Token::Procedure:
	    curAst = ParseDefinition(0);
	    break;

	case Token::Var:
	    curAst = ParseVarDecls();
	    break;

	case Token::Type:
	    ParseTypeDef();
	    break;

	case Token::Const:
	    ParseConstDef();
	    break;

	case Token::Interface:
	    if (!ParseInterface(interfaceList))
	    {
		return 0;
	    }
	    for(auto i : interfaceList.List())
	    {
		if (!nameStack.Add(i.first, i.second))
		{
		    return 0;
		}
	    }
	    break;

	case Token::Implementation:
	{
	    /* Start a new level of names */
	    AssertToken(Token::Implementation);
	    break;
	}

	case Token::Begin:
	{
	    Location loc = CurrentToken().Loc();
	    Location endLoc;
	    BlockAST* body = ParseBlock(endLoc);
	    if (!body)
	    {
		return 0;
	    }
	    PrototypeAST* proto = new PrototypeAST(loc, initName, std::vector<VarDef>(),
						   Types::GetVoidType(), 0);
	    initFunction = new FunctionAST(loc, proto, std::vector<VarDeclAST*>(), body);
	    initFunction->EndLoc(endLoc);
	    finished = true;
	    break;
	}

	case Token::End:
	    if (type != Unit)
	    {
		return Error("Unexpected 'end' token");
	    }
	    AssertToken(Token::End);
	    if (!Expect(Token::Period, true))
	    {
		return 0;
	    }
	    finished = true;
	    break;

	default:
	    Error("Unexpected token");
	    assert(0);
	    break;
	}

	if (curAst)
	{
	    ast.push_back(curAst);
	}
    } while(!finished);
    return new UnitAST(unitloc, ast, initFunction, interfaceList);
}

ExprAST* Parser::Parse(ParserType type)
{
    TIME_TRACE();

    NextToken();
    if (type == Program)
    {
	VarDef input("input", Types::GetTextType(), false, true);
	VarDef output("output", Types::GetTextType(), false, true);
	nameStack.Add("input", new VarDef(input));
	nameStack.Add("output", new VarDef(output));
	std::vector<VarDef> varList{input, output};
	ast.push_back(new VarDeclAST(Location("", 0, 0), varList));
    }

    return ParseUnit(type);
}

Parser::Parser(Source &source)
    : lexer(source), nextTokenValid(false), errCnt(0)
{
    Types::TypeDecl* ty = new Types::BoolDecl;
    if (!(AddType("integer", new Types::IntegerDecl) &&
	  AddType("longint", new Types::Int64Decl) &&
	  AddType("int64", new Types::Int64Decl) &&
	  AddType("real", new Types::RealDecl) &&
	  AddType("char", new Types::CharDecl) &&
	  AddType("text", Types::GetTextType()) &&
	  AddType("boolean", ty) &&
	  nameStack.Add("false", new EnumDef("false", 0, ty)) &&
	  nameStack.Add("true", new EnumDef("true", 1, ty)) &&
	  AddConst("maxint", new Constants::IntConstDecl(Location("", 0, 0), INT_MAX)) &&
	  AddConst("pi", new Constants::RealConstDecl(Location("", 0, 0), M_PI))))
    {
	assert(0 && "Failed to add builtin constants");
    }
}
