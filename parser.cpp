#include "lexer.h"
#include "parser.h"
#include "expr.h"
#include "namedobject.h"
#include "stack.h"
#include "builtin.h"
#include <iostream>
#include <cassert>

#define TRACE() std::cerr << __FILE__ << ":" << __LINE__ << "::" << __PRETTY_FUNCTION__ << std::endl

Parser::Parser(Lexer &l) 
    : 	lexer(l), nextTokenValid(false), errCnt(0)
{
    nameStack.NewLevel();
    if (!(AddType("integer", new Types::TypeDecl(Types::Integer)) &&
	  AddType("real", new Types::TypeDecl(Types::Real)) &&
	  AddType("char", new Types::TypeDecl(Types::Char)) &&
	  AddType("boolean", new Types::TypeDecl(Types::Boolean))))
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
    curToken.Dump(std::cout, file, line);
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
    std::cout << "peeking: ";
    nextToken.Dump(std::cout, file, line);
    return nextToken;
}

bool Parser::Expect(Token::TokenType type, bool eatIt, const char* file, int line)
{
    if (CurrentToken().GetType() != type)
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

#define NextToken() NextToken(__FILE__, __LINE__)
#define PeekToken() PeekToken(__FILE__, __LINE__)
#define Expect(t, e) Expect(t, e, __FILE__, __LINE__)

Types::TypeDecl* Parser::GetTypeDecl(const std::string& name)
{
    NamedObject* def = nameStack.Find(name);
    if (!def) 
    {
	return 0;
    }
    TypeDef *typeDef = dynamic_cast<TypeDef*>(def);
    if (!typeDef)
    {
	return 0;
    }
    return typeDef->Type();
}

Constants::ConstDecl* Parser::GetConstDecl(const std::string& name)
{
    NamedObject* def = nameStack.Find(name);
    if (!def) 
    {
	return 0;
    }
    ConstDef *constDef = dynamic_cast<ConstDef*>(def);
    if (!constDef)
    {
	return 0;
    }
    return constDef->ConstValue();
}

bool Parser::GetEnumValue(const std::string& name, int& value)
{
    NamedObject* def = nameStack.Find(name);
    if (!def) 
    {
	return false;
    }
    EnumDef *enumDef = dynamic_cast<EnumDef*>(def);
    if (!enumDef)
    {
	return false;
    }
    value = enumDef->Value();
    return true;
}

bool Parser::AddType(const std::string& name, Types::TypeDecl* ty)
{
    Types::EnumDecl* ed = dynamic_cast<Types::EnumDecl*>(ty);
    if (ed)
    {
	for(auto v : ed->Values())
	{
	    if (!nameStack.Add(v.name, new EnumDef(v.name, v.value)))
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
    if (CurrentToken().GetType() != Token::Identifier)
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

Types::Range* Parser::ParseRange()
{
    Token::TokenType tt = CurrentToken().GetType();
    
    if (tt == Token::Integer || tt == Token::Char)
    {
	int start = CurrentToken().GetIntVal();
	NextToken();
        if (!Expect(Token::DotDot, true))
	{
	    return 0;
	}
	if (!Expect(tt, false))
	{
	    return 0;
	}
	int end = CurrentToken().GetIntVal();
	NextToken();

	return new Types::Range(start, end);
    }
    else if (tt == Token::Identifier)
    {
	int start;
	int end;
	if (!GetEnumValue(CurrentToken().GetIdentName(), start))
	{
	    return ErrorR("Invalid range specification, expected identifier for enumerated type");
	}
	NextToken();
        if (!Expect(Token::DotDot, true))
	{
	    return 0;
	}
	if (CurrentToken().GetType() != Token::Identifier ||
	    !GetEnumValue(CurrentToken().GetIdentName(), end))
	{
	    return ErrorR("Invalid range specification, expected identifier for enumerated type");
	}
	NextToken();
	return new Types::Range(start, end);
    }
    else
    {
	return ErrorR("Invalid range specification");
    }
}

Types::Range* Parser::ParseRangeOrTypeRange()
{
    if (CurrentToken().GetType() == Token::Identifier)
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
	switch(CurrentToken().GetType())
	{
	case Token::String:
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

	case Token::True:
	    cd = new Constants:: BoolConstDecl(loc, true);
	    break;

	case Token::False:
	    cd = new Constants:: BoolConstDecl(loc, false);
	    break;

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
    } while (CurrentToken().GetType() == Token::Identifier);
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
	    Types::PointerDecl* pd = dynamic_cast<Types::PointerDecl*>(ty);
	    incomplete.push_back(pd);
	}	    
	if (!Expect(Token::Semicolon, true))
	{
	    return;
	}
    } while (CurrentToken().GetType() == Token::Identifier);

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
    while(CurrentToken().GetType() != Token::RightParen)
    {
	if (!Expect(Token::Identifier, false))
	{
	    return 0;
	}
	values.push_back(CurrentToken().GetIdentName());
	NextToken();
	if (CurrentToken().GetType() != Token::RightParen)
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
    if (CurrentToken().GetType() == Token::Identifier)
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
    while(CurrentToken().GetType() != Token::RightSquare)
    {
	Types::Range* r = ParseRangeOrTypeRange();
	if (!r) 

	{
	    return 0;
	}
	rv.push_back(r);
	if (CurrentToken().GetType() == Token::Comma)
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

Types::RecordDecl* Parser::ParseRecordDecl()
{
    if (!Expect(Token::Record, true))
    {
	return 0;
    }
    std::vector<Types::FieldDecl> fields;
    do
    {
	std::vector<std::string> names;
	do
	{
	    if (!Expect(Token::Identifier, false))
	    {
		return 0;
	    }
	    names.push_back(CurrentToken().GetIdentName());
	    NextToken();
	    if (CurrentToken().GetType() != Token::Colon)
	    {
		if (!Expect(Token::Comma, true))
		{
		    return 0;
		}
	    }
	} while(CurrentToken().GetType() != Token::Colon);
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
	if (!Expect(Token::Semicolon, true))
	{
	    return 0;
	}
    } while(CurrentToken().GetType() != Token::End);
    NextToken();
    if (fields.size() == 0)
    {
	return 0;
    }
    return new Types::RecordDecl(fields);
}

Types::TypeDecl* Parser::ParseType()
{
    Token::TokenType tt = CurrentToken().GetType();

    switch(tt)
    {
    case Token::Identifier:
    {
	int dummy;
	if (!GetEnumValue(CurrentToken().GetIdentName(), dummy))
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

    
    case Token::LeftParen:
	return ParseEnumDef();

    case Token::Uparrow:
	return ParsePointerType();

    default:
	return ErrorT("Can't understand type");
    }
}

ExprAST* Parser::ParseIntegerExpr(Token token)
{
    ExprAST* result = new IntegerExprAST(token.GetIntVal());
    NextToken();
    return result;
}

ExprAST* Parser::ParseCharExpr(Token token)
{
    ExprAST* result = new CharExprAST(token.GetIntVal());
    NextToken();
    return result;
}

ExprAST* Parser::ParseRealExpr(Token token)
{
    ExprAST* result = new RealExprAST(token.GetRealVal());
    NextToken();
    return result;
}

ExprAST* Parser::ParseStringExpr(Token token)
{
    ExprAST* result = new StringExprAST(token.GetStrVal());
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
    assert(CurrentToken().GetType() == Token::Minus && 
	   "Expected only minus at this time as a unary operator");

    Token oper = CurrentToken();

    NextToken();
    
    ExprAST* rhs = ParsePrimary();
    if (!rhs)
    {
	return 0;
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


VariableExprAST* Parser::ParseArrayExpr(VariableExprAST* expr, const Types::TypeDecl* type)
{
    const Types::ArrayDecl* adecl = 
	dynamic_cast<const Types::ArrayDecl*>(type); 
    if (!adecl)
    {
	return ErrorV("Expected variable of array type when using index");
    }
    NextToken();
    std::vector<ExprAST*> indices;
    while(CurrentToken().GetType() != Token::RightSquare)
    {
	ExprAST* index = ParseExpression();
	if (!index)
	{
	    return ErrorV("Expected index expression");
	}
	indices.push_back(index);
	if (CurrentToken().GetType() != Token::RightSquare)
	{
	    if (!Expect(Token::Comma, true))
	    {
		return 0;
	    }
	}
    }
    assert(indices.size() ==  adecl->Ranges().size() && 
	   "Expected same number of indices as declared subscripts");
    if (!Expect(Token::RightSquare, true))
    {
	return 0;
    }
    return new ArrayExprAST(expr, indices, adecl->Ranges());
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
    Types::RecordDecl* rd = dynamic_cast<Types::RecordDecl*>(type);
    std::string name = CurrentToken().GetIdentName();
    int elem = rd->Element(name);
    if (elem < 0)
    {
	return ErrorV(std::string("Can't find element ") + name + " in record");
    }
    type = rd->GetElement(elem).FieldType();
    NextToken();
    return new FieldExprAST(expr, elem);
}

bool Parser::IsCall(Types::TypeDecl* type)
{
    if (type->Type() == Types::Pointer &&
	(type->SubType()->Type() == Types::Function ||
	 type->SubType()->Type() == Types::Procedure))
    {
	return true;
    }
    if ((type->Type() == Types::Procedure || 
	 type->Type() == Types::Function) && 
	CurrentToken().GetType() != Token::Assign)
    {
	return true;
    }
    return false;
}

ExprAST* Parser::ParseIdentifierExpr()
{
    std::string idName = CurrentToken().GetIdentName();
    NextToken();
    /* TODO: Should we add builtin's to names at global level? */
    const NamedObject* def = nameStack.Find(idName);
    const EnumDef *enumDef = dynamic_cast<const EnumDef*>(def);
    if (enumDef)
    {
	return new IntegerExprAST(enumDef->Value());
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
	    VariableExprAST* expr = new VariableExprAST(idName);

	    assert(type);

	    Token::TokenType tt = CurrentToken().GetType();
	    while(tt == Token::LeftSquare || 
		  tt == Token::Uparrow || 
		  tt == Token::Period)
	    {
		assert(type && "Expect to have a type here...");
		switch(tt)
		{
		case Token::LeftSquare:
		    expr = ParseArrayExpr(expr, type);
		    type = type->SubType();
		    break;

		case Token::Uparrow:
		    NextToken();
		    expr = new PointerExprAST(expr);
		    type->dump();
		    type = type->SubType();
		    break;

		case Token::Period:
		    expr = ParseFieldExpr(expr, type);
		    break;

		default:
		    assert(0);
		}
		tt = CurrentToken().GetType();
	    }
	    return expr;	
	}
    }
    // Get past the '(' and fetch the next one. 
    const FuncDef *funcDef = dynamic_cast<const FuncDef*>(def);
    std::vector<ExprAST* > args;
    if (CurrentToken().GetType() == Token::LeftParen)
    {
	if (!Expect(Token::LeftParen, true))
	{
	    return 0;
	}
	unsigned argNo = 0;
	while (CurrentToken().GetType() != Token::RightParen)
	{
	    bool isFuncArg = false;
	    if (funcDef && funcDef->Proto())
	    {
		Types::TypeDecl* td = funcDef->Proto()->Args()[argNo].Type();
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
		if (CurrentToken().GetType() != Token::Identifier)
		{
		    return Error("Expected name of a function or procedure");
		}
		arg = new FunctionExprAST(CurrentToken().GetIdentName());
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
	    if (CurrentToken().GetType() == Token::Comma)
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
	expr = new FunctionExprAST(idName);
	return new CallExprAST(expr, args, proto);
    }
    else if (def)
    {
	const VarDef* varDef = dynamic_cast<const VarDef*>(def);
	if (!varDef)
	{
	    assert(0 && "Expected variable definition!");
	    return 0;
	}
	if (def->Type()->Type() == Types::Pointer)
	{
	    Types::FuncPtrDecl* fp = dynamic_cast<Types::FuncPtrDecl*>(def->Type());
	    assert(fp && "Expected function pointer here...");
	    proto = fp->Proto();
	    expr = new VariableExprAST(idName);
	    return new CallExprAST(expr, args, proto);
	}
    }

    assert(isBuiltin && "Should be a builtin function if we get here");
    return new BuiltinExprAST(idName, args);
}

ExprAST* Parser::ParseParenExpr()
{
    NextToken();
    ExprAST* V = ParseExpression();
    if (!V) 
    {
	return 0;
    }
    
    if (!Expect(Token::RightParen, true))
    {
	return 0;
    }
    return V;
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
	if (CurrentToken().GetType() == Token::Colon)
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
    } while(CurrentToken().GetType() == Token::Identifier);
    
    return new VarDeclAST(varList);
}

// if isFunction:
// functon name( { [var] name1, [,name2 ...]: type [; ...] } ) : type
// if !isFunction:
// procedure name ( { [var] name1 [,name2 ...]: type [; ...] } ) : type
PrototypeAST* Parser::ParsePrototype(bool isFunction)
{
    // Consume "function" or "procedure"
    assert(CurrentToken().GetType() == Token::Procedure ||
	   CurrentToken().GetType() == Token::Function && 
	   "Expected function or procedure token");
    NextToken();
    std::string funcName = CurrentToken().GetIdentName();
    // Get function name.
    if (!Expect(Token::Identifier, true))
    {
	return 0;
    }
    std::vector<VarDef> args;
    if (CurrentToken().GetType() == Token::LeftParen)
    {
	std::vector<std::string> names;
	NextToken();
	while(CurrentToken().GetType() != Token::RightParen)
	{
	    bool isRef = false;
	    if (CurrentToken().GetType() == Token::Function ||
		CurrentToken().GetType() == Token::Procedure)
	    {
		PrototypeAST* proto = ParsePrototype(CurrentToken().GetType() == Token::Function);
		Types::TypeDecl* type = new Types::FuncPtrDecl(proto);
		VarDef v(proto->Name(), type, false);
		args.push_back(v);
	    }
	    else
	    {
		if (CurrentToken().GetType() == Token::Var)
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
		if (CurrentToken().GetType() == Token::Colon)
		{
		    NextToken();
		    Types::TypeDecl* type = ParseSimpleType();
		    for(auto n : names)
		    {
			VarDef v(n, type, isRef);
			args.push_back(v);
		    }
		    names.clear();
		    if (CurrentToken().GetType() != Token::RightParen)
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
    if (CurrentToken().GetType() == Token::Assign)
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
    
    ExprAST* astHead = 0;
    ExprAST* astTail = 0;
    // Build ast of the content of the block.
    while(CurrentToken().GetType() != Token::End)
    {
	ExprAST* ast = ParseStatement();
	if (ast)
	{
	    if (!astHead)
	    {
		astHead = astTail = ast;
	    }
	    else
	    {
		astTail = astTail->SetNext(ast);
	    }
	}
	if (!Expect(Token::Semicolon, true))
	{
	    return 0;
	}
    }
    if (!Expect(Token::End, true))
    {
	return 0;
    }
    return new BlockAST(astHead);
}

FunctionAST* Parser::ParseDefinition()
{
    bool isFunction = CurrentToken().GetType() == Token::Function;
    PrototypeAST* proto = ParsePrototype(isFunction);
    if (!proto) 
    {
	return 0;
    }
    std::string name = proto->Name();
    Types::TypeDecl* ty = new Types::TypeDecl(isFunction?Types::Function:Types::Procedure);
    NamedObject* nmObj = new FuncDef(name, ty, proto);

    const NamedObject* def = nameStack.Find(name);
    const FuncDef *fnDef = dynamic_cast<const FuncDef*>(def);
    if (!(fnDef && fnDef->Proto() && fnDef->Proto()->IsForward()))
    {
	if (!nameStack.Add(name, nmObj))
	{
	    return ErrorF(std::string("Name '") + name + "' already exists...");
	}

	if (CurrentToken().GetType() == Token::Forward)
	{
	    NextToken();
	    proto->SetIsForward(true);
	    return new FunctionAST(proto, 0, 0);
	}
    }
    
    NameWrapper wrapper(nameStack);
    for(auto v : proto->Args())
    {
	if (!nameStack.Add(v.Name(), new VarDef(v.Name(), v.Type())))
	{
	    return ErrorF(std::string("Duplicate name ") + v.Name()); 
	}
    }

    VarDeclAST* varDecls = 0;
    BlockAST* body = 0;
    bool typeDecls = false;
    bool constDecls = false;
    do
    {
	switch(CurrentToken().GetType())
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
    if (CurrentToken().GetType() == Token::Begin)
    {
	return ParseBlock();
    }
    return ParseStatement();
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

    ExprAST* then = ParseStmtOrBlock();
    if (!then)
    {
	return 0;
    }

    ExprAST* elseExpr = 0;
    if (CurrentToken().GetType() != Token::Semicolon)
    {
	if (Expect(Token::Else, true))
	{
	    elseExpr = ParseStmtOrBlock();
	    if (!elseExpr)
	    {
		return 0;
	    }
	}
	else
	{
	    return 0;
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
    if (CurrentToken().GetType() != Token::Identifier)
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
    Token::TokenType tt = CurrentToken().GetType();
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
    ExprAST* bhead = 0;
    ExprAST* btail = 0;
    while(CurrentToken().GetType() != Token::Until)
    {
	ExprAST* stmt = ParseStatement();
	if (!stmt)
	{
	    return 0;
	}
	if (!bhead)
	{
	    bhead = btail = stmt;
	}
	else
	{
	    btail = btail->SetNext(stmt);
	}
	if(CurrentToken().GetType() == Token::Semicolon)
	{
	    NextToken();
	}
    }
    if (!Expect(Token::Until, true))
    {
	return 0;
    }
    ExprAST* cond = ParseExpression();
    BlockAST* body = new BlockAST(bhead);
    return new RepeatExprAST(cond, body);
}

ExprAST* Parser::ParseWrite()
{
    bool isWriteln = CurrentToken().GetType() == Token::Writeln;

    assert(CurrentToken().GetType() == Token::Write ||
	   CurrentToken().GetType() == Token::Writeln &&
	   "Expected write or writeln keyword here");
    NextToken();

    // TODO: Adde file support. 

    std::vector<WriteAST::WriteArg> args;
    if (CurrentToken().GetType() == Token::Semicolon)
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
	
	while(CurrentToken().GetType() != Token::RightParen)
	{
	    WriteAST::WriteArg wa;
	    wa.expr = ParseExpression();
	    if (!wa.expr)
	    {
		return 0;
	    }
	    if (CurrentToken().GetType() == Token::Colon)
	    {
		NextToken();
		wa.width = ParseExpression();
		if (!wa.width)
		{
		    return Error("Invalid width expression");
		}
	    }
	    if (CurrentToken().GetType() == Token::Colon)
	    {
		NextToken();
		wa.precision = ParseExpression();
		if (!wa.precision)
		{
		    return Error("Invalid precision expression");
		}
	    }
	    args.push_back(wa);
	    if (CurrentToken().GetType() != Token::RightParen)
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
	if (args.size() < 1)
	{
	    return Error("Expected expression in parenthesis of write statement");
	}
    }
    return new WriteAST(args, isWriteln);
}

ExprAST* Parser::ParseRead()
{
    bool isReadln = CurrentToken().GetType() == Token::Readln;

    assert(CurrentToken().GetType() == Token::Read ||
	   CurrentToken().GetType() == Token::Readln &&
	   "Expected read or readln keyword here");
    NextToken();

    std::vector<ExprAST*> args;
    if (CurrentToken().GetType() == Token::Semicolon)
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
	while(CurrentToken().GetType() != Token::RightParen)
	{
	    ExprAST* expr = ParseExpression();
	    if (!expr)
	    {
		return 0;
	    }
	    args.push_back(expr);
	    if (CurrentToken().GetType() != Token::RightParen)
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
	if (args.size() < 1)
	{
	    return Error("Expected expression in parenthesis of read statement");
	}
    }
    return new ReadAST(args, isReadln);
}

ExprAST* Parser::ParsePrimary()
{
    Token token = CurrentToken();
    if (token.GetType() == Token::Identifier)
    {
	Constants::ConstDecl* cd = GetConstDecl(token.GetIdentName());
	if (cd)
	{
	    token = cd->Translate();
	}
    }

    switch(token.GetType())
    {
    case Token::Real:
	return ParseRealExpr(token);

    case Token::Integer:
	return ParseIntegerExpr(token);

    case Token::Char:
	return ParseCharExpr(token);

    case Token::String:
	return ParseStringExpr(token);

    case Token::LeftParen:
	return ParseParenExpr();
	
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

    case Token::Write:
    case Token::Writeln:
	return ParseWrite();

    case Token::Read:
    case Token::Readln:
	return ParseRead();

    case Token::Minus:
	return ParseUnaryOp();

    default:
	CurrentToken().Dump(std::cerr);
	assert(0 && "Unexpected token");
	return 0;
    }
}

ExprAST* Parser::Parse()
{
    ExprAST* astHead = 0;
    ExprAST* astTail = 0;
    NextToken();
    if (!Expect(Token::Program, true))
    {
	return 0;
    }
    if (!Expect(Token::Identifier, false))
    {
	return 0;
    }
    moduleName = CurrentToken().GetIdentName();
    NextToken();
    for(;;)
    {
	ExprAST* curAst = 0;
	switch(CurrentToken().GetType())
	{
	case Token::EndOfFile:
	    return astHead;
	    
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
		return 0;
	    }
	    break;
	}
	default:
	    curAst = ParseExpression();
	    break;
	}
	if (curAst)
	{
	    // Append to the ast.
	    // First the empty list case.
	    if (!astTail)
	    {
		astHead = astTail = curAst;
	    }
	    else
	    {
		astTail = astTail->SetNext(curAst);
	    }
	}
    }
}
