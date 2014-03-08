#include "lexer.h"
#include "parser.h"
#include "variables.h"
#include "expr.h"
#include "namedobject.h"
#include "stack.h"
#include "builtin.h"
#include <iostream>
#include <cassert>

Parser::Parser(Lexer &l) 
    : 	lexer(l), nextTokenValid(false), errCnt(0)
{
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

Types::TypeDecl* Parser::ParseSimpleType()
{
    if (CurrentToken().GetType() != Token::TypeName)
    {
	return ErrorT("Expected simple type");
    }
    Types::TypeDecl* ty = types.GetTypeDecl(CurrentToken().GetIdentName());
    NextToken();
    return ty;
}


Types::Range* Parser::ParseRange()
{
    Token::TokenType tt = CurrentToken().GetType();
    
    if (tt == Token::Integer)
    {
	int start = CurrentToken().GetIntVal();
	NextToken();
        if (!Expect(Token::DotDot, true))
	{
	    return 0;
	}
	if (!Expect(Token::Integer, false))
	{
	    return 0;
	}
	int end = CurrentToken().GetIntVal();
	NextToken();

	return new Types::Range(start, end);
    }
#if 0
    else if (tt == Token::TypeName)
    {
	TypeDecl* ty = Types::GetTypeDecl(); 
	if (!ty->IsInteger())
	{
	    return ErrorR("Type used for array index must be of integer type");
	}
	return ty->GetRange();
    }
#endif
    else
    {
	return ErrorR("Invalid range specification");
    }
}

Types::TypeDecl* Parser::ParseType()
{
    Token::TokenType tt = CurrentToken().GetType();
    if (tt == Token::TypeName)
    {
	return ParseSimpleType();
    }
    if (tt == Token::Array)
    {
	NextToken();
	if (!Expect(Token::LeftSquare, true))
	{
	    return 0;
	}
	std::vector<Types::Range*> rv;
	while(CurrentToken().GetType() != Token::RightSquare)
	{
	    Types::Range* r = ParseRange();
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
    else 
    {
	return ErrorT("Can't understand type...");
    }
}

ExprAST* Parser::ParseIntegerExpr()
{
    ExprAST* result = new IntegerExprAST(CurrentToken().GetIntVal());
    NextToken();
    return result;
}

ExprAST* Parser::ParseCharExpr()
{
    ExprAST* result = new CharExprAST(CurrentToken().GetIntVal());
    NextToken();
    return result;
}

ExprAST* Parser::ParseRealExpr()
{
    ExprAST* result = new RealExprAST(CurrentToken().GetRealVal());
    NextToken();
    return result;
}

ExprAST* Parser::ParseStringExpr()
{
    ExprAST* result = new StringExprAST(CurrentToken().GetStrVal());
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

ExprAST* Parser::ParseIdentifierExpr()
{
    std::string idName = CurrentToken().GetIdentName();
    NextToken();
    /* TODO: Should we add builtin's to names at global level? */
    const NamedObject* def = nameStack.Find(idName);
    if (!Builtin::IsBuiltin(idName))
    {
	if (!def)
	{
	    return Error(std::string("Undefined name '") + idName + "'");
	}
	// Left Square = array access.
	if (CurrentToken().GetType() == Token::LeftSquare)
	{
	    assert(def->Type()->GetType() == Types::Array && "Expected array type here");
	    const Types::ArrayDecl* adecl = dynamic_cast<const Types::ArrayDecl*>(def->Type()); 
	    NextToken();
	    std::vector<ExprAST*> indices;
	    while(CurrentToken().GetType() != Token::RightSquare)
	    {
		ExprAST* index = ParseExpression();
		if (!index)
		{
		    return Error("Expected index expression");
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
	    return new ArrayExprAST(idName, indices, adecl->Ranges());
	}
	// If type is not function, not procedure, or the next thing is an assignment
	// then we want a "variable" with this name. 
	if ((def->Type()->GetType() != Types::Function && 
	     def->Type()->GetType() != Types::Procedure) || 
	    CurrentToken().GetType() == Token::Assign)
	{
	    return new VariableExprAST(idName);
	}
    }
    // Get past the '(' and fetch the next one. 
    std::vector<ExprAST* > args;
    if (CurrentToken().GetType() == Token::LeftParen)
    {
	if (!Expect(Token::LeftParen, true))
	{
	    return 0;
	}
	while (CurrentToken().GetType() != Token::RightParen)
	{
	    ExprAST* arg = ParseExpression();
	    if (!arg) return 0;
	    args.push_back(arg);
	    if (CurrentToken().GetType() == Token::Comma)
	    {
		NextToken();
	    }
	    else if (!Expect(Token::RightParen, false))
	    {
		return 0;
	    }
	}
	NextToken();
    }
    const PrototypeAST* proto = 0;
    if (def)
    {
	proto = def->Proto();
    }
    assert((!def || proto) && "Expected prototype...");
    return new CallExprAST(idName, args, proto);
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
		nameStack.Add(n, new NamedObject(n, type));
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
    if (!Expect(Token::Identifier, false))
    {
	return 0;
    }
    NextToken();
    std::vector<VarDef> args;
    if (CurrentToken().GetType() == Token::LeftParen)
    {
	std::vector<std::string> names;
	NextToken();
	while(CurrentToken().GetType() != Token::RightParen)
	{
	    bool isRef = false;
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
    return new PrototypeAST(funcName, args) ;
}

ExprAST* Parser::ParseStatement()
{
    ExprAST* expr = ParsePrimary();
    if(CurrentToken().GetType() == Token::Assign)
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
    NamedObject* nmObj = new NamedObject(name, ty, proto);
    if (!nameStack.Add(name, nmObj))
    {
	return ErrorF(std::string("Name '") + name + "' already exists...");
    }
    nameStack.Dump(std::cerr);
    
    NameWrapper wrapper(nameStack);
    for(auto v : proto->Args())
    {
	if (!nameStack.Add(v.Name(), new NamedObject(v.Name(), v.Type())))
	{
	    return ErrorF(std::string("Duplicate name ") + v.Name()); 
	}
    }

    VarDeclAST* varDecls = 0;
    ExprAST* body = 0;
    do
    {
	switch(CurrentToken().GetType())
	{
	case Token::Var:
	    if (varDecls)
	    {
		return ErrorF("Error: Can't declare variables multiple times");
	    }
	    varDecls = ParseVarDecls();
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
    switch(CurrentToken().GetType())
    {
    case Token::Real:
	return ParseRealExpr();

    case Token::Integer:
	return ParseIntegerExpr();

    case Token::Char:
	return ParseCharExpr();

    case Token::String:
	return ParseStringExpr();

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

	case Token::Unused:
	case Token::Unknown:
	    assert(0);
	    break;

	case Token::Function:
	case Token::Procedure:
	    curAst = ParseDefinition();
	    break;

	case Token::Var:
	    curAst = ParseVarDecls();
	    break;
	    
	case Token::Begin:
	{
	    curAst = ParseBlock();
	    /* Parse the "main" of the program - we call that
	     * "__PascalMain" so we can call it from C-code.*/
	    PrototypeAST* proto = new PrototypeAST("__PascalMain", std::vector<VarDef>());
	    FunctionAST* fun = new FunctionAST(proto, 0, curAst);
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
