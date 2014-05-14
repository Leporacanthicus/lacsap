#include "lexer.h"
#include "parser.h"
#include "expr.h"
#include "namedobject.h"
#include "stack.h"
#include "builtin.h"
#include "options.h"
#include <iostream>
#include <cassert>
#include <limits>
#include <vector>
#include <algorithm>

#define TRACE() std::cerr << __FILE__ << ":" << __LINE__ << "::" << __PRETTY_FUNCTION__ << std::endl

Parser::Parser(Lexer &l) 
    : lexer(l), nextTokenValid(false), errCnt(0)
{
    std::vector<std::string> FalseTrue;
    FalseTrue.push_back("false");
    FalseTrue.push_back("true");
    if (!(AddType("integer", new Types::TypeDecl(Types::Integer)) &&
	  AddType("longint", new Types::TypeDecl(Types::Int64)) &&
	  AddType("real", new Types::TypeDecl(Types::Real)) &&
	  AddType("char", new Types::TypeDecl(Types::Char)) &&
	  AddType("boolean", new Types::EnumDecl(FalseTrue, Types::Boolean)) &&
	  AddType("text", new Types::TextDecl())))
    {
	assert(0 && "Failed to add basic types...");
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
    return 0;
}

PrototypeAST* Parser::ErrorP(const std::string& msg)
{
    Error(msg);
    return 0;
}

FunctionAST* Parser::ErrorF(const std::string& msg)
{
    Error(msg);
    return 0;
}

Types::TypeDecl* Parser::ErrorT(const std::string& msg)
{
    Error(msg);
    return 0;
}

Types::Range* Parser::ErrorR(const std::string& msg)
{
    Error(msg);
    return 0;
}

VariableExprAST* Parser::ErrorV(const std::string& msg)
{
    Error(msg);
    return 0;
}

const Token& Parser::CurrentToken() const
{
    return curToken;
}

const Token& Parser::NextToken(const char* file, int line)
{
    (void)file;
    (void)line;
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
	curToken.dump(std::cout, file, line);
    }
    return curToken;
}

const Token& Parser::PeekToken(const char* file, int line) 
{
    if (nextTokenValid)
    {
	return nextToken;
    }
    else
    {
	nextTokenValid = true;
	nextToken = lexer.GetToken();
    }
    if (verbosity > 1)
    {
	std::cout << "peeking: ";
	nextToken.dump(std::cout, file, line);
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

bool Parser::ExpectSemicolonOrEnd(const char* file, int line)
{
    if (CurrentToken().GetToken() != Token::End && !Expect(Token::Semicolon, true, file, line))
    {
	return false;
    }
    return true;
}

#define NextToken() NextToken(__FILE__, __LINE__)
#define PeekToken() PeekToken(__FILE__, __LINE__)
#define ExpectSemicolonOrEnd() ExpectSemicolonOrEnd(__FILE__, __LINE__)
#define Expect(t, e) Expect(t, e, __FILE__, __LINE__)

Types::TypeDecl* Parser::GetTypeDecl(const std::string& name)
{
    NamedObject* def = nameStack.Find(name);
    if (!def) 
    {
	return 0;
    }
    TypeDef *typeDef = llvm::dyn_cast<TypeDef>(def);
    if (!typeDef)
    {
	return 0;
    }
    return typeDef->Type();
}

ExprAST* Parser::ParseNilExpr()
{
    if (!Expect(Token::Nil, true))
    {
	return 0;
    }
    return new NilExprAST();
}

Constants::ConstDecl* Parser::GetConstDecl(const std::string& name)
{
    NamedObject* def = nameStack.Find(name);
    if (!def) 
    {
	return 0;
    }
    ConstDef *constDef = llvm::dyn_cast<ConstDef>(def);
    if (!constDef)
    {
	return 0;
    }
    return constDef->ConstValue();
}

EnumDef* Parser::GetEnumValue(const std::string& name)
{
    NamedObject* def = nameStack.Find(name);
    if (!def) 
    {
	return 0;
    }
    EnumDef *enumDef = llvm::dyn_cast<EnumDef>(def);
    if (!enumDef)
    {
	return 0;
    }
    return enumDef;
}

bool Parser::AddType(const std::string& name, Types::TypeDecl* ty)
{
    Types::EnumDecl* ed = llvm::dyn_cast<Types::EnumDecl>(ty);
    if (ed)
    {
	for(auto v : ed->Values())
	{
	    if (!nameStack.Add(v.name, new EnumDef(v.name, v.value, ty)))
	    {
		Error("Enumerated value by name " + v.name + " already exists...");
		return false;
	    }
	}
    }
    return nameStack.Add(name, new TypeDef(name, ty));
}

Types::TypeDecl* Parser::ParseSimpleType()
{
    if (CurrentToken().GetToken() != Token::Identifier)
    {
	return ErrorT("Expected identifier of simple type");
    }
    Types::TypeDecl* ty = GetTypeDecl(CurrentToken().GetIdentName());
    if (!ty)
    {
	return ErrorT("Identifier does not name a type");
    }
    NextToken();
    return ty;
}

int Parser::ParseConstantValue(Token::TokenType& tt)
{
    Token token = CurrentToken();
    if (token.GetToken() == Token::Identifier)
    {
	Constants::ConstDecl* cd = GetConstDecl(token.GetIdentName());
	if (cd)
	{
	    token = cd->Translate();
	}
    }

    if (tt != Token::Unknown && token.GetToken() != tt)
    {
	Error("Expected token to match type"); 
	tt = Token::Unknown;
	return 0;
    }

    tt = token.GetToken();
    
    if (tt == Token::Integer || tt == Token::Char)
    {
	NextToken();
	return token.GetIntVal();
    }
    else if (tt == Token::Identifier)
    {
	tt = Token::Unknown;
	EnumDef* ed = GetEnumValue(CurrentToken().GetIdentName()); 
	if (!ed)
	{
	    Error("Invalid range specification, expected identifier for enumerated type");
	    return 0;
	}
	tt = CurrentToken().GetToken();
	NextToken();
	return ed->Value();
    }
    else
    {
	tt = Token::Unknown;
	Error("Invalid constant value, expected char, integer or enum value");
	return 0;
    }
}

Types::Range* Parser::ParseRange()
{
    Token::TokenType tt = Token::Unknown;
    int start = ParseConstantValue(tt);
    if (tt == Token::Unknown)
    {
	return 0;
    }
    if (!Expect(Token::DotDot, true))
    {
	return 0;
    }
    
    int end = ParseConstantValue(tt);

    if (tt == Token::Unknown)
    {
	return 0;
    }

    if (end <= start)
    {
	return ErrorR("Invalid range specification");
    }
    return new Types::Range(start, end);
}

Types::Range* Parser::ParseRangeOrTypeRange()
{
    if (CurrentToken().GetToken() == Token::Identifier)
    {
	Types::TypeDecl* ty = GetTypeDecl(CurrentToken().GetIdentName());
	if (!ty->isIntegral())
	{
	    return ErrorR("Type used as index specification should be integral type");
	}
	NextToken();
	return ty->GetRange();
    }
    else
    {
	return ParseRange();
    }
}

void Parser::ParseConstDef()
{
    if (!Expect(Token::Const, true))
    {
	return;
    }
    do
    {
	if (!Expect(Token::Identifier, false))
	{
	    return;
	}
	std::string nm = CurrentToken().GetIdentName();
	NextToken();
	if (!Expect(Token::Equal, true))
	{
	    return;
	}
	Constants::ConstDecl *cd = 0;
	Location loc = CurrentToken().Loc();
	switch(CurrentToken().GetToken())
	{
	case Token::StringLiteral:
	    cd = new Constants:: StringConstDecl(loc, CurrentToken().GetStrVal());
	    break;

	case Token::Integer:
	    cd = new Constants:: IntConstDecl(loc, CurrentToken().GetIntVal());
	    break;

	case Token::Real:
	    cd = new Constants:: RealConstDecl(loc, CurrentToken().GetRealVal());
	    break;

	case Token::Char:
	    cd = new Constants:: CharConstDecl(loc, (char) CurrentToken().GetIntVal());
	    break;

	case Token::Identifier:
	{
	    EnumDef* ed = GetEnumValue(CurrentToken().GetIdentName());
	    if (ed)
	    {
		cd = new Constants:: IntConstDecl(loc, ed->Value());
	    }
	    break;
	}
	 
	default:
	    break;
	}
	if (!cd) 
	{
	    Error("Invalid constant value");
	}
	NextToken();
	if (!nameStack.Add(nm, new ConstDef(nm, cd)))
	{
	    Error(std::string("Name ") + nm + " is already declared as a constant");
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
    if (!Expect(Token::Type, true))
    {
	return;
    }
    do
    {
	if (!Expect(Token::Identifier, false))
	{
	    return;
	}
	std::string nm = CurrentToken().GetIdentName();
	NextToken();
	if (!Expect(Token::Equal, true))
	{
	    return;
	}
	Types::TypeDecl* ty = ParseType();
	if (!ty)
	{
	    return;
	}
	
	if (!AddType(nm,  ty))
	{
	    Error(std::string("Name ") + nm + " is already in use.");
	}
	if (ty->Type() == Types::PointerIncomplete)
	{
	    Types::PointerDecl* pd = llvm::dyn_cast<Types::PointerDecl>(ty);
	    incomplete.push_back(pd);
	}	    
	if (!Expect(Token::Semicolon, true))
	{
	    return;
	}
    } while (CurrentToken().GetToken() == Token::Identifier);

    // Now fix up any incomplete types...
    for(auto p : incomplete)
    {
	Types::TypeDecl *ty = GetTypeDecl(p->Name());
	if(!ty)
	{
	    Error("Forward declared pointer type not declared: " + p->Name());
	    return;
	}
	p->SetSubType(ty);
    }
}

Types::EnumDecl* Parser::ParseEnumDef()
{
    if (!Expect(Token::LeftParen, true))
    {
	return 0;
    }
    std::vector<std::string> values;
    while(CurrentToken().GetToken() != Token::RightParen)
    {
	if (!Expect(Token::Identifier, false))
	{
	    return 0;
	}
	values.push_back(CurrentToken().GetIdentName());
	NextToken();
	if (CurrentToken().GetToken() != Token::RightParen)
	{
	    if (!Expect(Token::Comma, true))
	    {
		return 0;
	    }
	}
    }
    if (!Expect(Token::RightParen, true))
    {
	return 0;
    }
    return new Types::EnumDecl(values);
}

Types::PointerDecl* Parser::ParsePointerType()
{
    if (!Expect(Token::Uparrow, true))
    {
	return 0;
    }
    // If the name is an "identifier" then it's a name of a not yet declared type.
    // We need to forward declare it, and backpatch later. 
    if (CurrentToken().GetToken() == Token::Identifier)
    {
	std::string name = CurrentToken().GetIdentName();
	// Is it a known type?
	NextToken();
	Types::TypeDecl* ty = GetTypeDecl(name); 
	if (ty)
	{
	    return new Types::PointerDecl(ty);	
	}
	// Otherwise, forward declare... 
	return new Types::PointerDecl(name);
    }
    else
    {
	Types::TypeDecl *ty = ParseType();
	return new Types::PointerDecl(ty);
    }
}

Types::ArrayDecl* Parser::ParseArrayDecl()
{
    if (!Expect(Token::Array, true))
    {
	return 0;
    }
    if (!Expect(Token::LeftSquare, true))
    {
	return 0;
    }
    std::vector<Types::Range*> rv;
    while(CurrentToken().GetToken() != Token::RightSquare)
    {
	Types::Range* r = ParseRangeOrTypeRange();
	if (!r) 

	{
	    return 0;
	}
	rv.push_back(r);
	if (CurrentToken().GetToken() == Token::Comma)
	{
	    NextToken();
	}
    }
    if (!Expect(Token::RightSquare, true) || 
	!Expect(Token::Of, true))
    {
	return 0;
    }
    Types::TypeDecl* ty = ParseType();
    if (!ty)
    {
	return 0;
    }
    return new Types::ArrayDecl(ty, rv);
}


Types::VariantDecl* Parser::ParseVariantDecl()
{
    Token::TokenType tt = Token::Unknown;
    std::vector<int> variantsSeen;
    std::vector<Types::FieldDecl> variants; 
    do
    {
	do
	{
	    int v = ParseConstantValue(tt);
	    if (tt == Token::Unknown)
	    {
		return 0;
	    }
	    if (std::find(variantsSeen.begin(), variantsSeen.end(), v) != variantsSeen.end())
	    {
		Error(std::string("Value already used: ") + std::to_string(v) + " in variant declaration");
		return 0;
	    }
	    variantsSeen.push_back(v);
	    if (CurrentToken().GetToken() != Token::Colon)
	    {
		if (!Expect(Token::Comma, true))
		{
		    return 0;
		}
	    }
	} while (CurrentToken().GetToken() != Token::Colon);
	if (!Expect(Token::Colon, true))
	{
	    return 0;
	}
	if (!Expect(Token::LeftParen, true))
	{
	    return 0;
	}
	std::vector<Types::FieldDecl> fields; 
	do 
	{
	    std::vector<std::string> names;
	    do 
	    {
		// TODO: Fix up to reduce duplication of this code. It's in several places now.
		if (!Expect(Token::Identifier, false))
		{
		    return 0;
		}
		names.push_back(CurrentToken().GetIdentName());
		NextToken();
		if (CurrentToken().GetToken() != Token::Colon)
		{
		    if (!Expect(Token::Comma, true))
		    {
			return 0;
		    }
		}
	    } while(CurrentToken().GetToken() != Token::Colon);
	    if (!Expect(Token::Colon, true))
	    {
		return 0;
	    }
	    Types::TypeDecl* ty = ParseType();
	    if (!ty)
	    {
	    return 0;
	    }
	    for(auto n : names)
	    {
		for(auto f : fields)
		{
		    if (n == f.Name())
		    {
			Error(std::string("Duplicate field name '") + n + "' in record");
			return 0;
		    }
		}
		fields.push_back(Types::FieldDecl(n, ty));
	    }
	    if (CurrentToken().GetToken() != Token::RightParen)
	    {
		if (!Expect(Token::Semicolon, true))
		{
		    return 0;
		}
	    }
	} while(CurrentToken().GetToken() != Token::RightParen);
	if (!Expect(Token::RightParen, true))
	{
	}
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
	    variants.push_back(Types::FieldDecl("", new Types::RecordDecl(fields, 0)));
	}
    } while (CurrentToken().GetToken() != Token::End);
    return new Types::VariantDecl(variants);
}

Types::RecordDecl* Parser::ParseRecordDecl()
{
    if (!Expect(Token::Record, true))
    {
	return 0;
    }
    Types::VariantDecl* variant = 0;
    std::vector<Types::FieldDecl> fields;
    do
    {
	std::vector<std::string> names;
	// Parse Variant part if we have a "case". 
	if (CurrentToken().GetToken() == Token::Case)
	{
	    NextToken();
	    std::string marker = "";
	    Types::TypeDecl* markerTy;
	    if (CurrentToken().GetToken() == Token::Identifier && PeekToken().GetToken() == Token::Colon)
	    {
		marker = CurrentToken().GetIdentName();
		NextToken();
		if (!Expect(Token::Colon, true))
		{
		    return 0;
		}
	    }
	    markerTy = ParseType();
	    if (!markerTy->isIntegral())
	    {
		Error("Expect variant selector to be integral type");
		return 0;
	    }
	    if (marker != "")
	    {
		fields.push_back(Types::FieldDecl(marker, markerTy));
	    }
	    if (!Expect(Token::Of, true))
	    {
		return 0;
	    }
	    variant =  ParseVariantDecl();
	    assert(variant);
	}
	else
	{
	    do
	    {
		if (!Expect(Token::Identifier, false))
		{
		    return 0;
		}
		names.push_back(CurrentToken().GetIdentName());
		NextToken();
		if (CurrentToken().GetToken() != Token::Colon)
		{
		    if (!Expect(Token::Comma, true))
		    {
			return 0;
		    }
		}
	    } while(CurrentToken().GetToken() != Token::Colon);
	    if (!Expect(Token::Colon, true))
	    {
		return 0;
	    }
	    if (names.size() == 0)
	    {
		assert(0 && "Should have at least one name declared?");
		return 0;
	    }
	    Types::TypeDecl* ty = ParseType();
	    if (!ty)
	    {
		return 0;
	    }
	    for(auto n : names)
	    {
		for(auto f : fields)
		{
		    if (n == f.Name())
		    {
			Error(std::string("Duplicate field name '") + n + "' in record");
			return 0;
		    }
		}
		fields.push_back(Types::FieldDecl(n, ty));
	    }
	}
	if (!ExpectSemicolonOrEnd())
	{
	    return 0;
	}
    } while(CurrentToken().GetToken() != Token::End);
    NextToken();
    if (fields.size() == 0 && !variant)
    {
	Error("No elements in record declaration");
	return 0;
    }
    Types::RecordDecl* r = new Types::RecordDecl(fields, variant);
    return r;
}

Types::FileDecl* Parser::ParseFileDecl()
{
    if (!Expect(Token::File, true))
    {
	return 0;
    }

    if (!Expect(Token::Of, true))
    {
	return 0;
    }

    Types::TypeDecl* type = ParseType();
 
    return new Types::FileDecl(type);
}

Types::SetDecl* Parser::ParseSetDecl()
{
    if (!Expect(Token::Set, true))
    {
	return 0;
    }
    if (!Expect(Token::Of, true))
    {
	return 0;
    }

    Types::Range* r = ParseRangeOrTypeRange();
    if (!r)
    {
	return 0;
    }

    return new Types::SetDecl(r);
}

Types::StringDecl* Parser::ParseStringDecl()
{
    if (!Expect(Token::String, true))
    {
	return 0;
    }

    unsigned size = 255;

    if (CurrentToken().GetToken() == Token::LeftSquare)
    {
	NextToken();
	Token token = CurrentToken();
	if (token.GetToken() == Token::Identifier)
	{
	    Constants::ConstDecl* cd = GetConstDecl(token.GetIdentName());
	    if (cd)
	    {
		token = cd->Translate();
	    }
	}

	if (token.GetToken() != Token::Integer)
	{
	    Error("Expected integer value!");
	    return 0;
	}

	size = token.GetIntVal();
	
	NextToken();
	if (!Expect(Token::RightSquare, true))
	{
	    return 0;
	}
    }
    return new Types::StringDecl(size);
}

Types::TypeDecl* Parser::ParseType()
{
    Token::TokenType tt = CurrentToken().GetToken();
    if (tt == Token::Packed)
    {
	tt = NextToken().GetToken();
    }

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
    {
	Types::Range* r = ParseRange();
	return new Types::RangeDecl(r, (tt == Token::Char)?Types::Char:Types::Integer);
    }

    case Token::Array:
	return ParseArrayDecl();

    case Token::Record:
	return ParseRecordDecl();

    case Token::File:
	return ParseFileDecl();

    case Token::Set:
	return ParseSetDecl();
    
    case Token::LeftParen:
	return ParseEnumDef();

    case Token::Uparrow:
	return ParsePointerType();

    case Token::String:
	return ParseStringDecl();
	break;

    default:
	return ErrorT("Can't understand type");
    }
}

ExprAST* Parser::ParseIntegerExpr(Token token)
{
    long val = token.GetIntVal();
    ExprAST* result;
    if (val < std::numeric_limits<unsigned int>::min() || val > std::numeric_limits<unsigned int>::max())
    {
	result = new IntegerExprAST(val, GetTypeDecl("longint"));
    }
    else
    {
	result = new IntegerExprAST(val, GetTypeDecl("integer"));
    }
    NextToken();
    return result;
}

ExprAST* Parser::ParseCharExpr(Token token)
{
    ExprAST* result = new CharExprAST(token.GetIntVal(), GetTypeDecl("char"));
    NextToken();
    return result;
}

ExprAST* Parser::ParseRealExpr(Token token)
{
    ExprAST* result = new RealExprAST(token.GetRealVal(), GetTypeDecl("real"));
    NextToken();
    return result;
}

ExprAST* Parser::ParseStringExpr(Token token)
{
    std::vector<Types::Range*> rv;
    int len =  token.GetStrVal().length()-1;
    if (len < 1)
    {
	len = 1;
    }
    Types::Range* r = new Types::Range(0, len);
    rv.push_back(r);
    Types::ArrayDecl *ty = new Types::ArrayDecl(GetTypeDecl("char"), rv);
    ExprAST* result = new StringExprAST(token.GetStrVal(), ty);
    NextToken();
    return result;
}

ExprAST* Parser::ParseBinOpRHS(int exprPrec, ExprAST* lhs)
{
    for(;;)
    {
	int tokPrec = CurrentToken().Precedence();
	if (tokPrec < exprPrec)
	{
	    return lhs;
	}

	Token binOp = CurrentToken();
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
	    rhs = ParseBinOpRHS(tokPrec+1, rhs);
	    if (!rhs) 
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

    ExprAST* rhs = ParsePrimary();
    if (!rhs)
    {
	return 0;
    }
    // unary + = no change, so just return the expression.
    if (oper.GetToken() == Token::Plus)
    {
	return rhs;
    }
    return new UnaryExprAST(oper, rhs);
}

ExprAST* Parser::ParseExpression()
{
    ExprAST* lhs = ParsePrimary();
    if (!lhs)
    {
	return 0;
    }
    return ParseBinOpRHS(0, lhs);
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
    while(CurrentToken().GetToken() != Token::RightSquare)
    {
	ExprAST* index = ParseExpression();
	if (!index)
	{
	    return ErrorV("Expected index expression");
	}
	indices.push_back(index);
	if (indices.size() == adecl->Ranges().size())
	{
	    expr = new ArrayExprAST(expr, indices, adecl->Ranges(), adecl->SubType());
	    indices.clear();
	    type = adecl->SubType();
	    adecl = llvm::dyn_cast<Types::ArrayDecl>(type);
	}
	if (CurrentToken().GetToken() != Token::RightSquare)
	{
	    if (!Expect(Token::Comma, true) || !adecl)
	    {
		return 0;
	    }
	}
    }
    if (!Expect(Token::RightSquare, true))
    {
	return 0;
    }
    if (indices.size())
    {
	expr = new ArrayExprAST(expr, indices, adecl->Ranges(), adecl->SubType());
	type = adecl->SubType();
    }
    return expr;
}

VariableExprAST* Parser::ParseFieldExpr(VariableExprAST* expr, Types::TypeDecl*& type)
{
    if(!Expect(Token::Period, true))
    {
	return 0;
    }
    if (!Expect(Token::Identifier, false))
    {
	return 0;
    }

    Types::RecordDecl* rd = llvm::dyn_cast<Types::RecordDecl>(type);
    if (!rd)
    {
	return ErrorV("Attempt to use field of varaible that hasn't got fields");
    }
    
    std::string name = CurrentToken().GetIdentName();
    int elem = rd->Element(name);
    VariableExprAST* e = 0;
    
    if (elem >= 0)
    {
	type = rd->GetElement(elem).FieldType();
	e = new FieldExprAST(expr, elem, type);
    }
    else
    {
	// If the main structure doesn't have it, it may be a variant?
	Types::VariantDecl* v = rd->Variant();
	if (v)
	{
	    elem = v->Element(name);
	    if (elem >= 0)
	    {
		const Types::FieldDecl* fd = &v->GetElement(elem);
		type = fd->FieldType();
		e = new VariantFieldExprAST(expr, rd->FieldCount(), type);
		// If name is empty, we have a made up struct. Dig another level down. 
		if (fd->Name() == "")
		{
		    Types::RecordDecl* r = llvm::dyn_cast<Types::RecordDecl>(fd->FieldType()); 
		    assert(r && "Expect record declarataion");
		    elem = r->Element(name);
		    if (elem >= 0)
		    {
			type = r->GetElement(elem).FieldType();
			e = new FieldExprAST(e, elem, type);
		    }
		    else
		    {
			e = 0;
		    }
		}
	    }
	}
    }
    if (!e)
    {
	return ErrorV(std::string("Can't find element ") + name + " in record");
    }
    NextToken();
    return e;
}

VariableExprAST* Parser::ParsePointerExpr(VariableExprAST* expr, Types::TypeDecl*& type)
{
    if (!Expect(Token::Uparrow, true))
    {
	return 0;
    }
    if (type->Type() == Types::File)
    {
	type = type->SubType();
	return new FilePointerExprAST(expr, type);
    }

    type = type->SubType();
    return new PointerExprAST(expr, type);
}

bool Parser::IsCall(Types::TypeDecl* type)
{
    Types::SimpleTypes ty = type->Type();
    if (ty == Types::Pointer &&
	(type->SubType()->Type() == Types::Function ||
	 type->SubType()->Type() == Types::Procedure))
    {
	return true;
    }
    if ((ty == Types::Procedure || ty == Types::Function) && 
	CurrentToken().GetToken() != Token::Assign)
    {
	return true;
    }
    return false;
}

ExprAST* Parser::ParseIdentifierExpr()
{
    std::string idName = CurrentToken().GetIdentName();
    NextToken();
    NamedObject* def = nameStack.Find(idName);
    EnumDef *enumDef = llvm::dyn_cast_or_null<EnumDef>(def);
    if (enumDef)
    {
	return new IntegerExprAST(enumDef->Value(), enumDef->Type());
    }

    bool isBuiltin = Builtin::IsBuiltin(idName);
    if (!isBuiltin)
    {
	if (!def)
	{
	    return Error(std::string("Undefined name '") + idName + "'");
	}
	// If type is not function, not procedure, or the next thing is an assignment
	// then we want a "variable" with this name. 
	Types::TypeDecl* type = def->Type();
	assert(type && "Expect type here...");
	 
	if (!IsCall(type))
	{

	    VariableExprAST* expr;
	    if (WithDef *w = llvm::dyn_cast<WithDef>(def))
	    {
		expr = llvm::dyn_cast<VariableExprAST>(w->Actual());
	    }
	    else
	    {
		expr = new VariableExprAST(idName, type);
	    }
	    assert(expr && "Expected expression here");
	    // Ignore result - we may be adding the same variable 
	    // several times, but we don't really care.
	    usedVariables.Add(idName, def);

	    assert(type);

	    Token::TokenType tt = CurrentToken().GetToken();
	    while(tt == Token::LeftSquare || 
		  tt == Token::Uparrow || 
		  tt == Token::Period)
	    {
		assert(type && "Expect to have a type here...");
		switch(tt)
		{
		case Token::LeftSquare:
		    expr = ParseArrayExpr(expr, type);
		    break;

		case Token::Uparrow:
		    expr = ParsePointerExpr(expr, type);
		    break;

		case Token::Period:
		    expr = ParseFieldExpr(expr, type);
		    break;

		default:
		    assert(0);
		}
		tt = CurrentToken().GetToken();
	    }
	    return expr;	
	}
    }
    const FuncDef *funcDef = llvm::dyn_cast_or_null<const FuncDef>(def);
    std::vector<ExprAST* > args;
    if (CurrentToken().GetToken() == Token::LeftParen)
    {
	// Get past the '(' and fetch the next one. 
	if (!Expect(Token::LeftParen, true))
	{
	    return 0;
	}
	unsigned argNo = 0;
	while (CurrentToken().GetToken() != Token::RightParen)
	{
	    bool isFuncArg = false;
	    if (funcDef && funcDef->Proto())
	    {
		auto funcArgs = funcDef->Proto()->Args();
		if (argNo >= funcArgs.size())
		{
		    return Error("Too many arguments");
		}
		Types::TypeDecl* td = funcArgs[argNo].Type();
		if (td->Type() == Types::Pointer && 
		    (td->SubType()->Type() == Types::Function ||
		     td->SubType()->Type() == Types::Procedure))
		{
		    isFuncArg = true;
		}
	    }
	    ExprAST* arg;
	    if (isFuncArg)
	    {
		if (CurrentToken().GetToken() != Token::Identifier)
		{
		    return Error("Expected name of a function or procedure");
		}
		arg = new FunctionExprAST(CurrentToken().GetIdentName(), 
					  funcDef->Proto()->Args()[argNo].Type());
		NextToken();
	    }
	    else
	    {
		arg = ParseExpression();
	    }
	    if (!arg) 
	    {
		return 0;
	    }
	    args.push_back(arg);
	    if (CurrentToken().GetToken() == Token::Comma)
	    {
		NextToken();
	    }
	    else if (!Expect(Token::RightParen, false))
	    {
		return 0;
	    }
	    argNo++;
	}
	NextToken();
    }

    const PrototypeAST* proto = 0;
    ExprAST* expr = 0;
    if (funcDef)
    {
	proto = funcDef->Proto();
	expr = new FunctionExprAST(idName, funcDef->Type());
    }
    else if (def)
    {
	const VarDef* varDef = llvm::dyn_cast<const VarDef>(def);
	if (!varDef)
	{
	    assert(0 && "Expected variable definition!");
	    return 0;
	}
	if (def->Type()->Type() == Types::Pointer)
	{
	    Types::FuncPtrDecl* fp = llvm::dyn_cast<Types::FuncPtrDecl>(def->Type());
	    assert(fp && "Expected function pointer here...");
	    proto = fp->Proto();
	    expr = new VariableExprAST(idName, def->Type());
	}
    }
    if (expr)
    {
	FunctionAST* fn = proto->Function();
	if (fn)
	{
	    for(auto u : fn->UsedVars())
	    {
		args.push_back(new VariableExprAST(u.Name(), u.Type()));
	    }
	}
	return new CallExprAST(expr, args, proto);
    }

    assert(isBuiltin && "Should be a builtin function if we get here");
    Types::TypeDecl* ty = Builtin::Type(nameStack, idName, args);
    return new BuiltinExprAST(idName, args, ty);
}

ExprAST* Parser::ParseParenExpr()
{
    NextToken();
    ExprAST* v = ParseExpression();
    if (!v)
    {
	return 0;
    }
    
    if (!Expect(Token::RightParen, true))
    {
	return 0;
    }
    return v;
}

ExprAST* Parser::ParseSetExpr()
{
    if (!Expect(Token::LeftSquare, true))
    {
	return 0;
    }

    std::vector<ExprAST*> values;
    do
    {
	if (CurrentToken().GetToken() != Token::RightSquare)
	{
	    ExprAST* v = ParseExpression();
	    if (!v)
	    {
		return 0;
	    }
	    if (CurrentToken().GetToken() == Token::DotDot)
	    {
		NextToken();
		ExprAST* vEnd = ParseExpression();
		v = new RangeExprAST(v, vEnd);
	    }
	    values.push_back(v);
	}
	if (CurrentToken().GetToken() != Token::RightSquare)
	{
	    if (!Expect(Token::Comma, true))
	    {
		return 0;
	    }
	}
    } while(CurrentToken().GetToken() != Token::RightSquare);
    if (!Expect(Token::RightSquare, true))
    {
	return 0;
    }
    // TODO: Fix up type here... 
    return new SetExprAST(values, Types::TypeForSet());
}

VarDeclAST* Parser::ParseVarDecls()
{
    if (!Expect(Token::Var, true))
    {
	return 0;
    }

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
	NextToken();
	if (CurrentToken().GetToken() == Token::Colon)
	{
	    NextToken(); 
	    Types::TypeDecl* type = ParseType();
	    if (!type)
	    {
		return 0;
	    }
	    for(auto n : names)
	    {
		VarDef v(n, type);
		varList.push_back(v);
		if (!nameStack.Add(n, new VarDef(n, type)))
		{
		    Error(std::string("Name ") + n + " is already defined");
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
	    if (!Expect(Token::Comma, true))
	    {
		return 0;
	    }
	}
    } while(CurrentToken().GetToken() == Token::Identifier);
    
    return new VarDeclAST(varList);
}

// if isFunction:
// functon name( { [var] name1, [,name2 ...]: type [; ...] } ) : type
// if !isFunction:
// procedure name ( { [var] name1 [,name2 ...]: type [; ...] } ) : type
PrototypeAST* Parser::ParsePrototype(bool isFunction)
{
    // Consume "function" or "procedure"
    assert(CurrentToken().GetToken() == Token::Procedure ||
	   CurrentToken().GetToken() == Token::Function && 
	   "Expected function or procedure token");
    NextToken();
    std::string funcName = CurrentToken().GetIdentName();
    // Get function name.
    if (!Expect(Token::Identifier, true))
    {
	return 0;
    }
    std::vector<VarDef> args;
    if (CurrentToken().GetToken() == Token::LeftParen)
    {
	std::vector<std::string> names;
	NextToken();
	while(CurrentToken().GetToken() != Token::RightParen)
	{
	    bool isRef = false;
	    if (CurrentToken().GetToken() == Token::Function ||
		CurrentToken().GetToken() == Token::Procedure)
	    {
		PrototypeAST* proto = ParsePrototype(CurrentToken().GetToken() == Token::Function);
		Types::TypeDecl* type = new Types::FuncPtrDecl(proto);
		VarDef v(proto->Name(), type, false);
		args.push_back(v);
	    }
	    else
	    {
		if (CurrentToken().GetToken() == Token::Var)
		{
		    isRef = true;
		    NextToken();
		}
		if (!Expect(Token::Identifier, false))
		{
		    return 0;
		}
		std::string arg = CurrentToken().GetIdentName();
		NextToken();

		names.push_back(arg);
		if (CurrentToken().GetToken() == Token::Colon)
		{
		    NextToken();
		    Types::TypeDecl* type = ParseType();
		    for(auto n : names)
		    {
			VarDef v(n, type, isRef);
			args.push_back(v);
		    }
		    names.clear();
		    if (CurrentToken().GetToken() != Token::RightParen)
		    {
			if (!Expect(Token::Semicolon, true))
			{
			    return 0;
			}
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
	// Eat ')' at end of argument list.
	NextToken();
    }

    // If we have a function, expect ": type".
    if (isFunction)
    {
	if (!Expect(Token::Colon, true))
	{
	    return 0;
	}
	Types::TypeDecl* resultType = ParseSimpleType();
	if (!Expect(Token::Semicolon, true))
	{
	    return 0;
	}
	return new PrototypeAST(funcName, args, resultType);
    }
	
    if (!Expect(Token::Semicolon, true))
    {
	return 0;
    }
    return new PrototypeAST(funcName, args);
}

ExprAST* Parser::ParseStatement()
{
    ExprAST* expr = ParsePrimary();
    if (CurrentToken().GetToken() == Token::Assign)
    {
	NextToken();
	ExprAST* rhs = ParseExpression();
	expr = new AssignExprAST(expr, rhs);
    }
    return expr;
}

BlockAST* Parser::ParseBlock()
{
    if (!Expect(Token::Begin, true))
    {
	return 0;
    }
    
    std::vector<ExprAST*> v;
    // Build ast of the content of the block.
    while(CurrentToken().GetToken() != Token::End)
    {
	ExprAST* ast = ParseStatement();
	if (!ast)
	{
	    return 0;
	}
	v.push_back(ast);
	if (!ExpectSemicolonOrEnd())
	{
	    return 0;
	}
    }
    if (!Expect(Token::End, true))
    {
	return 0;
    }
    return new BlockAST(v);
}

FunctionAST* Parser::ParseDefinition()
{
    bool isFunction = CurrentToken().GetToken() == Token::Function;
    PrototypeAST* proto = ParsePrototype(isFunction);
    if (!proto) 
    {
	return 0;
    }
    std::string      name = proto->Name();
    Types::TypeDecl* ty = new Types::TypeDecl(isFunction?Types::Function:Types::Procedure);
    NamedObject*     nmObj = new FuncDef(name, ty, proto);

    const NamedObject* def = nameStack.Find(name);
    const FuncDef *fnDef = llvm::dyn_cast_or_null<const FuncDef>(def);
    if (!(fnDef && fnDef->Proto() && fnDef->Proto()->IsForward()))
    {
	if (!nameStack.Add(name, nmObj))
	{
	    return ErrorF(std::string("Name '") + name + "' already exists...");
	}
	
	if (CurrentToken().GetToken() == Token::Forward)
	{
	    NextToken();
	    proto->SetIsForward(true);
	    return new FunctionAST(proto, 0, 0);
	}
    }
    
    NameWrapper wrapper(nameStack);
    NameWrapper usedWrapper(usedVariables);
    for(auto v : proto->Args())
    {
	if (!nameStack.Add(v.Name(), new VarDef(v.Name(), v.Type())))
	{
	    return ErrorF(std::string("Duplicate name ") + v.Name()); 
	}
    }

    VarDeclAST*               varDecls = 0;
    BlockAST*                 body = 0;
    bool                      typeDecls = false;
    bool                      constDecls = false;
    std::vector<FunctionAST*> subFunctions;
    do
    {
	switch(CurrentToken().GetToken())
	{
	case Token::Var:
	    if (varDecls)
	    {
		return ErrorF("Can't declare variables multiple times");
	    }
	    varDecls = ParseVarDecls();
	    break;

	case Token::Type:
	    if (typeDecls)
	    {
		return ErrorF("Can't declare types multiple times");
	    }
	    ParseTypeDef();
	    typeDecls = true;
	    break;

	case Token::Const:
	    if (constDecls)
	    {
		return ErrorF("Can't declare const multiple times");
	    }
	    ParseConstDef();
	    constDecls = true;
	    break;

	case Token::Function:
	case Token::Procedure:
	{
	    FunctionAST* fn = ParseDefinition();
	    subFunctions.push_back(fn);
	    break;
	}
	   
	case Token::Begin:
	{
	    if (body)
	    {
		return ErrorF("Multiple body declarations for function?");
	    }
	    body = ParseBlock();
	    if (!body)
	    {
		return 0;
	    }
	    if (!Expect(Token::Semicolon, true))
	    {
		return 0;
	    }

	    FunctionAST* fn = new FunctionAST(proto, varDecls, body);
	    for(auto s : subFunctions)
	    {
		s->SetParent(fn);
	    }
	    // Need to add subFunctions before setting used vars!
	    fn->AddSubFunctions(subFunctions);
	    fn->SetUsedVars(usedVariables.GetLevel(), 
			    nameStack.GetLevel(),
			    nameStack.GetLevel(0));
	    proto->AddExtraArgs(fn->UsedVars()); 
	    return fn;
	}

	default:
	    assert(0 && "Unexpected token");
	    return ErrorF("Unexpected token");
	}
    } while(1);
    return 0;
}

ExprAST* Parser::ParseStmtOrBlock()
{
    switch(CurrentToken().GetToken())
    {
    case Token::Begin:
	return ParseBlock();

    case Token::Semicolon:
    case Token::End:
	// Empty block.
	return new BlockAST(std::vector<ExprAST*>());

    default:
	return ParseStatement();
    }
}

ExprAST* Parser::ParseIfExpr()
{
    if (!Expect(Token::If, true))
    {
	assert(0 && "Huh? Expected if");
    }
    ExprAST* cond = ParseExpression();
    if (!cond || !Expect(Token::Then, true))
    {
	return 0;
    }

    ExprAST* then = 0;
    if (CurrentToken().GetToken() != Token::Else)
    {
	then = ParseStmtOrBlock();
	if (!then)
	{
	    return 0;
	}
    }

    ExprAST* elseExpr = 0;
    if (CurrentToken().GetToken() == Token::Else)
    {
	if (Expect(Token::Else, true))
	{
	    elseExpr = ParseStmtOrBlock();
	    if (!elseExpr)
	    {
		return 0;
	    }
	}
    }
    return new IfExprAST(cond, then, elseExpr);
}

ExprAST* Parser::ParseForExpr()
{
    if (!Expect(Token::For, true))
    {
	assert(0 && "Huh? Expected for");
    }
    if (CurrentToken().GetToken() != Token::Identifier)
    {
	return Error("Expected identifier name, got " + CurrentToken().ToString());
    }
    std::string varName = CurrentToken().GetIdentName();
    NextToken();
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
    ExprAST* body = ParseStmtOrBlock();
    if (!body)
    {
	return 0;
    }
    return new ForExprAST(varName, start, end, down, body);
}

ExprAST* Parser::ParseWhile()
{
    NextToken();

    ExprAST* cond = ParseExpression();
    if (!cond || !Expect(Token::Do, true))
    {
	return 0;
    }
    ExprAST* body = ParseStmtOrBlock();

    return new WhileExprAST(cond, body);
}

ExprAST* Parser::ParseRepeat()
{
    NextToken();
    std::vector<ExprAST*> v;
    while(CurrentToken().GetToken() != Token::Until)
    {
	ExprAST* stmt = ParseStatement();
	if (!stmt)
	{
	    return 0;
	}
	v.push_back(stmt);
	if(CurrentToken().GetToken() == Token::Semicolon)
	{
	    NextToken();
	}
    }
    if (!Expect(Token::Until, true))
    {
	return 0;
    }
    ExprAST* cond = ParseExpression();
    return new RepeatExprAST(cond, new BlockAST(v));
}

ExprAST* Parser::ParseCaseExpr()
{
    if (!Expect(Token::Case, true))
    {
	return 0;
    }
    ExprAST* expr = ParseExpression();
    if (!Expect(Token::Of, true))
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
	{
	    EnumDef* ed = GetEnumValue(CurrentToken().GetIdentName());
	    if (ed)
	    {
		lab.push_back(ed->Value());
	    }
	    else
	    {
		return Error("Expected enumerated type value");
	    }
	    break;
	}

	case Token::Otherwise:
	    if (otherwise)
	    {
		return Error("Otherwise already used in this case block");
	    }
	    isOtherwise = true;
	    break;

	default:
	    return Error("Syntax error, expected case label");
	}
	NextToken();
	switch(CurrentToken().GetToken())
	{
	case Token::Comma:
	    if (isOtherwise)
	    {
		return Error("Can't have multiple case labels with otherwise case label");
	    }
	    NextToken();
	    break;

	case Token::Colon:
	{
	    NextToken();
	    ExprAST* s = ParseStmtOrBlock();
	    if (isOtherwise)
	    {
		otherwise = s;
		if (lab.size())
		{
		    return Error("Can't have multiple case labels with otherwise case label");
		}
	    }
	    else
	    {
		labels.push_back(new LabelExprAST(lab, s));
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
    } while(CurrentToken().GetToken() != Token::End);
    if (!Expect(Token::End, true))
    {
	return 0;
    }
    return new CaseExprAST(expr, labels, otherwise);
}

void Parser::ExpandWithNames(const Types::FieldCollection* fields, VariableExprAST* v, int parentCount)
{
    int count = fields->FieldCount();
    for(int i = 0; i < count; i++)
    {
	const Types::FieldDecl f = fields->GetElement(i);
	Types::TypeDecl* ty = f.FieldType();
	if (f.Name() == "")
	{
	    Types::RecordDecl* rd = llvm::dyn_cast<Types::RecordDecl>(ty);
	    assert(rd && "Expected record declarataion here!");
	    ExpandWithNames(rd, new VariantFieldExprAST(v, parentCount, ty), 0);
	}
	else
	{
	    ExprAST* e;
	    if (llvm::isa<Types::RecordDecl>(fields))
	    {
		e = new FieldExprAST(v, i, ty);
	    }
	    else
	    {
		e = new VariantFieldExprAST(v, parentCount, ty);
	    }
	    nameStack.Add(f.Name(), new WithDef(f.Name(), e, f.FieldType()));
	}
    }
}

ExprAST* Parser::ParseWithBlock()
{
    if (!Expect(Token::With, true))
    {
	return 0;
    }
    std::vector<VariableExprAST*> vars;
    do
    {
	ExprAST* e = ParseIdentifierExpr();
	VariableExprAST* v = llvm::dyn_cast<VariableExprAST>(e);
	if (!v)
	{
	    return Error("With statement must contain only variable expression");
	}
	vars.push_back(v);
	if (CurrentToken().GetToken() != Token::Do)
	{
	    if (!Expect(Token::Comma, true))
	    {
		return 0;
	    }
	}
    } while(CurrentToken().GetToken() != Token::Do);
    if (!Expect(Token::Do, true))
    {
	return 0;
    }
    
    NameWrapper wrapper(nameStack);
    for(auto v : vars)
    {
	Types::RecordDecl* rd = llvm::dyn_cast<Types::RecordDecl>(v->Type());
	if(!rd)
	{
	    return Error("Type for with statement should be a record type");
	}
	ExpandWithNames(rd, v, 0);
	Types::VariantDecl* variant = rd->Variant();
	if (variant)
	{
	    ExpandWithNames(variant, v, rd->FieldCount());
	}
    }
    ExprAST* body = ParseStmtOrBlock();
    return new WithExprAST(body);
}

ExprAST* Parser::ParseWrite()
{
    bool isWriteln = CurrentToken().GetToken() == Token::Writeln;

    assert(CurrentToken().GetToken() == Token::Write ||
	   CurrentToken().GetToken() == Token::Writeln &&
	   "Expected write or writeln keyword here");
    NextToken();

    VariableExprAST* file = 0;
    std::vector<WriteAST::WriteArg> args;
    if (CurrentToken().GetToken() == Token::Semicolon || CurrentToken().GetToken() == Token::End)
    {
	if (!isWriteln)
	{
	    return Error("Write must have arguments.");
	}
    }
    else
    {
	if (!Expect(Token::LeftParen, true))
	{
	    return 0;
	}
	
	while(CurrentToken().GetToken() != Token::RightParen)
	{
	    WriteAST::WriteArg wa;
	    wa.expr = ParseExpression();
	    if (!wa.expr)
	    {
		return 0;
	    }
	    if (args.size() == 0)
	    {
		VariableExprAST* vexpr = llvm::dyn_cast<VariableExprAST>(wa.expr);
		if (vexpr)
		{
		    if (vexpr->Type()->Type() == Types::File)
		    {
			file = vexpr;
			wa.expr = 0;
		    }
		}
	    }
	    if (wa.expr)
	    {
		if (CurrentToken().GetToken() == Token::Colon)
		{
		    NextToken();
		    wa.width = ParseExpression();
		    if (!wa.width)
		    {
			return Error("Invalid width expression");
		    }
		}
		if (CurrentToken().GetToken() == Token::Colon)
		{
		    NextToken();
		    wa.precision = ParseExpression();
		    if (!wa.precision)
		    {
			return Error("Invalid precision expression");
		    }
		}
		args.push_back(wa);
	    }
	    if (CurrentToken().GetToken() != Token::RightParen)
	    {
		if (!Expect(Token::Comma, true))
		{
		    return 0;
		}
	    }
	}
	if(!Expect(Token::RightParen, true))
	{
	    return 0;
	}
	if (args.size() < 1 && !isWriteln)
	{
	    return Error("Expected at least one expression for output in write");
	}
    }
    return new WriteAST(file, args, isWriteln);
}

ExprAST* Parser::ParseRead()
{
    bool isReadln = CurrentToken().GetToken() == Token::Readln;

    assert(CurrentToken().GetToken() == Token::Read ||
	   CurrentToken().GetToken() == Token::Readln &&
	   "Expected read or readln keyword here");
    NextToken();

    std::vector<ExprAST*> args;
    VariableExprAST* file = 0;
    if (CurrentToken().GetToken() == Token::Semicolon || CurrentToken().GetToken() == Token::End)
    {
	if (!isReadln)
	{
	    return Error("Read must have arguments.");
	}
    }
    else
    {
	if (!Expect(Token::LeftParen, true))
	{
	    return 0;
	}
	while(CurrentToken().GetToken() != Token::RightParen)
	{
	    ExprAST* expr = ParseExpression();
	    if (!expr)
	    {
		return 0;
	    }
	    if (args.size() == 0)
	    {
		VariableExprAST* vexpr = llvm::dyn_cast<VariableExprAST>(expr);
		if (vexpr)
		{
		    if (vexpr->Type()->Type() == Types::File)
		    {
			file = vexpr;
			expr = 0;
		    }
		}
	    }
	    if (expr)
	    {
		args.push_back(expr);
	    }
	    if (CurrentToken().GetToken() != Token::RightParen)
	    {
		if (!Expect(Token::Comma, true))
		{
		    return 0;
		}
	    }
	}
	if(!Expect(Token::RightParen, true))
	{
	    return 0;
	}
	if (args.size() < 1 && !isReadln)
	{
	    return Error("Expected at least one variable in read statement");
	}
    }
    return new ReadAST(file, args, isReadln);
}

ExprAST* Parser::ParsePrimary()
{
    Token token = CurrentToken();
    if (token.GetToken() == Token::Identifier)
    {
	Constants::ConstDecl* cd = GetConstDecl(token.GetIdentName());
	if (cd)
	{
	    token = cd->Translate();
	}
    }

    switch(token.GetToken())
    {
    case Token::Nil:
	return ParseNilExpr();

    case Token::Real:
	return ParseRealExpr(token);

    case Token::Integer:
	return ParseIntegerExpr(token);

    case Token::Char:
	return ParseCharExpr(token);

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

    default:
	CurrentToken().dump(std::cerr);
	assert(0 && "Unexpected token");
	return 0;
    }
}

bool Parser::ParseProgram()
{
    if (!Expect(Token::Program, true))
    {
	return false;
    }
    if (!Expect(Token::Identifier, false))
    {
	return false;
    }
    moduleName = CurrentToken().GetIdentName();
    NextToken();
    if (CurrentToken().GetToken() == Token::LeftParen)
    {
	NextToken();
	do
	{
	    if (!Expect(Token::Identifier, true))
	    {
		return false;
	    }
	    if (CurrentToken().GetToken() != Token::RightParen)
	    {
		if (!Expect(Token::Comma, true))
		{
		    return false;
		}
	    }
	} while(CurrentToken().GetToken() != Token::RightParen);
	NextToken();
    }
    return true;
}

std::vector<ExprAST*> Parser::Parse()
{
    std::vector<ExprAST*> v;
    NextToken();
    if(!ParseProgram())
    {
	return v;
    }
    for(;;)
    {
	ExprAST* curAst = 0;
	switch(CurrentToken().GetToken())
	{
	case Token::EndOfFile:
	    // TODO: Is this not an error?
	    return v;
	    
	case Token::Semicolon:
	    NextToken();
	    break;

	case Token::Function:
	case Token::Procedure:
	    curAst = ParseDefinition();
	    break;

	case Token::Var:
	    curAst = ParseVarDecls();
	    break;

	case Token::Type:
	    ParseTypeDef();   
	    // Generates no AST, so need to "continue" here
	    continue;

	case Token::Const:
	    ParseConstDef();
	    // No AST from constdef, so continue.
	    continue;
	    
	case Token::Begin:
	{
	    BlockAST* body = ParseBlock();
	    // Parse the "main" of the program - we call that
	    // "__PascalMain" so we can call it from C-code.
	    PrototypeAST* proto = new PrototypeAST("__PascalMain", std::vector<VarDef>());
	    FunctionAST* fun = new FunctionAST(proto, 0, body);
	    curAst = fun;
	    if (!Expect(Token::Period, true))
	    {
		v.clear();
		return v;
	    }
	    break;
	}

	default:
	    curAst = ParseExpression();
	    break;
	}

	if (curAst)
	{
	    v.push_back(curAst);
	}
    }
}
