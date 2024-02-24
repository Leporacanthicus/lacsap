#include "parser.h"
#include "builtin.h"
#include "callgraph.h"
#include "expr.h"
#include "lexer.h"
#include "namedobject.h"
#include "options.h"
#include "source.h"
#include "stack.h"
#include "trace.h"
#include "utils.h"

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/Support/MathExtras.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using TerminatorList = std::vector<Token::TokenType>;

class Parser : public ParserInterface
{
public:
    enum Forwarding
    {
	NoForwarding,
	AllowForwarding
    };
    enum ExpectConsuming
    {
	NoExpectConsume,
	ExpectConsume
    };

    Parser(Source& source);
    ExprAST* Parse(ParserType type) override;

    int GetErrors() override { return errCnt; }

    // Token handling functions
    const Token&     CurrentToken() const;
    void             NextToken(const char* file, int line);
    Token::TokenType PeekToken(const char* file, int line);

    // Simple expression parsing
    ExprAST* ParseExpression();
    ExprAST* ParseExprElement();
    ExprAST* ParseCallOrVariableExpr(const Token& token);
    ExprAST* ParseIdentifierExpr(const Token& token);
    ExprAST* ParseVariableExpr(const NamedObject* def);
    ExprAST* ParseIntegerExpr(const Token& token);
    ExprAST* ParseStringExpr(const Token& token);
    ExprAST* ParseParenExpr();
    ExprAST* ParsePrimary();
    ExprAST* ParseBinOpRHS(int exprPrec, ExprAST* lhs);
    ExprAST* ParseUnaryOp();
    ExprAST* ParseSetExpr(Types::TypeDecl* setType);
    ExprAST* ParseSizeOfExpr();
    ExprAST* ParseDefaultExpr();

    ExprAST*         ParseArrayExpr(ExprAST* expr, Types::TypeDecl*& type);
    ExprAST*         ParsePointerExpr(ExprAST* expr, Types::TypeDecl*& type);
    ExprAST*         FindVariant(ExprAST* expr, Types::TypeDecl*& type, int fc, Types::VariantDecl* v,
                                 const std::string& name);
    ExprAST*         ParseFieldExpr(ExprAST* expr, Types::TypeDecl*& type);
    VariableExprAST* ParseStaticMember(const TypeDef* def, Types::TypeDecl*& type);

    // Control flow functionality
    ExprAST* ParseRepeat();
    ExprAST* ParseIfExpr();
    ExprAST* ParseForExpr();
    ExprAST* ParseWhile();
    ExprAST* ParseCaseExpr();
    ExprAST* ParseWithBlock();
    ExprAST* ParseGoto();

    // I/O functions
    ExprAST* ParseWrite();
    ExprAST* ParseRead();

    // Statements, blocks and calls
    ExprAST*      ParseStatement();
    VarDeclAST*   ParseVarDecls();
    BlockAST*     ParseBlock(Location& endLoc);
    FunctionAST*  ParseDefinition(int level);
    PrototypeAST* ParsePrototype(bool unnamed);
    bool          ParseProgram(ParserType type);
    void          ParseLabels();
    ExprAST*      ParseUses();
    ExprAST*      ParseUnit(ParserType type);
    bool          ParseInterface(InterfaceList& iList);
    void          ParseImports();

    ExprAST* ConstDeclToExpr(const Location& loc, Types::TypeDecl* ty, const Constants::ConstDecl* c);
    ExprAST* ParseInitValue(Types::TypeDecl* ty);

    // Type declarations and defintitions
    void  ParseTypeDef();
    void  ParseConstDef();
    Token TranslateToken(const Token& token);

    const Constants::ConstDecl* ParseConstExpr(const TerminatorList& terminators);
    const Constants::ConstDecl* ParseConstRHS(int exprPrec, const Constants::ConstDecl* lhs);
    Constants::ConstDecl*       ParseConstEval(const Constants::ConstDecl* lhs, const Token& binOp,
                                               const Constants::ConstDecl* rhs);
    const Constants::ConstDecl* ParseConstTerm(const Location& loc);
    const Constants::ConstDecl* ParseConstFunction(const std::string& name);
    const Constants::ConstDecl* ParseConstSetExpr();

    Types::RangeBaseDecl* ParseRange(Types::TypeDecl*& type, Token::TokenType endToken,
                                     Token::TokenType altToken);
    Types::RangeBaseDecl* ParseRangeOrTypeRange(Types::TypeDecl*& type, Token::TokenType endToken,
                                                Token::TokenType altToken);

    Types::TypeDecl*    ParseSimpleType(bool errOnNoType);
    Types::ClassDecl*   ParseClassDecl(const std::string& name);
    Types::TypeDecl*    ParseType(const std::string& name, Forwarding maybeForwarded);
    Types::EnumDecl*    ParseEnumDef();
    Types::PointerDecl* ParsePointerType(Forwarding maybeForwarded);
    Types::TypeDecl*    ParseArrayDecl();
    bool                ParseFields(std::vector<Types::FieldDecl*>& fields, Types::VariantDecl*& variant,
                                    Token::TokenType type);
    Types::RecordDecl*  ParseRecordDecl();
    Types::FileDecl*    ParseFileDecl();
    Types::SetDecl*     ParseSetDecl();
    Types::StringDecl*  ParseStringDecl();
    Types::VariantDecl* ParseVariantDecl(Types::FieldDecl*& markerField);
    int64_t             ParseConstantValue(Token::TokenType& tt, Types::TypeDecl*& type);
    bool                ParseArgs(const NamedObject* def, std::vector<ExprAST*>& args);
    unsigned            ParseStringSize(Token::TokenType end);

    // Helper for syntax checking
    bool        Expect(Token::TokenType type, ExpectConsuming consumption, const char* file, int line);
    void        AssertToken(Token::TokenType type, const char* file, int line);
    bool        AcceptToken(Token::TokenType type, const char* file, int line);
    bool        IsSemicolonOrEnd();
    bool        ExpectSemicolonOrEnd(const char* file, int line);
    std::string GetIdentifier(const char* file, int line, ExpectConsuming consumption);

    // General helper functions
    void ExpandWithNames(const Types::FieldCollection* fields, ExprAST* v, int parentCount);

    // Helper functions for expression evaluation
    bool     IsCall(const NamedObject* def);
    ExprAST* MakeCallExpr(const NamedObject* def, const std::string& funcName, std::vector<ExprAST*>& args);
    ExprAST* MakeSimpleCall(ExprAST* expr, const PrototypeAST* proto, std::vector<ExprAST*>& args);
    ExprAST* MakeSelfCall(ExprAST* self, Types::MemberFuncDecl* mf, Types::ClassDecl* cd,
                          std::vector<ExprAST*>& args);

    // Helper functions for identifier access/checking
    const EnumDef*              GetEnumValue(const std::string& name);
    Types::TypeDecl*            GetTypeDecl(const std::string& name);
    const Constants::ConstDecl* GetConstDecl(const std::string& name);
    bool AddType(const std::string& name, Types::TypeDecl* type, bool restricted = false);
    bool AddConst(const std::string& name, const Constants::ConstDecl* cd);

    std::vector<VarDef> CalculateUsedVars(FunctionAST* fn, const std::vector<const NamedObject*>& varsUsed,
                                          const Stack<const NamedObject*>& nameStack);

    void PrintError(const Token& t, const std::string& msg);
    template<typename T = std::nullptr_t>
    T Error(const Token& t, const std::string& msg)
    {
	PrintError(t, msg);
	return T{};
    }
    template<typename T = std::nullptr_t>
    T Error(const std::string& msg)
    {
	PrintError(CurrentToken(), msg);
	return T{};
    }

private:
    Lexer                     lexer;
    Token                     curToken;
    Token                     nextToken;
    bool                      nextTokenValid;
    std::string               moduleName;
    int                       errCnt;
    Stack<const NamedObject*> nameStack;
    std::vector<ExprAST*>     ast;
};

using NameWrapper = StackWrapper<const NamedObject*>;

static bool IsOneOf(Token::TokenType t, const TerminatorList& list)
{
    for (auto i : list)
    {
	if (t == i)
	{
	    return true;
	}
    }
    return false;
}

class ListConsumer
{
public:
    enum class AllowEmpty
    {
	No,
	Yes
    };
    ListConsumer(Token::TokenType s, Token::TokenType t, AllowEmpty a)
        : separators{ s }, terminators{ t }, allowEmpty(a)
    {
    }
    ListConsumer(const TerminatorList& s, const TerminatorList& t, AllowEmpty a)
        : separators{ s }, terminators{ t }, allowEmpty(a)
    {
    }
    virtual bool Consume(Parser& parser) = 0;
    virtual ~ListConsumer() {}

    TerminatorList separators;
    TerminatorList terminators;
    AllowEmpty     allowEmpty;
};

void Parser::PrintError(const Token& t, const std::string& msg)
{
    const Location& loc = t.Loc();
    if (loc)
    {
	std::cerr << loc << " ";
    }
    std::cerr << "Error: " << msg << std::endl;
    errCnt++;
    if (loc)
    {
	Source& source = lexer.GetSource();
	source.PrintSource(loc.LineNumber());
	if (errCnt > 30)
	{
	    std::cerr << "Too many errors, quitting..." << std::endl;
	    exit(1);
	}
    }
}

const Token& Parser::CurrentToken() const
{
    return curToken;
}

void Parser::NextToken(const char* file, int line)
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
	std::cerr << file << ": " << line << ": ";
	curToken.dump();
    }
}

Token::TokenType Parser::PeekToken(const char* file, int line)
{
    if (nextTokenValid)
    {
	return nextToken.GetToken();
    }

    nextTokenValid = true;
    nextToken = lexer.GetToken();

    if (verbosity > 1)
    {
	std::cerr << "peeking: ";
	std::cerr << file << ": " << line << ": ";
	nextToken.dump(std::cerr);
    }
    return nextToken.GetToken();
}

std::string Parser::GetIdentifier(const char* file, int line, ExpectConsuming consumption)
{
    if (CurrentToken().GetToken() == Token::Identifier)
    {
	std::string name = CurrentToken().GetIdentName();
	if (consumption == ExpectConsume)
	{
	    NextToken(file, line);
	}
	return name;
    }
    return "";
}

bool Parser::Expect(Token::TokenType type, ExpectConsuming consumption, const char* file, int line)
{
    if (CurrentToken().GetToken() != type)
    {
	Token t(type);
	return Error<bool>("Expected '" + t.TypeStr() + "', got '" + CurrentToken().TypeStr() + "'.");
    }
    if (consumption == ExpectConsume)
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
    return !(CurrentToken().GetToken() != Token::End && !Expect(Token::Semicolon, ExpectConsume, file, line));
}

// Skip token, and check that it's matching what we expect.
// This is used when we (should) have already checked the token, so asserting is fine.
void Parser::AssertToken(Token::TokenType type, const char* file, int line)
{
    if (CurrentToken().GetToken() != type)
    {
	Token t(type);
	Error("Expected '" + t.TypeStr() + "', got '" + CurrentToken().ToString() + "'.");
	assert(0 && "Unexpected token");
    }
    NextToken(file, line);
}

// Check if the current token is a particular one.
// Return true and eat token if it is, return false if if it's not (and leave token in place)..
bool Parser::AcceptToken(Token::TokenType type, const char* file, int line)
{
    if (CurrentToken().GetToken() == type)
    {
	if (verbosity > 0)
	{
	    std::cerr << "accepting: ";
	    curToken.dump();
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
#define GetIdentifier(a) GetIdentifier(__FILE__, __LINE__, a)

Types::TypeDecl* Parser::GetTypeDecl(const std::string& name)
{
    if (const auto typeDef = llvm::dyn_cast_or_null<const TypeDef>(nameStack.Find(name)))
    {
	return typeDef->Type();
    }
    return 0;
}

ExprAST* Parser::ParseSizeOfExpr()
{
    AssertToken(Token::SizeOf);
    if (!Expect(Token::LeftParen, ExpectConsume))
    {
	return 0;
    }
    ExprAST*    expr = 0;
    std::string name = GetIdentifier(NoExpectConsume);
    if (!name.empty())
    {
	if (Types::TypeDecl* ty = GetTypeDecl(name))
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
    if (!Expect(Token::RightParen, ExpectConsume))
    {
	return 0;
    }
    return expr;
}

ExprAST* Parser::ParseDefaultExpr()
{
    AssertToken(Token::Default);
    if (!Expect(Token::LeftParen, ExpectConsume))
    {
	return 0;
    }
    ExprAST*         expr = 0;
    Types::TypeDecl* ty = GetTypeDecl(GetIdentifier(NoExpectConsume));
    if (!ty)
    {
	if (ExprAST* e = ParseExpression())
	{
	    ty = e->Type();
	}
    }
    else
    {
	AssertToken(Token::Identifier);
    }
    if (ty)
    {
	expr = ty->Init();
    }
    if (!Expect(Token::RightParen, ExpectConsume))
    {
	return 0;
    }
    return expr;
}

bool ParseSeparatedList(Parser& parser, ListConsumer& lc)
{
    bool done = lc.allowEmpty == ListConsumer::AllowEmpty::Yes &&
                IsOneOf(parser.CurrentToken().GetToken(), lc.terminators);

    while (!done)
    {
	if (!lc.Consume(parser))
	{
	    return false;
	}
	if (IsOneOf(parser.CurrentToken().GetToken(), lc.terminators))
	{
	    done = true;
	}
	else
	{
	    bool good = IsOneOf(parser.CurrentToken().GetToken(), lc.separators);
	    parser.NextToken();
	    if (!good)
	    {
		return false;
	    }
	}
    }
    parser.NextToken();
    return true;
}

ExprAST* Parser::ParseGoto()
{
    AssertToken(Token::Goto);
    Token t = CurrentToken();
    if (Expect(Token::Integer, ExpectConsume))
    {
	int n = t.GetIntVal();
	if (!nameStack.FindTopLevel(std::to_string(n)))
	{
	    if (!nameStack.Find(std::to_string(n)))
	    {
		return Error(t, "Label not defined");
	    }
	    return Error(t, "Label and GOTO need to be declared in the same scope");
	}
	return new GotoAST(t.Loc(), n);
    }
    return 0;
}

const Constants::ConstDecl* Parser::GetConstDecl(const std::string& name)
{
    if (const auto constDef = llvm::dyn_cast_or_null<const ConstDef>(nameStack.Find(name)))
    {
	return constDef->ConstValue();
    }
    return 0;
}

const EnumDef* Parser::GetEnumValue(const std::string& name)
{
    return llvm::dyn_cast_or_null<EnumDef>(nameStack.Find(name));
}

bool Parser::AddType(const std::string& name, Types::TypeDecl* ty, bool restricted)
{
    return nameStack.Add(new TypeDef(name, ty, restricted));
}

bool Parser::AddConst(const std::string& name, const Constants::ConstDecl* cd)
{
    if (!nameStack.Add(new ConstDef(name, cd)))
    {
	// TODO: Track location better.
	return Error<bool>("Name " + name + " is already declared as a constant");
    }
    return true;
}

Types::TypeDecl* Parser::ParseSimpleType(bool errOnNoType)
{
    std::string name = GetIdentifier(NoExpectConsume);
    if (!name.empty())
    {
	if (Types::TypeDecl* ty = GetTypeDecl(name))
	{
	    AssertToken(Token::Identifier);
	    return ty;
	}
	if (errOnNoType)
	{
	    return Error("Identifier '" + name + "' does not name a type");
	}
    }
    return 0;
}

Token Parser::TranslateToken(const Token& token)
{
    if (token.GetToken() == Token::Identifier)
    {
	if (const Constants::ConstDecl* cd = GetConstDecl(token.GetIdentName()))
	{
	    if (!llvm::isa<Constants::CompoundConstDecl>(cd) && !llvm::isa<Constants::SetConstDecl>(cd) &&
	        !llvm::isa<Constants::EnumConstDecl>(cd))
	    {
		return cd->Translate();
	    }
	}
    }
    return token;
}

int64_t Parser::ParseConstantValue(Token::TokenType& tt, Types::TypeDecl*& type)
{
    bool negative = false;
    if (CurrentToken().GetToken() == Token::Minus)
    {
	negative = true;
	NextToken();
    }
    Token::TokenType oldtt = tt;

    Token token = TranslateToken(CurrentToken());

    tt = token.GetToken();
    int64_t result = 0;

    switch (tt)
    {
    case Token::Integer:
	type = Types::Get<Types::IntegerDecl>();
	result = token.GetIntVal();
	break;

    case Token::Char:
	type = Types::Get<Types::CharDecl>();
	result = token.GetIntVal();
	break;

    case Token::Identifier:
    {
	tt = CurrentToken().GetToken();

	std::string name = CurrentToken().GetIdentName();

	if (const Constants::ConstDecl* cd = GetConstDecl(name))
	{
	    type = cd->Type();
	    result = Constants::ToInt(cd);
	}
	else if (const EnumDef* ed = GetEnumValue(name))
	{
	    type = ed->Type();
	    result = ed->Value();
	}
	else
	{
	    tt = Token::Unknown;
	    return Error<int>("Invalid constant, expected identifier for enumerated type");
	}
	break;
    }
    default:
	tt = Token::Unknown;
	return Error<int>("Invalid constant value, expected char, integer or enum value");
    }

    if (oldtt != Token::Unknown && oldtt != tt)
    {
	tt = Token::Unknown;
	return Error<int>("Expected token to match type");
    }

    NextToken();
    if (negative)
    {
	if (tt != Token::Integer)
	{
	    tt = Token::Unknown;
	    return Error<int>("Invalid negative constant");
	}
	result = -result;
    }
    return result;
}

Types::RangeBaseDecl* Parser::ParseRange(Types::TypeDecl*& type, Token::TokenType endToken,
                                         Token::TokenType altToken)
{
    TRACE();
    const Constants::ConstDecl* startC = ParseConstExpr({ Token::DotDot });
    if (!startC && Expect(Token::Identifier, NoExpectConsume) && PeekToken() == Token::DotDot)
    {
	std::string lowName = GetIdentifier(ExpectConsume);
	if (!Expect(Token::DotDot, ExpectConsume) || !Expect(Token::Identifier, NoExpectConsume))
	{
	    return 0;
	}
	std::string highName = GetIdentifier(ExpectConsume);
	if (endToken != Token::Semicolon)
	{
	    AcceptToken(Token::Colon);
	    if (!Expect(Token::Identifier, NoExpectConsume))
	    {
		return 0;
	    }
	    if ((type = GetTypeDecl(GetIdentifier(NoExpectConsume))))
	    {
		NextToken();
	    }
	}
	else
	{
	    const VarDef* vardef = nullptr;
	    if (const NamedObject* def = nameStack.Find(lowName))
	    {
		if (!(vardef = llvm::dyn_cast<VarDef>(def)))
		{
		    return Error("Expected variable name");
		}
	    }
	    if (vardef)
	    {
		type = vardef->Type();
	    }
	    if (const NamedObject* def = nameStack.Find(highName))
	    {
		if (!(vardef = llvm::dyn_cast<VarDef>(def)))
		{
		    return Error("Expected variable name");
		}
	    }
	    if (type != vardef->Type())
	    {
		return Error("Expected same type for both high and low variable");
	    }
	}
	if (type)
	{
	    return new Types::DynRangeDecl(lowName, highName, type);
	}
    }
    if (!(startC && Expect(Token::DotDot, ExpectConsume)))
    {
	return Error("Expected constant name");
    }

    const Constants::ConstDecl* endC = ParseConstExpr({ endToken, altToken });
    if (!endC)
    {
	return 0;
    }

    int64_t start = Constants::ToInt(startC);
    int64_t end = Constants::ToInt(endC);

    type = startC->Type();
    assert(type == endC->Type() && "Expect same type on both sides");
    if (end <= start)
    {
	return Error("Invalid range specification");
    }
    return new Types::RangeDecl(new Types::Range(start, end), type);
}

Types::RangeBaseDecl* Parser::ParseRangeOrTypeRange(Types::TypeDecl*& type, Token::TokenType endToken,
                                                    Token::TokenType altToken)
{
    std::string name = GetIdentifier(NoExpectConsume);
    if ((type = GetTypeDecl(name)))
    {
	if (!type->IsIntegral())
	{
	    return Error("Type used as index specification should be integral type");
	}
	AssertToken(Token::Identifier);
	return new Types::RangeDecl(type->GetRange(), type);
    }

    return ParseRange(type, endToken, altToken);
}

Constants::ConstDecl* Parser::ParseConstEval(const Constants::ConstDecl* lhs, const Token& binOp,
                                             const Constants::ConstDecl* rhs)
{
    switch (binOp.GetToken())
    {
    case Token::Plus:
	return *lhs + *rhs;
	break;

    case Token::Minus:
	return *lhs - *rhs;
	break;

    case Token::Multiply:
	return *lhs * *rhs;

    case Token::Divide:
    {
	const Constants::ConstDecl* tmpL = Constants::ToRealConstDecl(lhs);
	const Constants::ConstDecl* tmpR = Constants::ToRealConstDecl(rhs);
	if (tmpL && tmpR)
	{
	    return *tmpL / *tmpR;
	}
    }

    case Token::Div:
    {
	if (llvm::isa<Constants::IntConstDecl>(lhs) && llvm::isa<Constants::IntConstDecl>(rhs))
	{
	    return *lhs / *rhs;
	}
	return Error("Expected integer constants for 'div' operation");
    }

    case Token::Mod:
    {
	if (llvm::isa<Constants::IntConstDecl>(lhs) && llvm::isa<Constants::IntConstDecl>(rhs))
	{
	    return *lhs % *rhs;
	}
    }

    default:
	break;
    }
    return 0;
}

// Note: name is not const ref, because we call strlower on it.
const Constants::ConstDecl* Parser::ParseConstFunction(const std::string& name)
{
    // Don't even try if the next token to come isn't a left paren.
    if (PeekToken() != Token::LeftParen)
    {
	return 0;
    }
    if (Constants::IsEvaluableFunc(name))
    {
	NextToken();
	AssertToken(Token::LeftParen);
	std::vector<const Constants::ConstDecl*> args;
	for (;;)
	{
	    const Constants::ConstDecl* cd = ParseConstExpr({ Token::Comma, Token::RightParen });
	    args.push_back(cd);
	    if (AcceptToken(Token::Comma))
	    {
		continue;
	    }
	    else if (Expect(Token::RightParen, ExpectConsume))
	    {
		return Constants::EvalFunction(name, args);
	    }
	}
    }
    return 0;
}

class CCConstSetList : public ListConsumer
{
public:
    CCConstSetList() : ListConsumer{ Token::Comma, Token::RightSquare, ListConsumer::AllowEmpty::Yes } {}
    bool Consume(Parser& parser) override
    {
	if (const Constants::ConstDecl* cd = parser.ParseConstExpr(
	        { Token::DotDot, Token::Comma, Token::RightSquare }))
	{
	    Location loc = cd->Loc();
	    if (parser.AcceptToken(Token::DotDot))
	    {
		if (const Constants::ConstDecl* cde = parser.ParseConstExpr(
		        { Token::Comma, Token::RightSquare }))
		{
		    Types::TypeDecl* type = cd->Type();
		    if (type != cde->Type())
		    {
			return false;
		    }
		    int64_t start = Constants::ToInt(cd);
		    int64_t end = Constants::ToInt(cde);
		    cd = new Constants::RangeConstDecl(loc, type, Types::Range(start, end));
		}
		else
		{
		    return false;
		}
	    }
	    values.push_back(cd);
	    return true;
	}
	return false;
    }
    std::vector<const Constants::ConstDecl*>& Values() { return values; }

private:
    std::vector<const Constants::ConstDecl*> values;
};

const Constants::ConstDecl* Parser::ParseConstSetExpr()
{
    AssertToken(Token::LeftSquare);
    CCConstSetList ccs;
    Location       loc = CurrentToken().Loc();
    if (ParseSeparatedList(*this, ccs))
    {
	Types::TypeDecl* type = 0;
	if (!ccs.Values().empty())
	{
	    type = ccs.Values()[0]->Type();
	    for (auto i = ccs.Values().begin() + 1, e = ccs.Values().end(); i != e; i++)
	    {
		if ((*i)->Type() != type)
		{
		    return Error("Not all elements of set are same type");
		}
	    }
	}
	return new Constants::SetConstDecl(loc, new Types::SetDecl(0, type), ccs.Values());
    }
    return 0;
}

const Constants::ConstDecl* Parser::ParseConstTerm(const Location& loc)
{
    TRACE();
    Token::TokenType            unaryToken = Token::Unknown;
    const Constants::ConstDecl* cd = 0;
    int                         mul = 1;

    switch (CurrentToken().GetToken())
    {
    case Token::Minus:
	mul = -1;
    case Token::Plus:
    case Token::Not:
	unaryToken = CurrentToken().GetToken();
	NextToken();
	break;
    default:
	break;
    }

    switch (CurrentToken().GetToken())
    {
    case Token::LeftParen:
	AssertToken(Token::LeftParen);
	cd = ParseConstExpr({ Token::RightParen });
	// We don't eat the right paren here, it gets eaten later.
	if (!Expect(Token::RightParen, ExpectConsume))
	{
	    return 0;
	}
	break;

    case Token::LeftSquare:
	cd = ParseConstSetExpr();
	break;

    case Token::StringLiteral:
	if (unaryToken != Token::Unknown)
	{
	    return Error("Unary + or - not allowed for string constants");
	}
	cd = new Constants::StringConstDecl(loc, CurrentToken().GetStrVal());
	NextToken();
	break;

    case Token::Integer:
    {
	uint64_t v = CurrentToken().GetIntVal();
	if (unaryToken == Token::Not)
	{
	    v = ~v;
	}
	cd = new Constants::IntConstDecl(loc, v * mul);
	NextToken();
	break;
    }

    case Token::Real:
	if (unaryToken == Token::Not)
	{
	    return Error("Unary 'not' is not allowed for real constants");
	}
	cd = new Constants::RealConstDecl(loc, CurrentToken().GetRealVal() * mul);
	NextToken();
	break;

    case Token::Char:
	if (unaryToken != Token::Unknown)
	{
	    return Error("Unary + or - not allowed for char constants");
	}
	cd = new Constants::CharConstDecl(loc, (char)CurrentToken().GetIntVal());
	NextToken();
	break;

    case Token::Identifier:
    {
	std::string name = GetIdentifier(NoExpectConsume);
	if ((cd = ParseConstFunction(name)))
	{
	}
	else if (const EnumDef* ed = GetEnumValue(name))
	{
	    if (llvm::isa<Types::BoolDecl>(ed->Type()))
	    {
		uint64_t v = ed->Value();
		if (unaryToken == Token::Not)
		{
		    v = !v;
		}
		else if (unaryToken != Token::Unknown)
		{
		    return Error("Unary + or - not allowed for bool constants");
		}
		cd = new Constants::BoolConstDecl(loc, v);
	    }
	    else
	    {
		if (unaryToken != Token::Unknown)
		{
		    return Error("Unary + or - not allowed for enum constants");
		}
		cd = new Constants::EnumConstDecl(ed->Type(), loc, ed->Value());
	    }
	    NextToken();
	}
	else
	{
	    if (!(cd = GetConstDecl(name)))
	    {
		return 0;
	    }
	    if (llvm::isa<Constants::BoolConstDecl>(cd) && unaryToken == Token::Not)
	    {
		const auto bd = llvm::dyn_cast<Constants::BoolConstDecl>(cd);
		cd = new Constants::BoolConstDecl(loc, !bd->Value());
	    }

	    if (mul == -1)
	    {
		if (const auto rd = llvm::dyn_cast<Constants::RealConstDecl>(cd))
		{
		    ;
		    cd = new Constants::RealConstDecl(loc, -rd->Value());
		}
		else if (const auto id = llvm::dyn_cast<Constants::IntConstDecl>(cd))
		{
		    cd = new Constants::IntConstDecl(loc, -id->Value());
		}
		else
		{
		    return Error("Can't negate the type of " + CurrentToken().GetIdentName() +
		                 " only integer and real types can be negated");
		}
	    }
	    NextToken();
	}
	break;
    }

    default:
	return 0;
    }
    return cd;
}

const Constants::ConstDecl* Parser::ParseConstRHS(int exprPrec, const Constants::ConstDecl* lhs)
{
    for (;;)
    {
	Token binOp = CurrentToken();
	int   tokPrec = binOp.Precedence();
	// Lower precedence, so don't use up the binOp.
	if (tokPrec < exprPrec)
	{
	    return lhs;
	}

	NextToken();

	const Constants::ConstDecl* rhs = ParseConstTerm(lhs->Loc());
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

const Constants::ConstDecl* Parser::ParseConstExpr(const TerminatorList& terminators)
{
    const Location              loc = CurrentToken().Loc();
    const Constants::ConstDecl* cd;

    do
    {
	if (!(cd = ParseConstTerm(loc)))
	{
	    return 0;
	}

	if (!IsOneOf(CurrentToken().GetToken(), terminators))
	{
	    if (!(cd = ParseConstRHS(0, cd)))
	    {
		return 0;
	    }
	}
    } while (!IsOneOf(CurrentToken().GetToken(), terminators));
    return cd;
}

void Parser::ParseConstDef()
{
    AssertToken(Token::Const);
    do
    {
	std::string nm = GetIdentifier(ExpectConsume);
	if (nm.empty())
	{
	    return;
	}
	if (!Expect(Token::Equal, ExpectConsume))
	{
	    return;
	}
	std::string val = GetIdentifier(NoExpectConsume);
	if (Types::TypeDecl* ty = GetTypeDecl(val))
	{
	    AssertToken(Token::Identifier);
	    if (auto init = ParseInitValue(ty))
	    {
		if (!AddConst(nm, new Constants::CompoundConstDecl(CurrentToken().Loc(), ty, init)))
		{
		    return;
		}
		if (!Expect(Token::Semicolon, ExpectConsume))
		{
		    return;
		}
		continue;
	    }
	    else
	    {
		Error("Unexpected constant content?");
		return;
	    }
	}
	const Constants::ConstDecl* cd = ParseConstExpr({ Token::Semicolon });
	if (!cd)
	{
	    Error("Invalid constant value");
	    return;
	}
	if (!AddConst(nm, cd))
	{
	    Error("Duplicate constant declaration");
	    return;
	}
	if (!Expect(Token::Semicolon, ExpectConsume))
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
	std::string name = GetIdentifier(ExpectConsume);
	if (name.empty())
	{
	    return;
	}
	if (!Expect(Token::Equal, ExpectConsume))
	{
	    return;
	}
	bool restricted = AcceptToken(Token::Restricted);
	if (Types::TypeDecl* ty = ParseType(name, AllowForwarding))
	{
	    ExprAST* init = 0;
	    if (AcceptToken(Token::Value))
	    {
		init = ParseInitValue(ty);
		if (!init)
		{
		    return;
		}
		ty = Types::CloneWithInit(ty, init);
	    }
	    if (!AddType(name, ty, restricted))
	    {
		Error("Name " + name + " is already in use.");
		return;
	    }
	    auto pty = llvm::dyn_cast<Types::PointerDecl>(ty);
	    if (pty && pty->IsIncomplete())
	    {
		incomplete.push_back(pty);
	    }
	    if (!Expect(Token::Semicolon, ExpectConsume))
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
    for (auto p : incomplete)
    {
	std::string name = p->SubType()->Name();
	if (Types::TypeDecl* ty = GetTypeDecl(name))
	{
	    if (ty->IsIncomplete())
	    {
		Error("Forward declared type '" + name + "' is incomplete.");
		return;
	    }
	    p->SetSubType(ty);
	}
	else
	{
	    // TODO: Store token?
	    Error("Forward declared pointer type not declared: " + name);
	    return;
	}
    }
}

class CCNames : public ListConsumer
{
public:
    CCNames(Token::TokenType e) : ListConsumer{ Token::Comma, e, ListConsumer::AllowEmpty::No } {}
    bool Consume(Parser& parser)
    {
	std::string name = parser.GetIdentifier(Parser::ExpectConsume);
	if (!name.empty())
	{
	    names.push_back(name);
	    return true;
	}
	return false;
    }
    std::vector<std::string>& Names() { return names; }

private:
    std::vector<std::string> names;
};

class CCIntegers : public ListConsumer
{
public:
    CCIntegers(Token::TokenType e) : ListConsumer(Token::Comma, e, ListConsumer::AllowEmpty::No) {}
    virtual bool GetValue(Parser& parser, int& val)
    {
	if (parser.CurrentToken().GetToken() == Token::Integer)
	{
	    val = parser.CurrentToken().GetIntVal();
	}
	return parser.Expect(Token::Integer, Parser::ExpectConsume);
    }
    bool Consume(Parser& parser) override
    {
	// Don't move forward here.
	int val;
	if (GetValue(parser, val))
	{
	    values.push_back(val);
	    return true;
	}
	return false;
    }
    std::vector<int>& Values() { return values; }

private:
    std::vector<int> values;
};

class CCConstants : public CCIntegers
{
public:
    CCConstants(Token::TokenType e) : CCIntegers{ e }, tt(Token::Unknown), type(0) {}
    bool GetValue(Parser& parser, int& val) override
    {
	val = parser.ParseConstantValue(tt, type);
	return tt != Token::Unknown;
    }
    Types::TypeDecl* Type() const { return type; }

private:
    Token::TokenType tt;
    Types::TypeDecl* type;
};

Types::EnumDecl* Parser::ParseEnumDef()
{
    AssertToken(Token::LeftParen);
    CCNames ccv(Token::RightParen);

    if (ParseSeparatedList(*this, ccv))
    {
	Types::EnumDecl* ty = new Types::EnumDecl(ccv.Names(), Types::Get<Types::IntegerDecl>());
	for (auto v : ty->Values())
	{
	    if (!nameStack.Add(new EnumDef(v.name, v.value, ty)))
	    {
		return Error("Enumerated value by name " + v.name + " already exists...");
	    }
	}
	return ty;
    }
    return 0;
}

Types::PointerDecl* Parser::ParsePointerType(Forwarding maybeForwarded)
{
    assert((CurrentToken().GetToken() == Token::Uparrow || CurrentToken().GetToken() == Token::At) &&
           "Expected @ or ^ token...");
    NextToken();
    // If the name is an identifier then it may be name of a not yet declared type.
    // We need to forward declare it, and backpatch later.
    std::string name = GetIdentifier(ExpectConsume);
    if (!name.empty())
    {
	if (maybeForwarded == NoForwarding)
	{
	    // Is it a known type?
	    if (Types::TypeDecl* ty = GetTypeDecl(name))
	    {
		return new Types::PointerDecl(ty);
	    }
	    else
	    {
		return Error("Unknown type '" + name + "' in pointer declaration");
	    }
	}
	// Otherwise, forward declare...
	Types::ForwardDecl* fwd = new Types::ForwardDecl(name);
	return new Types::PointerDecl(fwd);
    }

    if (Types::TypeDecl* ty = ParseType("", NoForwarding))
    {
	return new Types::PointerDecl(ty);
    }
    return 0;
}

Types::TypeDecl* Parser::ParseArrayDecl()
{
    AssertToken(Token::Array);
    if (Expect(Token::LeftSquare, ExpectConsume))
    {
	std::vector<Types::RangeDecl*> rv;
	Types::DynRangeDecl*           dr = nullptr;
	Types::TypeDecl*               type = 0;
	while (!AcceptToken(Token::RightSquare))
	{
	    if (Types::RangeBaseDecl* r = ParseRangeOrTypeRange(type, Token::RightSquare, Token::Comma))
	    {
		if (auto rr = llvm::dyn_cast<Types::RangeDecl>(r))
		{
		    assert(type && "Uh? Type is supposed to be set now");
		    rv.push_back(rr);
		}
		else
		{
		    assert(!dr && "Expect only one dynamic range at this point");
		    dr = llvm::dyn_cast<Types::DynRangeDecl>(r);
		    assert(dr && "Expect to have a dynrange here");
		}
	    }
	    else
	    {
		return 0;
	    }
	    AcceptToken(Token::Comma);
	}
	if (rv.empty() && !dr)
	{
	    return Error("Expected array size to be declared");
	}
	if (Expect(Token::Of, ExpectConsume))
	{
	    if (Types::TypeDecl* ty = ParseType("", NoForwarding))
	    {
		if (dr)
		{
		    return new Types::DynArrayDecl(ty, dr);
		}
		return new Types::ArrayDecl(ty, rv);
	    }
	}
    }
    return 0;
}

// Parse Variant declaration:
// CASE [name:] typename OF
//   constant: ({identifier {, identifier}: typename;});
// | constant: (case [name:] typename OF ...);

Types::VariantDecl* Parser::ParseVariantDecl(Types::FieldDecl*& markerField)
{
    std::vector<Types::FieldDecl*> variants;
    std::string                    marker = GetIdentifier(NoExpectConsume);
    if (!marker.empty() && PeekToken() == Token::Colon)
    {
	AssertToken(Token::Identifier);
	AssertToken(Token::Colon);
    }
    Types::TypeDecl* markerTy = ParseType("", NoForwarding);
    if (!markerTy)
    {
	return 0;
    }
    if (!markerTy->IsIntegral())
    {
	return Error("Expect variant selector to be integral type");
    }
    if (!marker.empty())
    {
	markerField = new Types::FieldDecl(marker, markerTy, false);
    }
    if (!Expect(Token::Of, ExpectConsume))
    {
	return 0;
    }
    do
    {
	CCConstants labels{ Token::Colon };
	if (!ParseSeparatedList(*this, labels))
	{
	    return 0;
	}
	if (*markerTy != *labels.Type())
	{
	    // TODO: This needs a better location.
	    return Error("Marker type does not match member variant type");
	}
	std::vector<int>& vals = labels.Values();
	auto              e = vals.end();
	auto              b = vals.begin();
	for (auto i = b; i != e; i++)
	{
	    auto f = std::find(b, e, *i);
	    if (f != i && f != e)
	    {
		// TODO: Track location.
		return Error("Value already used: " + std::to_string(*i) + " in variant declaration");
	    }
	}
	if (!Expect(Token::LeftParen, ExpectConsume))
	{
	    return 0;
	}
	std::vector<Types::FieldDecl*> fields;
	do
	{
	    std::vector<std::string> names;
	    if (CurrentToken().GetToken() != Token::RightParen)
	    {
		if (AcceptToken(Token::Case))
		{
		    Types::FieldDecl*   innerMarker = 0;
		    Types::VariantDecl* v = ParseVariantDecl(innerMarker);
		    if (!v)
		    {
			return 0;
		    }
		    std::vector<Types::FieldDecl*> innerFields;
		    if (innerMarker)
		    {
			innerFields.push_back(innerMarker);
		    }
		    Types::RecordDecl* rec = new Types::RecordDecl(innerFields, v);
		    fields.push_back(new Types::FieldDecl("", rec, false));
		}
		else
		{
		    do
		    {
			std::string name = GetIdentifier(ExpectConsume);
			if (name.empty())
			{
			    return 0;
			}
			names.push_back(name);
			if (CurrentToken().GetToken() != Token::Colon && !Expect(Token::Comma, ExpectConsume))
			{
			    return 0;
			}
		    } while (!AcceptToken(Token::Colon));

		    if (Types::TypeDecl* ty = ParseType("", NoForwarding))
		    {
			for (auto n : names)
			{
			    for (auto f : fields)
			    {
				if (n == f->Name())
				{
				    // TODO: Track location.
				    return Error("Duplicate field name '" + n + "' in record");
				}
			    }
			    // Variants can't be static, can they?
			    fields.push_back(new Types::FieldDecl(n, ty, false));
			}
		    }
		}
		if (CurrentToken().GetToken() != Token::RightParen &&
		    !Expect(Token::Semicolon, ExpectConsume))
		{
		    TRACE();
		    return 0;
		}
	    }
	} while (!AcceptToken(Token::RightParen));
	if (CurrentToken().GetToken() != Token::RightParen && !ExpectSemicolonOrEnd())
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
    } while (CurrentToken().GetToken() != Token::End && CurrentToken().GetToken() != Token::RightParen);
    return new Types::VariantDecl(variants);
}

bool Parser::ParseFields(std::vector<Types::FieldDecl*>& fields, Types::VariantDecl*& variant,
                         Token::TokenType type)
{
    TRACE();

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
	    Types::FieldDecl* markerField = 0;
	    if (!(variant = ParseVariantDecl(markerField)))
	    {
		return false;
	    }
	    if (markerField)
	    {
		fields.push_back(markerField);
	    }
	}
	else if (isClass && (CurrentToken().GetToken() == Token::Function ||
	                     CurrentToken().GetToken() == Token::Procedure))
	{
	    PrototypeAST* p = ParsePrototype(false);
	    if (!Expect(Token::Semicolon, ExpectConsume))
	    {
		return false;
	    }
	    int f = 0;
	    if (AcceptToken(Token::Static))
	    {
		f |= Types::MemberFuncDecl::Static;
		if (!Expect(Token::Semicolon, ExpectConsume))
		{
		    return false;
		}
	    }
	    if (AcceptToken(Token::Virtual))
	    { //
		f |= Types::MemberFuncDecl::Virtual;
		if (!Expect(Token::Semicolon, ExpectConsume))
		{
		    return false;
		}
	    }
	    if (AcceptToken(Token::Override))
	    {
		f |= Types::MemberFuncDecl::Override;
		if (!Expect(Token::Semicolon, ExpectConsume))
		{
		    return false;
		}
	    }
	    // Ignore "inline" token
	    if (AcceptToken(Token::Inline))
	    {
		if (!Expect(Token::Semicolon, ExpectConsume))
		{
		    return false;
		}
	    }
	    Types::MemberFuncDecl* m = new Types::MemberFuncDecl(p, f);
	    fields.push_back(new Types::FieldDecl(p->Name(), m, false));
	}
	else
	{
	    // Cope with empty classes - but not empty Record!
	    if (!isClass || CurrentToken().GetToken() != Token::End)
	    {
		CCNames ccv(Token::Colon);
		if (!ParseSeparatedList(*this, ccv))
		{
		    return false;
		}
		assert(!ccv.Names().empty() && "Should have some names here...");
		if (Types::TypeDecl* ty = ParseType("", NoForwarding))
		{
		    ExprAST* init = 0;
		    if (AcceptToken(Token::Value))
		    {
			init = ParseInitValue(ty);
			if (!init)
			{
			    return 0;
			}
			ty = Types::CloneWithInit(ty, init);
		    }
		    for (auto n : ccv.Names())
		    {
			for (auto f : fields)
			{
			    if (n == f->Name())
			    {
				// TODO: Needs better location.
				return Error<bool>("Duplicate field name '" + n + "' in record");
			    }
			}
			bool isStatic = false;
			if (isClass && CurrentToken().GetToken() == Token::Semicolon &&
			    PeekToken() == Token::Static)
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
    } while (!AcceptToken(Token::End));
    return true;
}

Types::RecordDecl* Parser::ParseRecordDecl()
{
    AssertToken(Token::Record);
    std::vector<Types::FieldDecl*> fields;
    Types::VariantDecl*            variant;
    if (ParseFields(fields, variant, Token::Record))
    {
	if (fields.size() == 0 && !variant)
	{
	    return Error("No elements in record declaration");
	}

	std::vector<RecordInit> init;

	int index = 0;
	for (auto field : fields)
	{
	    if (field->SubType()->Init())
	    {
		init.push_back({ { index }, field->SubType()->Init() });
	    }
	    index++;
	}
	auto rd = new Types::RecordDecl(fields, variant);
	if (init.size())
	{
	    auto ir = new InitRecordAST(Location(), rd, init);
	    rd = llvm::dyn_cast<Types::RecordDecl>(Types::CloneWithInit(rd, ir));
	}
	return rd;
    }
    return 0;
}

Types::FileDecl* Parser::ParseFileDecl()
{
    AssertToken(Token::File);
    if (Expect(Token::Of, ExpectConsume))
    {
	if (Types::TypeDecl* type = ParseType("", NoForwarding))
	{
	    return new Types::FileDecl(type);
	}
    }
    return 0;
}

Types::SetDecl* Parser::ParseSetDecl()
{
    TRACE();
    AssertToken(Token::Set);
    if (Expect(Token::Of, ExpectConsume))
    {
	Types::TypeDecl* type = 0;
	if (Types::RangeBaseDecl* r = ParseRangeOrTypeRange(type, Token::Semicolon, Token::Unknown))
	{
	    auto rd = llvm::dyn_cast<Types::RangeDecl>(r);
	    if (rd && rd->GetRange()->Size() > Types::SetDecl::MaxSetSize)
	    {
		return Error("Set too large");
	    }
	    assert(type && "Uh? Type is supposed to be set");
	    return new Types::SetDecl(rd, type);
	}
    }
    return 0;
}

unsigned Parser::ParseStringSize(Token::TokenType end)
{
    if (const Constants::ConstDecl* size = ParseConstExpr({ end }))
    {
	if (auto is = llvm::dyn_cast<Constants::IntConstDecl>(size))
	{
	    return is->Value();
	}
    }
    return Error<unsigned>("Invalid string size");
}

Types::StringDecl* Parser::ParseStringDecl()
{
    TRACE();
    AssertToken(Token::String);
    unsigned size = 255;

    if (AcceptToken(Token::LeftSquare))
    {
	size = ParseStringSize(Token::RightSquare);
	if (!Expect(Token::RightSquare, ExpectConsume))
	{
	    return 0;
	}
    }
    else if (AcceptToken(Token::LeftParen))
    {
	size = ParseStringSize(Token::RightParen);
	if (!Expect(Token::RightParen, ExpectConsume))
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

Types::ClassDecl* Parser::ParseClassDecl(const std::string& name)
{
    TRACE();
    const Location loc = CurrentToken().Loc();
    AssertToken(Token::Class);
    Types::ClassDecl* base = 0;
    // Find derived class, if available.
    if (AcceptToken(Token::LeftParen))
    {
	std::string baseName = GetIdentifier(ExpectConsume);
	if (baseName.empty())
	{
	    return 0;
	}
	if (!(base = llvm::dyn_cast_or_null<Types::ClassDecl>(GetTypeDecl(baseName))))
	{
	    return Error("Expected class as base");
	}
	if (!Expect(Token::RightParen, ExpectConsume))
	{
	    return 0;
	}
    }

    std::vector<Types::FieldDecl*> fields;
    Types::VariantDecl*            variant;
    if (!ParseFields(fields, variant, Token::Class))
    {
	return 0;
    }

    std::vector<Types::MemberFuncDecl*> mf;
    std::vector<VarDef>                 vars;
    bool                                needVtable = false;
    for (auto f = fields.begin(); f != fields.end();)
    {
	if (auto m = llvm::dyn_cast<Types::MemberFuncDecl>((*f)->SubType()))
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
		vars.push_back(VarDef(vname, (*f)->SubType()));
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

Types::TypeDecl* Parser::ParseType(const std::string& name, Forwarding maybeForwarded)
{
    TRACE();
    Token::TokenType tt = CurrentToken().GetToken();
    if (AcceptToken(Token::Packed))
    {
	tt = CurrentToken().GetToken();
	if (tt != Token::Array && tt != Token::Record && tt != Token::Set && tt != Token::File)
	{
	    return Error("Expected 'array', 'record', 'file' or 'set' after 'packed'");
	}
    }

    AcceptToken(Token::Bindable);

    tt = TranslateToken(CurrentToken()).GetToken();

    switch (tt)
    {
    case Token::Type:
    {
	// Accept "type of x", where x is a variable-expression. ISO10206 feature.
	NextToken();
	if (!Expect(Token::Of, ExpectConsume))
	{
	    return 0;
	}
	std::string varName = GetIdentifier(ExpectConsume);
	if (const NamedObject* def = nameStack.Find(varName))
	{
	    if (!llvm::isa<VarDef>(def))
	    {
		return Error("Expected variable name");
	    }
	    return def->Type();
	}
	return Error("Expected an identifier for 'type of'");
    }
    break;

    case Token::Identifier:
    {
	if (!GetEnumValue(GetIdentifier(NoExpectConsume)))
	{
	    if (Types::TypeDecl* ty = ParseSimpleType(false))
	    {
		return ty;
	    }
	}
    }
    // Fall through:
    case Token::Integer:
    case Token::Char:
    case Token::Minus:
    {
	Types::TypeDecl* type = 0;
	return ParseRange(type, Token::Semicolon, Token::Of);
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
	return Error("Can't understand type");
    }
}

ExprAST* Parser::ParseIntegerExpr(const Token& token)
{
    int64_t          val = token.GetIntVal();
    const Location&  loc = token.Loc();
    Types::TypeDecl* type = Types::Get<Types::IntegerDecl>();

    if (val > std::numeric_limits<unsigned int>::max())
    {
	type = Types::Get<Types::Int64Decl>();
    }
    NextToken();
    return new IntegerExprAST(loc, val, type);
}

ExprAST* Parser::ParseStringExpr(const Token& token)
{
    int                            len = std::max(1, (int)(token.GetStrVal().length() - 1));
    std::vector<Types::RangeDecl*> rv = { new Types::RangeDecl(new Types::Range(0, len),
	                                                       Types::Get<Types::IntegerDecl>()) };
    Types::ArrayDecl*              ty = new Types::ArrayDecl(Types::Get<Types::CharDecl>(), rv);
    NextToken();
    return new StringExprAST(token.Loc(), token.GetStrVal(), ty);
}

void Parser::ParseLabels()
{
    AssertToken(Token::Label);
    CCIntegers labels{ Token::Semicolon };
    if (ParseSeparatedList(*this, labels))
    {
	for (auto n : labels.Values())
	{
	    if (!nameStack.Add(new LabelDef(n)))
	    {
		Error("Multiple label defintions?");
		return;
	    }
	}
    }
}

ExprAST* Parser::ParseExprElement()
{
    Token token = TranslateToken(CurrentToken());
    switch (token.GetToken())
    {
    case Token::Default:
	return ParseDefaultExpr();

    case Token::Nil:
	AssertToken(Token::Nil);
	return new NilExprAST(CurrentToken().Loc());

    case Token::Real:
	NextToken();
	return new RealExprAST(token.Loc(), token.GetRealVal(), Types::Get<Types::RealDecl>());

    case Token::Integer:
	return ParseIntegerExpr(token);

    case Token::Char:
	NextToken();
	return new CharExprAST(token.Loc(), token.GetIntVal(), Types::Get<Types::CharDecl>());

    case Token::StringLiteral:
	return ParseStringExpr(token);

    case Token::LeftParen:
	return ParseParenExpr();

    case Token::LeftSquare:
	return ParseSetExpr(0);

    case Token::Minus:
    case Token::Plus:
    case Token::Not:
	return ParseUnaryOp();

    case Token::SizeOf:
	return ParseSizeOfExpr();

    case Token::Identifier:
	return ParseIdentifierExpr(token);

    default:
	return 0;
    }
}

ExprAST* Parser::ParseBinOpRHS(int exprPrec, ExprAST* lhs)
{
    for (;;)
    {
	Token binOp = CurrentToken();
	int   tokPrec = binOp.Precedence();
	if (tokPrec < exprPrec)
	{
	    return lhs;
	}

	NextToken();

	ExprAST* rhs = ParseExprElement();
	if (!rhs)
	{
	    return 0;
	}

	// If the new operator binds less tightly, take it as LHS of
	// the next operator.
	int nextPrec = CurrentToken().Precedence();
	if (tokPrec < nextPrec)
	{
	    if (!(rhs = ParseBinOpRHS(tokPrec + 1, rhs)))
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
    assert((CurrentToken().GetToken() == Token::Minus || CurrentToken().GetToken() == Token::Plus ||
            CurrentToken().GetToken() == Token::Not) &&
           "Expected only minus at this time as a unary operator");

    Token oper = CurrentToken();
    NextToken();

    if (ExprAST* rhs = ParseExprElement())
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
    if (ExprAST* lhs = ParseExprElement())
    {
	return ParseBinOpRHS(0, lhs);
    }
    return 0;
}

class CCExpressions : public ListConsumer
{
public:
    CCExpressions() : ListConsumer{ Token::Comma, Token::RightSquare, ListConsumer::AllowEmpty::No } {}
    bool Consume(Parser& parser)
    {
	if (ExprAST* e = parser.ParseExpression())
	{
	    if (parser.AcceptToken(Token::DotDot))
	    {
		if (ExprAST* ee = parser.ParseExpression())
		{
		    e = new RangeExprAST(e->Loc(), e, ee);
		}
	    }
	    exprs.push_back(e);
	    return true;
	}
	return parser.Error<bool>("Expected index expression");
    }
    std::vector<ExprAST*>& Exprs() { return exprs; }

private:
    std::vector<ExprAST*> exprs;
};

ExprAST* Parser::ParseArrayExpr(ExprAST* expr, Types::TypeDecl*& type)
{
    TRACE();

    AssertToken(Token::LeftSquare);
    CCExpressions cce;
    size_t        taken = 0;
    bool          ok;
    while ((ok = ParseSeparatedList(*this, cce)) && AcceptToken(Token::LeftSquare))
    {
    }
    if (!ok)
    {
	return 0;
    }
    std::vector<ExprAST*> indices = cce.Exprs();
    while (!indices.empty())
    {
	RangeExprAST* range;
	if (auto dty = llvm::dyn_cast<Types::DynArrayDecl>(type))
	{
	    if (indices.size() != 1)
	    {
		return Error("Too many indices");
	    }

	    return new DynArrayExprAST(CurrentToken().Loc(), expr, indices[0], dty->Range(), dty->SubType());
	}
	else
	{
	    auto adecl = llvm::dyn_cast<Types::ArrayDecl>(type);
	    if (!adecl)
	    {
		return Error("Expected variable of array type when using index");
	    }
	    if ((range = llvm::dyn_cast<RangeExprAST>(indices[0])) && (indices.size() == 1))
	    {
		if (adecl->Ranges().size() == 1)
		{
		    expr = new ArraySliceAST(CurrentToken().Loc(), expr, range, adecl);
		}
		else
		{
		    return Error("Invalid slice expression, doesn't match size array dimensions");
		}
		indices.erase(indices.begin(), indices.begin() + 1);
	    }
	    else if (indices.size() >= adecl->Ranges().size())
	    {
		taken += adecl->Ranges().size();
		indices.resize(adecl->Ranges().size());
		expr = new ArrayExprAST(CurrentToken().Loc(), expr, indices, adecl->Ranges(),
		                        adecl->SubType());
		type = adecl->SubType();
		assert(type && "Expected a type here!");
		indices = cce.Exprs();
		indices.erase(indices.begin(), indices.begin() + taken);
		if (!(adecl = llvm::dyn_cast<Types::ArrayDecl>(type)))
		{
		    if (!indices.empty())
		    {
			return Error("Too many indices");
		    }
		    return expr;
		}
	    }
	    else
	    {
		return Error("Not enough indices");
	    }
	}
    }
    return expr;
}

ExprAST* Parser::MakeSimpleCall(ExprAST* expr, const PrototypeAST* proto, std::vector<ExprAST*>& args)
{
    assert(expr && "Expect to get an expression here");
    assert(proto && "Prototype should have been found here...");
    return new CallExprAST(CurrentToken().Loc(), expr, args, proto);
}

ExprAST* Parser::MakeCallExpr(const NamedObject* def, const std::string& funcName,
                              std::vector<ExprAST*>& args)
{
    TRACE();

    const PrototypeAST* proto = 0;
    ExprAST*            expr = 0;
    if (const auto funcDef = llvm::dyn_cast<const FuncDef>(def))
    {
	proto = funcDef->Proto();
	expr = new FunctionExprAST(CurrentToken().Loc(), proto);
    }
    else if (llvm::isa<const VarDef>(def))
    {
	if (auto fp = llvm::dyn_cast<Types::FuncPtrDecl>(def->Type()))
	{
	    proto = fp->Proto();
	    expr = new VariableExprAST(CurrentToken().Loc(), funcName, def->Type());
	}
	else
	{
	    return Error("Expected function pointer");
	}
    }
    else if (const auto m = llvm::dyn_cast_or_null<MembFuncDef>(def))
    {
	auto                   cd = llvm::dyn_cast<Types::ClassDecl>(m->Type());
	Types::MemberFuncDecl* mf = cd->GetMembFunc(m->Index());
	VariableExprAST*       self = new VariableExprAST(CurrentToken().Loc(), "self", cd);
	return MakeSelfCall(self, mf, cd, args);
    }
    else
    {
	assert(0 && "Huh? Strange call");
    }
    return MakeSimpleCall(expr, proto, args);
}

ExprAST* Parser::MakeSelfCall(ExprAST* self, Types::MemberFuncDecl* mf, Types::ClassDecl* cd,
                              std::vector<ExprAST*>& args)
{
    ExprAST*            expr = 0;
    const PrototypeAST* proto = mf->Proto();
    // Make sure we enumerate the index of virtual functions
    (void)cd->VTableType(true);
    if (mf->IsVirtual() || mf->IsOverride())
    {
	int                 index = mf->VirtIndex();
	Types::FuncPtrDecl* funcPtr = new Types::FuncPtrDecl(proto);
	expr = new VirtFunctionAST(CurrentToken().Loc(), self, index, funcPtr);
    }
    else
    {
	std::string fname = mf->LongName();
	expr = new FunctionExprAST(CurrentToken().Loc(), proto);
    }
    if (proto->HasSelf())
    {
	assert(self && "Should have a 'self' expression here");
	args.insert(args.begin(), self);
    }
    return MakeSimpleCall(expr, proto, args);
}

ExprAST* Parser::FindVariant(ExprAST* expr, Types::TypeDecl*& type, int fc, Types::VariantDecl* v,
                             const std::string& name)
{
    ExprAST* e = 0;
    int      elem = v->Element(name);
    if (elem >= 0)
    {
	const Types::FieldDecl* fd = v->GetElement(elem);
	type = fd->SubType();
	e = new VariantFieldExprAST(CurrentToken().Loc(), expr, fc, type);
	// If name is empty, we have a made up struct. Dig another level down.
	if (fd->Name() == "")
	{
	    auto r = llvm::dyn_cast<Types::RecordDecl>(fd->SubType());
	    assert(r && "Expect record declarataion");
	    elem = r->Element(name);
	    if (elem >= 0)
	    {
		type = r->GetElement(elem)->SubType();
		e = new FieldExprAST(CurrentToken().Loc(), e, elem, type);
	    }
	}
	return e;
    }

    for (int i = 0; i < v->FieldCount(); i++)
    {
	const Types::FieldDecl* fd = v->GetElement(i);
	if (fd->Name() == "")
	{
	    e = new VariantFieldExprAST(CurrentToken().Loc(), expr, fc, type);
	    const auto r = llvm::dyn_cast<Types::RecordDecl>(fd->SubType());
	    assert(r && "Expect record declarataion");
	    if ((elem = r->Element(name)) >= 0)
	    {
		e = new FieldExprAST(CurrentToken().Loc(), e, elem, type);
	    }
	    if (r->Variant())
	    {
		if ((e = FindVariant(e, type, r->FieldCount(), r->Variant(), name)))
		{
		    return e;
		}
	    }
	}
    }
    return 0;
}

ExprAST* Parser::ParseFieldExpr(ExprAST* expr, Types::TypeDecl*& type)
{
    AssertToken(Token::Period);
    std::string name = GetIdentifier(ExpectConsume);
    if (name.empty())
    {
	return 0;
    }
    std::string         typedesc;
    ExprAST*            e = 0;
    Types::VariantDecl* v = 0;
    unsigned            fc = 0;
    if (auto cd = llvm::dyn_cast<Types::ClassDecl>(type))
    {
	int elem = cd->Element(name);
	typedesc = "object";
	if (elem >= 0)
	{
	    std::string             objname;
	    const Types::FieldDecl* fd = cd->GetElement(elem, objname);
	    if (!fd)
	    {
		return Error("Field " + name + " not found in object");
	    }

	    type = fd->SubType();
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
		const NamedObject*     def = nameStack.Find(membfunc->LongName());

		std::vector<ExprAST*> args;
		if (!ParseArgs(def, args))
		{
		    return 0;
		}

		return MakeSelfCall(expr, membfunc, cd, args);
	    }
	    else
	    {
		fc = cd->FieldCount();
		v = cd->Variant();
	    }
	}
    }
    else if (auto rd = llvm::dyn_cast<Types::RecordDecl>(type))
    {
	typedesc = "record";
	int elem = rd->Element(name);
	if (elem >= 0)
	{
	    type = rd->GetElement(elem)->SubType();
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
	if (auto sd = llvm::dyn_cast<Types::StringDecl>(type))
	{
	    strlower(name);
	    if (name == "capacity")
	    {
		int cap = sd->Capacity();
		e = new IntegerExprAST(CurrentToken().Loc(), cap, Types::Get<Types::IntegerDecl>());
	    }
	}
	if (!e)
	{
	    return Error("Attempt to use field of variable that hasn't got fields");
	}
    }
    if (!e && v)
    {
	e = FindVariant(expr, type, fc, v, name);
    }
    if (!e)
    {
	return Error("Can't find element " + name + " in " + typedesc);
    }
    return e;
}

ExprAST* Parser::ParsePointerExpr(ExprAST* expr, Types::TypeDecl*& type)
{
    assert((CurrentToken().GetToken() == Token::Uparrow || CurrentToken().GetToken() == Token::At) &&
           "Expected @ or ^ token...");
    NextToken();
    if (auto fd = llvm::dyn_cast<Types::FileDecl>(type))
    {
	type = fd->SubType();
	return new FilePointerExprAST(CurrentToken().Loc(), expr, type);
    }
    if (auto pd = llvm::dyn_cast<Types::PointerDecl>(type))
    {
	type = pd->SubType();
	return new PointerExprAST(CurrentToken().Loc(), expr, type);
    }
    return Error("Expected pointer expression");
}

bool Parser::IsCall(const NamedObject* def)
{
    assert(def && "Expected def to be non-NULL");

    Types::TypeDecl* type = def->Type();
    if (llvm::isa<Types::FuncPtrDecl>(type))
    {
	return true;
    }
    if (llvm::isa<Types::ClassDecl>(type) && llvm::isa<MembFuncDef>(def))
    {
	if (CurrentToken().GetToken() != Token::Assign)
	{
	    return true;
	}
    }
    if ((llvm::isa<Types::FunctionDecl>(type) || llvm::isa<Types::MemberFuncDecl>(type)) &&
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
	unsigned            argNo = 0;
	const PrototypeAST* proto = 0;
	if (const auto funcDef = llvm::dyn_cast_or_null<FuncDef>(def))
	{
	    proto = funcDef->Proto();
	}
	else if (const auto varDef = llvm::dyn_cast_or_null<VarDef>(def))
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
		    return Error<bool>("Too many arguments");
		}
		isFuncArg = llvm::isa<Types::FuncPtrDecl>(funcArgs[argNo].Type());
	    }
	    ExprAST* arg = 0;
	    if (isFuncArg)
	    {
		std::string idName = GetIdentifier(ExpectConsume);
		if (!idName.empty())
		{
		    if (const NamedObject* argDef = nameStack.Find(idName))
		    {
			if (const auto fd = llvm::dyn_cast<FuncDef>(argDef))
			{
			    arg = new FunctionExprAST(CurrentToken().Loc(), fd->Proto());
			}
			else if (const auto vd = llvm::dyn_cast<VarDef>(argDef))
			{
			    if (llvm::isa<Types::FuncPtrDecl>(vd->Type()))
			    {
				arg = new VariableExprAST(CurrentToken().Loc(), idName, argDef->Type());
			    }
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
	    if (!AcceptToken(Token::Comma) && !Expect(Token::RightParen, NoExpectConsume))
	    {
		return false;
	    }
	    argNo++;
	}
    }
    return true;
}

VariableExprAST* Parser::ParseStaticMember(const TypeDef* def, Types::TypeDecl*& type)
{
    if (Expect(Token::Period, ExpectConsume))
    {
	std::string field = GetIdentifier(ExpectConsume);
	if (!field.empty())
	{
	    const auto od = llvm::dyn_cast<Types::ClassDecl>(def->Type());
	    int        elem;
	    if ((elem = od->Element(field)) >= 0)
	    {
		std::string             objname;
		const Types::FieldDecl* fd = od->GetElement(elem, objname);
		if (fd->IsStatic())
		{
		    type = fd->SubType();
		    std::string name = objname + "$" + field;
		    return new VariableExprAST(CurrentToken().Loc(), name, type);
		}
		return Error("Expected static variable '" + field + "'");
	    }
	    return Error("Expected member variabe name '" + field + "'");
	}
    }
    return 0;
}

ExprAST* Parser::ParseVariableExpr(const NamedObject* def)
{
    TRACE();

    ExprAST*         expr = 0;
    Types::TypeDecl* type = def->Type();
    assert(type && "Expect type here...");

    if (const auto w = llvm::dyn_cast<WithDef>(def))
    {
	expr = llvm::dyn_cast<AddressableAST>(w->Actual());
    }
    else
    {
	if (const auto m = llvm::dyn_cast<MembFuncDef>(def))
	{
	    Types::ClassDecl*      od = llvm::dyn_cast<Types::ClassDecl>(type);
	    Types::MemberFuncDecl* mf = od->GetMembFunc(m->Index());
	    type = mf->Proto()->Type();
	}
	else if (const auto ty = llvm::dyn_cast<TypeDef>(def))
	{
	    if (llvm::isa<Types::ClassDecl>(ty->Type()))
	    {
		expr = ParseStaticMember(ty, type);
	    }
	}
	if (!expr)
	{
	    if (auto fd = llvm::dyn_cast<Types::FunctionDecl>(type))
	    {
		type = fd->Proto()->Type();
	    }
	    if (auto cd = llvm::dyn_cast<ConstDef>(def))
	    {
		if (auto cc = llvm::dyn_cast<Constants::CompoundConstDecl>(cd->ConstValue()))
		{
		    expr = cc->Value();
		}
	    }
	    if (!expr)
	    {
		expr = new VariableExprAST(CurrentToken().Loc(), def->Name(), type);
	    }
	}
    }

    assert(expr && "Expected expression here");
    assert(type && "Type is supposed to be set here");

    return expr;
}

static ExprAST* CreateSetExprFromSetConst(const Constants::SetConstDecl* set)
{
    TRACE();
    std::vector<ExprAST*> setValues;
    for (auto v : set->Value())
    {
	ExprAST* e = 0;
	if (auto rd = llvm::dyn_cast<Constants::RangeConstDecl>(v))
	{
	    Types::Range r = rd->Value();
	    ExprAST*     start = new IntegerExprAST(rd->Loc(), r.Start(), rd->Type());
	    ExprAST*     end = new IntegerExprAST(rd->Loc(), r.End(), rd->Type());

	    e = new RangeExprAST(v->Loc(), start, end);
	}
	else
	{
	    e = new IntegerExprAST(v->Loc(), Constants::ToInt(v), v->Type());
	}
	assert(e && "Expected to have an ExprAST now!");
	setValues.push_back(e);
    }
    return new SetExprAST(set->Loc(), setValues, set->Type());
}

ExprAST* Parser::ParseCallOrVariableExpr(const Token& token)
{
    TRACE();

    std::string idName = token.GetIdentName();
    AssertToken(Token::Identifier);
    const NamedObject* def = nameStack.Find(idName);
    if (const auto constDef = llvm::dyn_cast_or_null<const ConstDef>(def))
    {
	const Constants::ConstDecl* cd = constDef->ConstValue();
	if (auto ed = llvm::dyn_cast<Constants::EnumConstDecl>(cd))
	{
	    return new IntegerExprAST(token.Loc(), ed->Value(), ed->Type());
	}
	if (auto sd = llvm::dyn_cast<Constants::SetConstDecl>(cd))
	{
	    return CreateSetExprFromSetConst(sd);
	}
    }
    if (const EnumDef* enumDef = llvm::dyn_cast_or_null<EnumDef>(def))
    {
	return new IntegerExprAST(token.Loc(), enumDef->Value(), enumDef->Type());
    }

    bool isBuiltin = Builtin::IsBuiltin(idName);
    if (!def && !isBuiltin)
    {
	return Error("Undefined name '" + idName + "'");
    }
    if (def)
    {
	if (!IsCall(def))
	{
	    return ParseVariableExpr(def);
	}
    }

    // Have to check twice for `def` as we need args for both
    // builtin and regular functions.
    std::vector<ExprAST*> args;
    if (!ParseArgs(def, args))
    {
	return 0;
    }

    if (def)
    {
	if (ExprAST* expr = MakeCallExpr(def, idName, args))
	{
	    return expr;
	}
    }

    assert(isBuiltin && "Should be a builtin function by now...");
    if (Builtin::FunctionBase* bif = Builtin::CreateBuiltinFunction(idName, args))
    {
	return new BuiltinExprAST(CurrentToken().Loc(), bif);
    }

    assert(0 && "Should not get here");
    return 0;
}

ExprAST* Parser::ParseIdentifierExpr(const Token& token)
{
    TRACE();

    ExprAST* expr = ParseCallOrVariableExpr(token);
    if (!expr)
    {
	return 0;
    }
    Types::TypeDecl* type = expr->Type();

    Token::TokenType tt = CurrentToken().GetToken();
    while (tt == Token::LeftSquare || tt == Token::Uparrow || tt == Token::At || tt == Token::Period)
    {
	assert(type && "Expect to have a type here...");
	switch (tt)
	{
	case Token::LeftSquare:
	    if (!(expr = ParseArrayExpr(expr, type)))
	    {
		return 0;
	    }
	    break;

	case Token::Uparrow:
	case Token::At:
	    if (!(expr = ParsePointerExpr(expr, type)))
	    {
		return 0;
	    }
	    break;

	case Token::Period:
	    if (ExprAST* tmp = ParseFieldExpr(expr, type))
	    {
		if (auto v = llvm::dyn_cast<AddressableAST>(tmp))
		{
		    expr = v;
		}
		else
		{
		    return tmp;
		}
	    }
	    else
	    {
		return Error("Failed to parse token");
	    }
	    break;

	default:
	    assert(0);
	}
	tt = CurrentToken().GetToken();
    }

    return expr;
}

ExprAST* Parser::ParseParenExpr()
{
    AssertToken(Token::LeftParen);
    ExprAST* v;
    if ((v = ParseExpression()) && Expect(Token::RightParen, ExpectConsume))
    {
	return v;
    }
    return 0;
}

class CCSetList : public ListConsumer
{
public:
    CCSetList() : ListConsumer{ Token::Comma, Token::RightSquare, ListConsumer::AllowEmpty::Yes } {}
    bool Consume(Parser& parser) override
    {
	if (ExprAST* v = parser.ParseExpression())
	{
	    if (parser.AcceptToken(Token::DotDot))
	    {
		ExprAST* vEnd = parser.ParseExpression();
		v = new RangeExprAST(parser.CurrentToken().Loc(), v, vEnd);
	    }
	    values.push_back(v);
	    return true;
	}
	return false;
    }
    std::vector<ExprAST*>& Values() { return values; }

private:
    std::vector<ExprAST*> values;
};

ExprAST* Parser::ParseSetExpr(Types::TypeDecl* setType)
{
    TRACE();
    AssertToken(Token::LeftSquare);

    const Location loc = CurrentToken().Loc();
    CCSetList      ccs;
    if (ParseSeparatedList(*this, ccs))
    {
	Types::TypeDecl* type = 0;
	if (!ccs.Values().empty())
	{
	    type = ccs.Values()[0]->Type();
	    for (auto i = ccs.Values().begin() + 1, e = ccs.Values().end(); i != e; i++)
	    {
		if ((*i)->Type() != type)
		{
		    return Error("Not all elements of set are same type");
		}
	    }
	}
	if (!setType)
	{
	    setType = new Types::SetDecl(0, type);
	}
	return new SetExprAST(loc, ccs.Values(), setType);
    }
    return 0;
}

ExprAST* Parser::ConstDeclToExpr(const Location& loc, Types::TypeDecl* ty, const Constants::ConstDecl* c)
{
    if (c->Type()->IsIntegral())
    {
	if (llvm::isa<Types::RealDecl>(ty))
	{
	    if (auto ci = llvm::dyn_cast<Constants::IntConstDecl>(c))
	    {
		return new RealExprAST(loc, ci->Value(), ty);
	    }
	    return Error("Real constant initializer from incompatible type");
	}
	return new IntegerExprAST(loc, Constants::ToInt(c), c->Type());
    }
    if (auto rc = llvm::dyn_cast<Constants::RealConstDecl>(c))
    {
	if (!llvm::isa<Types::RealDecl>(ty))
	{
	    return Error("Real constant initializer into incompatible type");
	}
	return new RealExprAST(loc, rc->Value(), rc->Type());
    }
    if (auto sc = llvm::dyn_cast<Constants::StringConstDecl>(c))
    {
	return new StringExprAST(loc, sc->Value(), sc->Type());
    }
    return Error("Unexpected constant type");
}

class CCArrayInitList : public ListConsumer
{
public:
    CCArrayInitList(Types::ArrayDecl* ty)
        : ListConsumer{ { Token::Comma, Token::Semicolon },
	                { Token::RightSquare },
	                ListConsumer::AllowEmpty::Yes }
        , type(ty)
    {
    }
    bool Consume(Parser& parser) override
    {

	if (const Constants::ConstDecl* cd = parser.ParseConstExpr(
	        { Token::Comma, Token::Colon, Token::DotDot }))
	{
	    int  value = Constants::ToInt(cd);
	    int  end = 0;
	    bool hasEnd = false;
	    if (parser.AcceptToken(Token::DotDot))
	    {
		cd = parser.ParseConstExpr({ Token::Colon });
		if (!cd)
		{
		    return false;
		}
		end = Constants::ToInt(cd);
		hasEnd = true;
	    }
	    parser.Expect(Token::Colon, Parser::ExpectConsume);
	    ExprAST* e = parser.ParseInitValue(type->SubType());
	    if (!e)
	    {
		return false;
	    }
	    if (hasEnd)
	    {
		list.push_back({ value, end, e });
	    }
	    else
	    {
		list.push_back({ value, e });
	    }
	    return true;
	}
	return false;
    }
    const std::vector<ArrayInit>& Values() { return list; }

private:
    std::vector<ArrayInit> list;
    Types::ArrayDecl*      type;
};

class CCRecordInitList : public ListConsumer
{
public:
    CCRecordInitList(Types::TypeDecl* ty)
        : ListConsumer{ Token::Semicolon, Token::RightSquare, ListConsumer::AllowEmpty::Yes }
        , type(llvm::dyn_cast<Types::FieldCollection>(ty))
    {
    }

    bool Consume(Parser& parser) override
    {
	std::vector<int> elems;
	do
	{
	    std::string fieldName = parser.GetIdentifier(Parser::ExpectConsume);
	    if (fieldName.empty())
	    {
		return false;
	    }
	    int                     elem = type->Element(fieldName);
	    const Types::FieldDecl* fty = nullptr;
	    if (elem >= 0)
	    {
		if (fty)
		{
		    if (fty != type->GetElement(elem))
		    {
			return parser.Error<bool>("Should be same types for field initializers");
		    }
		}
		else
		{
		    fty = type->GetElement(elem);
		}
		elems.push_back(elem);
	    }
	    else
	    {
		return parser.Error<bool>("Unknown record field: " + fieldName);
	    }
	    if (!parser.AcceptToken(Token::Comma))
	    {
		parser.Expect(Token::Colon, Parser::ExpectConsume);
		if (ExprAST* e = parser.ParseInitValue(fty->SubType()))
		{
		    list.push_back({ elems, e });
		    return true;
		}
	    }
	} while (true);
    }
    const std::vector<RecordInit>& Values() { return list; }

private:
    std::vector<RecordInit> list;
    Types::FieldCollection* type;
};

ExprAST* Parser::ParseInitValue(Types::TypeDecl* ty)
{
    const Location loc = CurrentToken().Loc();
    if (llvm::isa<Types::SetDecl>(ty))
    {
	if (ExprAST* e = ParseSetExpr(ty))
	{
	    return new InitValueAST(loc, ty, { e });
	}
	return 0;
    }
    if (llvm::isa<Types::PointerDecl>(ty))
    {
	if (AcceptToken(Token::Nil))
	{
	    return new InitValueAST(loc, ty, { new NilExprAST(loc) });
	}
	return Error("Expected NIL initallzier for pointer");
    }
    if (auto arrTy = llvm::dyn_cast<Types::ArrayDecl>(ty))
    {
	if (AcceptToken(Token::LeftSquare))
	{
	    CCArrayInitList cca(arrTy);
	    if (ParseSeparatedList(*this, cca))
	    {
		return new InitArrayAST(loc, ty, cca.Values());
	    }
	    return 0;
	}
	else if (!ty->IsStringLike())
	{
	    return 0;
	}
    }
    if (llvm::isa<Types::RecordDecl>(ty))
    {
	if (AcceptToken(Token::LeftSquare))
	{
	    CCRecordInitList ccr(ty);
	    if (ParseSeparatedList(*this, ccr))
	    {
		return new InitRecordAST(loc, ty, ccr.Values());
	    }
	    return 0;
	}
    }
    if (const Constants::ConstDecl* cd = ParseConstExpr(
            { Token::Comma, Token::Semicolon, Token::Colon, Token::RightSquare }))
    {
	return new InitValueAST(loc, ty, { ConstDeclToExpr(loc, ty, cd) });
    }
    return 0;
}

VarDeclAST* Parser::ParseVarDecls()
{
    TRACE();
    AssertToken(Token::Var);

    std::vector<VarDef> varList;
    do
    {
	bool    good = false;
	CCNames ccv(Token::Colon);
	if (ParseSeparatedList(*this, ccv))
	{
	    if (Types::TypeDecl* type = ParseType("", NoForwarding))
	    {
		for (auto n : ccv.Names())
		{
		    VarDef v(n, type);
		    varList.push_back(v);
		    if (!nameStack.Add(new VarDef(v)))
		    {
			return Error("Name '" + n + "' is already defined");
		    }
		}
		ExprAST* init = 0;
		if (AcceptToken(Token::Value))
		{
		    init = ParseInitValue(type);
		    if (!init)
		    {
			return 0;
		    }
		}
		else
		{
		    init = type->Init();
		}
		varList.back().SetInit(init);
		good = Expect(Token::Semicolon, ExpectConsume);
	    }
	}
	if (!good)
	{
	    return 0;
	}
    } while (CurrentToken().GetToken() == Token::Identifier);

    return new VarDeclAST(CurrentToken().Loc(), varList);
}

// functon name( { [var] name1, [,name2 ...]: type [; ...] } ) : type
// procedure name ( { [var] name1 [,name2 ...]: type [; ...] } )
// member function/procedure:
//   function classname.name{( args... )}: type;
//   procedure classname.name{ args... }
PrototypeAST* Parser::ParsePrototype(bool unnamed)
{
    assert((CurrentToken().GetToken() == Token::Procedure || CurrentToken().GetToken() == Token::Function) &&
           "Expected function or procedure token");

    bool                   isFunction = CurrentToken().GetToken() == Token::Function;
    PrototypeAST*          fwdProto = 0;
    Types::ClassDecl*      od = 0;
    Types::MemberFuncDecl* membfunc = 0;

    // Consume "function" or "procedure"
    NextToken();
    std::string funcName = "noname";
    if (!unnamed)
    {
	// Get function name.
	funcName = GetIdentifier(ExpectConsume);
	if (funcName.empty())
	{
	    return 0;
	}

	// Is it a member function?
	// FIXME: Nested classes, should we do this again?
	if (AcceptToken(Token::Period))
	{
	    if (Types::TypeDecl* ty = GetTypeDecl(funcName))
	    {
		if ((od = llvm::dyn_cast<Types::ClassDecl>(ty)))
		{
		    if (!Expect(Token::Identifier, NoExpectConsume))
		    {
			return 0;
		    }

		    std::string m = GetIdentifier(ExpectConsume);

		    int elem;
		    if ((elem = od->MembFunc(m)) >= 0)
		    {
			membfunc = od->GetMembFunc(elem);
		    }
		    if (!membfunc)
		    {
			return Error("Member function '" + m + "' not found in '" + funcName + "'.");
		    }
		    funcName = membfunc->LongName();

		    fwdProto = membfunc->Proto();
		}
	    }
	    if (!od)
	    {
		return Error("Expected object name");
	    }
	}
	// See if it's a "forward declaration"?
	if (const NamedObject* def = nameStack.Find(funcName))
	{
	    const auto fnDef = llvm::dyn_cast_or_null<const FuncDef>(def);
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

	VarDef::Flags flags = VarDef::Flags::None;
	while (!AcceptToken(Token::RightParen))
	{
	    if (CurrentToken().GetToken() == Token::Function || CurrentToken().GetToken() == Token::Procedure)
	    {
		if (PrototypeAST* proto = ParsePrototype(false))
		{
		    Types::TypeDecl* type = new Types::FuncPtrDecl(proto);
		    VarDef           v(proto->Name(), type);
		    args.push_back(v);
		    if (CurrentToken().GetToken() != Token::RightParen &&
		        !Expect(Token::Semicolon, ExpectConsume))
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
		if (AcceptToken(Token::Protected))
		{
		    flags |= VarDef::Flags::Protected;
		}
		if (AcceptToken(Token::Var))
		{
		    flags |= VarDef::Flags::Reference;
		}

		std::string arg = GetIdentifier(ExpectConsume);
		if (arg.empty())
		{
		    return 0;
		}

		names.push_back(arg);
		if (AcceptToken(Token::Colon))
		{
		    if (Types::TypeDecl* type = ParseType("", NoForwarding))
		    {
			for (auto n : names)
			{
			    VarDef v(n, type, flags);
			    args.push_back(v);
			}
			flags = VarDef::Flags::None;
			names.clear();
			if (CurrentToken().GetToken() != Token::RightParen &&
			    !Expect(Token::Semicolon, ExpectConsume))
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
		    if (!Expect(Token::Comma, ExpectConsume))
		    {
			return 0;
		    }
		}
	    }
	}
    }

    Types::TypeDecl* resultType;
    std::string      resultName = funcName;
    // If we have a function, expect ": type".
    if (isFunction)
    {
	if (AcceptToken(Token::Equal))
	{
	    resultName = GetIdentifier(ExpectConsume);
	}
	if (!Expect(Token::Colon, Parser::ExpectConsume) || !(resultType = ParseSimpleType(true)))
	{
	    return 0;
	}
    }
    else
    {
	resultType = Types::Get<Types::VoidDecl>();
    }

    if (fwdProto)
    {
	if (*resultType != *fwdProto->Type())
	{
	    return Error("Forward declared function should have same return type as definition.");
	}
	// TODO: Check argument types...
	return fwdProto;
    }

    assert(!od && "Expect no object here");
    PrototypeAST* proto = new PrototypeAST(CurrentToken().Loc(), funcName, args, resultType, resultName, 0);
    return proto;
}

ExprAST* Parser::ParseStatement()
{
    TRACE();
    switch (CurrentToken().GetToken())
    {
    case Token::Begin:
    {
	Location dummy;
	return ParseBlock(dummy);
    }

    case Token::Semicolon:
    case Token::End:
	// Empty block.
	return new BlockAST(CurrentToken().Loc(), {});

    default:
	if (ExprAST* expr = ParsePrimary())
	{
	    if (AcceptToken(Token::Assign))
	    {
		const Location loc = CurrentToken().Loc();
		ExprAST*       rhs = ParseExpression();
		if (rhs)
		{
		    expr = new AssignExprAST(loc, expr, rhs);
		}
		else
		{
		    return Error("Invalid assignment");
		}
	    }
	    return expr;
	}
	break;
    }
    return Error("Syntax error");
}

BlockAST* Parser::ParseBlock(Location& endLoc)
{
    TRACE();
    AssertToken(Token::Begin);

    std::vector<ExprAST*> v;
    // Build ast of the content of the block.
    const Location loc = CurrentToken().Loc();
    while (!AcceptToken(Token::End))
    {
	// Superfluous semicolons are discarded here
	AcceptToken(Token::Semicolon);
	if (CurrentToken().GetToken() == Token::Integer && PeekToken() == Token::Colon)
	{
	    Token token = CurrentToken();
	    AcceptToken(Token::Integer);
	    AcceptToken(Token::Colon);
	    int n = token.GetIntVal();
	    if (!nameStack.FindTopLevel(std::to_string(n)))
	    {
		std::cerr << "Label=" << n << " at " << token.Loc() << std::endl;
		return Error("Can't use label in a different scope than the declaration");
	    }
	    v.push_back(new LabelExprAST(token.Loc(), { { n, n } }, 0));
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
    TRACE();

    PrototypeAST* proto = ParsePrototype(false);
    if (!proto || !Expect(Token::Semicolon, ExpectConsume))
    {
	return 0;
    }

    const Location&    loc = CurrentToken().Loc();
    const std::string& name = proto->Name();
    NamedObject*       nmObj = 0;

    const NamedObject* def = nameStack.Find(name);
    const auto         fnDef = llvm::dyn_cast_or_null<const FuncDef>(def);
    std::string        shortname;
    if (!(fnDef && fnDef->Proto() && fnDef->Proto() == proto))
    {
	shortname = ShortName(name);
	// Allow "inline" keyword. Currently ignored...
	if (AcceptToken(Token::Inline))
	{
	    if (!Expect(Token::Semicolon, ExpectConsume))
	    {
		return 0;
	    }
	}
	if (Types::ClassDecl* cd = proto->BaseObj())
	{
	    int elem = cd->MembFunc(shortname);
	    if (elem < 0)
	    {
		return Error("Name '" + shortname + "' doesn't appear to be a member function...");
	    }
	    nmObj = new MembFuncDef(shortname, elem, proto->BaseObj());
	}
	else
	{
	    Types::TypeDecl* ty = new Types::FunctionDecl(proto);
	    nmObj = new FuncDef(name, ty, proto);
	    // Procedure = void function. Do not introdce a result name.
	    if (llvm::isa<Types::VoidDecl>(proto->Type()))
	    {
		shortname = "";
	    }
	    if (!nameStack.Add(nmObj))
	    {
		return Error("Name '" + name + "' already exists...");
	    }
	}
	if (AcceptToken(Token::Forward))
	{
	    proto->SetIsForward(true);
	    if (!Expect(Token::Semicolon, ExpectConsume))
	    {
		return 0;
	    }
	    return new FunctionAST(CurrentToken().Loc(), proto, {}, 0);
	}
    }

    // We're not in a forward declaration.
    if (proto->Function())
    {
	return Error("Duplicate defintion of " + proto->Name());
    }

    NameWrapper wrapper(nameStack);
    if (proto->HasSelf())
    {
	assert(proto->BaseObj() && "Expect base object!");
	VariableExprAST* v = new VariableExprAST(Location(), "self", proto->BaseObj());
	ExpandWithNames(proto->BaseObj(), v, 0);
    }

    if (shortname != "" && proto->ResName() != shortname)
    {
	assert(nmObj);
	VarDef* vd = new VarDef(proto->ResName(), proto->Type());
	nameStack.Add(vd);
    }

    for (auto v : proto->Args())
    {
	if (!nameStack.Add(new VarDef(v.Name(), v.Type())))
	{
	    return Error("Duplicate name '" + v.Name() + "'.");
	}
	if (auto dty = llvm::dyn_cast<Types::DynArrayDecl>(v.Type()))
	{
	    Types::DynRangeDecl* dr = dty->Range();
	    if (!nameStack.Add(new VarDef(dr->LowName(), dr->SubType())))
	    {
		return Error("Duplicate name '" + dr->LowName() + "'.");
	    }
	    if (!nameStack.Add(new VarDef(dr->HighName(), dr->SubType())))
	    {
		return Error("Duplicate name '" + dr->HighName() + "'.");
	    }
	}
    }

    std::vector<VarDeclAST*>  varDecls;
    BlockAST*                 body = 0;
    std::vector<FunctionAST*> subFunctions;
    for (;;)
    {
	switch (CurrentToken().GetToken())
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
	    if (FunctionAST* fn = ParseDefinition(level + 1))
	    {
		subFunctions.push_back(fn);
	    }
	    break;
	}

	case Token::Begin:
	{
	    Location endLoc;
	    assert(!body && "Multiple body declarations for function?");

	    if (!(body = ParseBlock(endLoc)) || !Expect(Token::Semicolon, ExpectConsume))
	    {
		return 0;
	    }

	    FunctionAST* fn = new FunctionAST(loc, proto, varDecls, body);
	    if (!proto->Function())
	    {
		proto->SetFunction(fn);
	    }
	    for (auto s : subFunctions)
	    {
		s->SetParent(fn);
	    }
	    fn->AddSubFunctions(subFunctions);
	    fn->EndLoc(endLoc);
	    return fn;
	}

	default:
	    return Error("Unexpected token.");
	}
    }
    return 0;
}

ExprAST* Parser::ParseIfExpr()
{
    TRACE();
    const Location loc = CurrentToken().Loc();
    AssertToken(Token::If);
    ExprAST* cond = ParseExpression();
    if (!cond || !Expect(Token::Then, ExpectConsume))
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
    AssertToken(Token::For);
    const Location loc = CurrentToken().Loc();

    std::string varName = GetIdentifier(ExpectConsume);
    if (varName.empty())
    {
	return Error("Expected identifier name, got " + CurrentToken().ToString());
    }
    const NamedObject* def = nameStack.Find(varName);
    if (!def)
    {
	return Error("Loop variable not found");
    }
    if (!llvm::isa<VarDef>(def))
    {
	return Error("Loop induction must be a variable");
    }
    VariableExprAST* varExpr = new VariableExprAST(CurrentToken().Loc(), varName, def->Type());
    if (AcceptToken(Token::Assign))
    {
	if (ExprAST* start = ParseExpression())
	{
	    bool             down = false;
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
	    if (end && Expect(Token::Do, ExpectConsume))
	    {
		if (ExprAST* body = ParseStatement())
		{
		    return new ForExprAST(loc, varExpr, start, end, down, body);
		}
	    }
	}
    }
    else if (AcceptToken(Token::In))
    {
	if (ExprAST* start = ParseExpression())
	{
	    if (Expect(Token::Do, ExpectConsume))
	    {
		if (ExprAST* body = ParseStatement())
		{
		    return new ForExprAST(loc, varExpr, start, body);
		}
	    }
	}
    }
    return 0;
}

ExprAST* Parser::ParseWhile()
{
    TRACE();
    const Location loc = CurrentToken().Loc();
    AssertToken(Token::While);
    ExprAST* cond = ParseExpression();
    if (cond && Expect(Token::Do, ExpectConsume))
    {
	if (ExprAST* stmt = ParseStatement())
	{
	    return new WhileExprAST(loc, cond, stmt);
	}
    }
    return 0;
}

ExprAST* Parser::ParseRepeat()
{
    TRACE();
    const Location loc = CurrentToken().Loc();
    AssertToken(Token::Repeat);
    std::vector<ExprAST*> v;
    const Location        loc2 = CurrentToken().Loc();
    while (!AcceptToken(Token::Until))
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

    if (ExprAST* cond = ParseExpression())
    {
	return new RepeatExprAST(loc, cond, new BlockAST(loc2, v));
    }
    return 0;
}

ExprAST* Parser::ParseCaseExpr()
{
    TRACE();
    const Location loc = CurrentToken().Loc();
    AssertToken(Token::Case);
    ExprAST* expr = ParseExpression();
    if (!expr || !Expect(Token::Of, ExpectConsume))
    {
	return 0;
    }
    std::vector<LabelExprAST*>       labels;
    std::vector<std::pair<int, int>> ranges;
    ExprAST*                         otherwise{ nullptr };
    Types::TypeDecl*                 type{ nullptr };

    do
    {
	if (CurrentToken().GetToken() == Token::Otherwise || CurrentToken().GetToken() == Token::Else)
	{
	    NextToken();
	    if (otherwise)
	    {
		return Error("An 'otherwise' or 'else' already used in this case block");
	    }
	    if (ranges.size())
	    {
		return Error("Can't have multiple case labels with 'otherwise' or 'else' case label");
	    }
	    const Location loc = CurrentToken().Loc();
	    otherwise = ParseStatement();
	    labels.push_back(new LabelExprAST(loc, {}, otherwise));
	    if (!ExpectSemicolonOrEnd())
	    {
		return 0;
	    }
	}
	else
	{
	    const Constants::ConstDecl* cd = ParseConstExpr({ Token::Comma, Token::Colon, Token::DotDot });
	    int                         value = Constants::ToInt(cd);
	    if (type)
	    {
		if (type != cd->Type())
		{
		    return Error("Expected case labels to have same type");
		}
	    }
	    else
	    {
		type = cd->Type();
	    }
	    int end = value;
	    if (AcceptToken(Token::DotDot))
	    {
		cd = ParseConstExpr({ Token::Comma, Token::Colon });
		if (type != cd->Type())
		{
		    return Error("Expected case labels to have same type");
		}
		end = Constants::ToInt(cd);
		if (end <= value)
		{
		    return Error("Expected case label range to be low..high");
		}
	    }
	    ranges.push_back({ value, end });
	}

	switch (CurrentToken().GetToken())
	{
	case Token::Comma:
	    AssertToken(Token::Comma);
	    break;

	case Token::Colon:
	{
	    NextToken();
	    const Location locColon = CurrentToken().Loc();
	    ExprAST*       s = ParseStatement();
	    labels.push_back(new LabelExprAST(locColon, ranges, s));
	    ranges.clear();
	    if (!ExpectSemicolonOrEnd())
	    {
		return 0;
	    }
	    break;
	}
	case Token::End:
	    break;

	default:
	    return Error("Syntax error: Expected ',' or ':' in case-statement.");
	}
    } while (!AcceptToken(Token::End));
    return new CaseExprAST(loc, expr, labels, otherwise);
}

void Parser::ExpandWithNames(const Types::FieldCollection* fields, ExprAST* v, int parentCount)
{
    TRACE();
    int vtableoffset = 0;
    if (const auto cd = llvm::dyn_cast<Types::ClassDecl>(fields))
    {
	vtableoffset = !!cd->VTableType(true);
    }
    for (int i = vtableoffset; i < fields->FieldCount() + vtableoffset; i++)
    {
	const Types::FieldDecl* f = fields->GetElement(i);
	Types::TypeDecl*        ty = f->SubType();
	if (f->Name() == "")
	{
	    auto rd = llvm::dyn_cast<Types::RecordDecl>(ty);
	    assert(rd && "Expected record declarataion here!");
	    ExprAST* vv = new VariantFieldExprAST(CurrentToken().Loc(), v, parentCount, ty);
	    ExpandWithNames(rd, vv, 0);
	    if (rd->Variant())
	    {
		ExpandWithNames(rd->Variant(), vv, rd->FieldCount());
	    }
	}
	else
	{
	    ExprAST* e;
	    if (llvm::isa<Types::RecordDecl>(fields) || llvm::isa<Types::ClassDecl>(fields))
	    {
		e = new FieldExprAST(CurrentToken().Loc(), v, i, ty);
	    }
	    else
	    {
		e = new VariantFieldExprAST(CurrentToken().Loc(), v, parentCount, ty);
	    }
	    nameStack.Add(new WithDef(f->Name(), e, f->SubType()));
	}
    }
    if (auto od = const_cast<Types::ClassDecl*>(llvm::dyn_cast<Types::ClassDecl>(fields)))
    {
	int count = od->MembFuncCount();
	for (int i = 0; i < count; i++)
	{
	    Types::MemberFuncDecl* mf = const_cast<Types::MemberFuncDecl*>(od->GetMembFunc(i));
	    std::string            name = mf->Proto()->Name();
	    nameStack.Add(new MembFuncDef(name, i, od));
	}
    }
}

class CCWith : public ListConsumer
{
public:
    CCWith(Stack<const NamedObject*>& ns)
        : ListConsumer{ Token::Comma, Token::Do, ListConsumer::AllowEmpty::No }, levels(0), nameStack(ns)
    {
    }
    bool Consume(Parser& parser) override
    {
	nameStack.NewLevel();
	levels++;
	if (parser.Expect(Token::Identifier, Parser::NoExpectConsume))
	{
	    ExprAST* e = parser.ParseIdentifierExpr(parser.CurrentToken());
	    if (auto v = llvm::dyn_cast_or_null<AddressableAST>(e))
	    {
		if (auto rd = llvm::dyn_cast<Types::RecordDecl>(v->Type()))
		{
		    parser.ExpandWithNames(rd, v, 0);
		    if (Types::VariantDecl* variant = rd->Variant())
		    {
			parser.ExpandWithNames(variant, v, rd->FieldCount());
		    }
		}
		else
		{
		    return parser.Error<bool>("Type for with statement should be a record type");
		}
	    }
	    else
	    {
		return parser.Error<bool>("With statement must contain only variable expression");
	    }
	    return true;
	}
	return false;
    }
    int Levels() { return levels; }

private:
    int                        levels;
    Stack<const NamedObject*>& nameStack;
};

ExprAST* Parser::ParseWithBlock()
{
    TRACE();
    const Location loc = CurrentToken().Loc();
    AssertToken(Token::With);

    CCWith ccw(nameStack);
    bool   error = !ParseSeparatedList(*this, ccw);

    ExprAST* body = 0;
    if (!error)
    {
	body = ParseStatement();
    }

    int levels = ccw.Levels();
    for (int i = 0; i < levels; i++)
    {
	nameStack.DropLevel();
    }

    if (!error)
    {
	return new WithExprAST(loc, body);
    }
    return 0;
}

class CCWrite : public ListConsumer
{
public:
    CCWrite(WriteAST::WriteKind knd)
        : ListConsumer{ Token::Comma, Token::RightParen, ListConsumer::AllowEmpty::No }, dest(0), kind(knd){};
    bool Consume(Parser& parser) override
    {
	WriteAST::WriteArg wa;
	if ((wa.expr = parser.ParseExpression()))
	{
	    if (!dest)
	    {
		if (auto vexpr = llvm::dyn_cast<AddressableAST>(wa.expr))
		{
		    if (kind == WriteAST::WriteKind::WriteStr)
		    {
			dest = vexpr;
			return true;
		    }
		    else if (llvm::isa<Types::FileDecl>(vexpr->Type()))
		    {
			dest = vexpr;
			return true;
		    }
		}
		if (!dest)
		{
		    dest = new VariableExprAST(parser.CurrentToken().Loc(), "output",
		                               Types::Get<Types::TextDecl>());
		}
	    }
	    if (parser.AcceptToken(Token::Colon))
	    {
		wa.width = parser.ParseExpression();
		if (!wa.width)
		{
		    return parser.Error<bool>("Invalid width expression");
		}
	    }
	    if (parser.AcceptToken(Token::Colon))
	    {
		wa.precision = parser.ParseExpression();
		if (!wa.precision)
		{
		    return parser.Error<bool>("Invalid precision expression");
		}
	    }
	    args.push_back(wa);

	    return true;
	}
	return false;
    }
    std::vector<WriteAST::WriteArg>& Args() { return args; }
    AddressableAST*                  Dest() { return dest; }

private:
    std::vector<WriteAST::WriteArg> args;
    AddressableAST*                 dest;
    WriteAST::WriteKind             kind;
};

ExprAST* Parser::ParseWrite()
{
    Token::TokenType    writeToken = CurrentToken().GetToken();
    WriteAST::WriteKind kind;
    switch (writeToken)
    {
    case Token::Write:
	kind = WriteAST::WriteKind::Write;
	break;
    case Token::Writeln:
	kind = WriteAST::WriteKind::WriteLn;
	break;
    case Token::WriteStr:
	kind = WriteAST::WriteKind::WriteStr;
	break;
    default:
	llvm_unreachable("Unexpected type of write operation");
	return 0;
    }

    NextToken();

    const Location                  loc = CurrentToken().Loc();
    AddressableAST*                 dest;
    std::vector<WriteAST::WriteArg> args;
    if (IsSemicolonOrEnd())
    {
	if (kind != WriteAST::WriteKind::WriteLn)
	{
	    return Error("Write or WriteStr must have arguments.");
	}
	dest = new VariableExprAST(loc, "output", Types::Get<Types::TextDecl>());
    }
    else
    {
	if (!Expect(Token::LeftParen, ExpectConsume))
	{
	    return 0;
	}
	CCWrite ccw(kind);
	if (!ParseSeparatedList(*this, ccw))
	{
	    return 0;
	}
	dest = ccw.Dest();
	args = ccw.Args();
    }
    return new WriteAST(loc, dest, args, kind);
}

class CCRead : public ListConsumer
{
public:
    CCRead(ReadAST::ReadKind knd)
        : ListConsumer{ Token::Comma, Token::RightParen, ListConsumer::AllowEmpty::No }, src(0), kind(knd)
    {
    }
    bool Consume(Parser& parser) override
    {
	if (ExprAST* expr = parser.ParseExpression())
	{
	    if (!src)
	    {
		if (auto vexpr = llvm::dyn_cast<AddressableAST>(expr))
		{
		    if (kind == ReadAST::ReadKind::ReadStr)
		    {
			src = vexpr;
			return true;
		    }
		    if (llvm::isa<Types::FileDecl>(vexpr->Type()))
		    {
			src = vexpr;
			return true;
		    }
		}
		if (!src)
		{
		    src = new VariableExprAST(parser.CurrentToken().Loc(), "input",
		                              Types::Get<Types::TextDecl>());
		}
	    }
	    args.push_back(expr);
	    return true;
	}
	return false;
    }
    std::vector<ExprAST*>& Args() { return args; }
    AddressableAST*        Src() { return src; }

private:
    std::vector<ExprAST*> args;
    AddressableAST*       src;
    ReadAST::ReadKind     kind;
};

ExprAST* Parser::ParseRead()
{
    const Location    loc = CurrentToken().Loc();
    Token::TokenType  readToken = CurrentToken().GetToken();
    ReadAST::ReadKind kind;
    switch (readToken)
    {
    case Token::Read:
	kind = ReadAST::ReadKind::Read;
	break;
    case Token::Readln:
	kind = ReadAST::ReadKind::ReadLn;
	break;
    case Token::ReadStr:
	kind = ReadAST::ReadKind::ReadStr;
	break;
    default:
	llvm_unreachable("Unexpected type of read operation");
	return 0;
    }

    NextToken();

    std::vector<ExprAST*> args;
    AddressableAST*       src = 0;
    if (IsSemicolonOrEnd())
    {
	if (kind != ReadAST::ReadKind::ReadLn)
	{
	    return Error("Read must have arguments.");
	}
	src = new VariableExprAST(loc, "input", Types::Get<Types::TextDecl>());
    }
    else
    {
	CCRead ccr(kind);
	if (Expect(Token::LeftParen, ExpectConsume) && ParseSeparatedList(*this, ccr))
	{
	    src = ccr.Src();
	    args = ccr.Args();
	}
	else
	{
	    return 0;
	}
    }
    return new ReadAST(loc, src, args, kind);
}

ExprAST* Parser::ParsePrimary()
{
    TRACE();
    Token token = CurrentToken();

    switch (token.GetToken())
    {
    case Token::Identifier:
	return ParseIdentifierExpr(token);

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
    case Token::WriteStr:
	return ParseWrite();

    case Token::Read:
    case Token::Readln:
    case Token::ReadStr:
	return ParseRead();

    case Token::Goto:
	return ParseGoto();

    default:
	NextToken();
	return Error("Syntax error");
    }
}

class CCProgram : public ListConsumer
{
public:
    CCProgram() : ListConsumer{ Token::Comma, Token::RightParen, ListConsumer::AllowEmpty::No } {}
    bool Consume(Parser& parser) override { return parser.Expect(Token::Identifier, Parser::ExpectConsume); }
};

bool Parser::ParseProgram(ParserType type)
{
    Token::TokenType t = Token::Program;
    if (type == ParserType::Unit)
    {
	t = Token::Unit;
    }
    if (Expect(t, ExpectConsume))
    {
	moduleName = GetIdentifier(ExpectConsume);
	if (type == ParserType::Program && AcceptToken(Token::LeftParen))
	{
	    CCProgram ccp;
	    return ParseSeparatedList(*this, ccp);
	}
	return true;
    }
    return false;
}

ExprAST* Parser::ParseUses()
{
    AssertToken(Token::Uses);
    std::string unitname = GetIdentifier(ExpectConsume);
    if (!unitname.empty())
    {
	if (unitname == "math")
	{
	    if (Expect(Token::Semicolon, ExpectConsume))
	    {
		// Math unit is "fake", so nothing inside it for now
		return new UnitAST(CurrentToken().Loc(), {}, 0, {});
	    }
	}
	else
	{
	    // TODO: Loop over comma separated list
	    strlower(unitname);
	    std::string path = GetPath(CurrentToken().Loc().FileName());
	    std::string fileName = path + "/" + unitname + ".pas";
	    FileSource  source(fileName);
	    if (!source)
	    {
		return Error("Could not open " + fileName);
	    }
	    Parser   p(source);
	    ExprAST* e = p.Parse(ParserType::Unit);
	    errCnt += p.GetErrors();
	    if (Expect(Token::Semicolon, ExpectConsume))
	    {
		if (auto ua = llvm::dyn_cast_or_null<UnitAST>(e))
		{
		    for (auto i : ua->Interface().List())
		    {
			if (!nameStack.Add(i.second))
			{
			    return 0;
			}
		    }
		}
		return e;
	    }
	}
    }
    return 0;
}

bool Parser::ParseInterface(InterfaceList& iList)
{
    NameWrapper wrapper(nameStack);
    AssertToken(Token::Interface);
    do
    {
	switch (CurrentToken().GetToken())
	{
	case Token::Type:
	    ParseTypeDef();
	    break;

	case Token::Uses:
	{
	    if (ExprAST* e = ParseUses())
	    {
		ast.push_back(e);
	    }
	    else
	    {
		return false;
	    }
	}
	break;

	case Token::Procedure:
	case Token::Function:
	{
	    PrototypeAST* proto = ParsePrototype(false);
	    if (!proto || !Expect(Token::Semicolon, ExpectConsume))
	    {
		return false;
	    }
	    proto->SetIsForward(true);
	    std::string      name = proto->Name();
	    Types::TypeDecl* ty = new Types::FunctionDecl(proto);
	    FuncDef*         nmObj = new FuncDef(name, ty, proto);
	    if (!nameStack.Add(nmObj))
	    {
		return Error<bool>("Interface name '" + name + "' already exists in...");
	    }
	}
	break;

	case Token::Var:
	    if (VarDeclAST* v = ParseVarDecls())
	    {
		ast.push_back(v);
	    }
	    break;

	case Token::Implementation:
	    break;

	default:
	    return Error<bool>("Unexpected token");
	    break;
	}
    } while (CurrentToken().GetToken() != Token::Implementation);
    for (auto i : nameStack.GetLevel())
    {
	iList.Add(i.second);
    }
    return true;
}

void Parser::ParseImports()
{
    AssertToken(Token::Import);
    while (AcceptToken(Token::Identifier))
    {
	Expect(Token::Semicolon, ExpectConsume);
    }
}

ExprAST* Parser::ParseUnit(ParserType type)
{
    const Location unitloc = CurrentToken().Loc();
    if (!ParseProgram(type) || !Expect(Token::Semicolon, ExpectConsume))
    {
	return 0;
    }

    // The "main" of the program - we call that "__PascalMain" so we can call it from C-code.
    std::string initName = "__PascalMain";
    // In a unit, we use the moduleName to form the "init functioin" name.
    if (type == ParserType::Unit)
    {
	initName = moduleName + ".init";
    }

    FunctionAST*  initFunction = 0;
    bool          finished = false;
    InterfaceList interfaceList;
    do
    {
	ExprAST* curAst = 0;
	switch (CurrentToken().GetToken())
	{
	case Token::EndOfFile:
	    return Error("Unexpected end of file");

	case Token::Uses:
	    curAst = ParseUses();
	    break;

	case Token::Import:
	    ParseImports();
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
	    for (auto i : interfaceList.List())
	    {
		if (!nameStack.Add(i.second))
		{
		    return 0;
		}
	    }
	    break;

	case Token::Implementation:
	    // Start a new level of names
	    AssertToken(Token::Implementation);
	    break;

	case Token::Begin:
	{
	    const Location loc = CurrentToken().Loc();
	    Location       endLoc;
	    BlockAST*      body = ParseBlock(endLoc);
	    if (!body)
	    {
		return 0;
	    }
	    PrototypeAST* proto = new PrototypeAST(loc, initName, std::vector<VarDef>(),
	                                           Types::Get<Types::VoidDecl>(), "", 0);
	    initFunction = new FunctionAST(loc, proto, {}, body);
	    initFunction->EndLoc(endLoc);
	    if (!Expect(Token::Period, ExpectConsume))
	    {
		return 0;
	    }
	    finished = true;
	    break;
	}

	case Token::End:
	    if (type != ParserType::Unit)
	    {
		return Error("Unexpected 'end' token");
	    }
	    AssertToken(Token::End);
	    if (!Expect(Token::Period, ExpectConsume))
	    {
		return 0;
	    }
	    finished = true;
	    break;

	default:
	    Error("Unexpected token");
	    NextToken();
	    break;
	}

	if (curAst)
	{
	    ast.push_back(curAst);
	}
    } while (!finished);
    return new UnitAST(unitloc, ast, initFunction, interfaceList);
}

ExprAST* Parser::Parse(ParserType type)
{
    TIME_TRACE();

    NextToken();
    if (type == ParserType::Program)
    {
	VarDef input("input", Types::Get<Types::TextDecl>(), VarDef::Flags::External);
	VarDef output("output", Types::Get<Types::TextDecl>(), VarDef::Flags::External);
	nameStack.Add(new VarDef(input));
	nameStack.Add(new VarDef(output));
	std::vector<VarDef> varList{ input, output };
	ast.push_back(new VarDeclAST(Location(), varList));
    }

    return ParseUnit(type);
}

Parser::Parser(Source& source) : lexer(source), nextTokenValid(false), errCnt(0)
{
    const llvm::fltSemantics& sem = llvm::APFloat::IEEEdouble();
    double                    maxReal = llvm::APFloat::getLargest(sem).convertToDouble();
    double                    minReal = llvm::APFloat::getLargest(sem, /*Negative=*/true).convertToDouble();
    Location                  unknownLoc;
    if (!(AddType("integer", Types::Get<Types::IntegerDecl>()) &&
          AddType("longint", Types::Get<Types::Int64Decl>()) &&
          AddType("int64", Types::Get<Types::Int64Decl>()) &&
          AddType("real", Types::Get<Types::RealDecl>()) && AddType("char", Types::Get<Types::CharDecl>()) &&
          AddType("text", Types::Get<Types::TextDecl>()) &&
          AddType("boolean", Types::Get<Types::BoolDecl>()) &&
          AddType("timestamp", Types::GetTimeStampType()) &&
          AddType("bindingtype", Types::GetBindingType()) &&
          AddType("complex", Types::Get<Types::ComplexDecl>()) &&
          nameStack.Add(new EnumDef("false", 0, Types::Get<Types::BoolDecl>())) &&
          nameStack.Add(new EnumDef("true", 1, Types::Get<Types::BoolDecl>())) &&
          AddConst("maxint", new Constants::IntConstDecl(unknownLoc, INT_MAX)) &&
          AddConst("maxchar", new Constants::IntConstDecl(unknownLoc, UCHAR_MAX)) &&
          AddConst("pi", new Constants::RealConstDecl(unknownLoc, llvm::numbers::pi)) &&
          AddConst("maxreal", new Constants::RealConstDecl(unknownLoc, maxReal)) &&
          AddConst("minreal", new Constants::RealConstDecl(unknownLoc, minReal)) &&
          AddConst("epsreal", new Constants::RealConstDecl(unknownLoc, 0x1.0p-52))))
    {
	assert(0 && "Failed to add builtin constants");
    }
}

ParserInterface& GetParser(Source& source)
{
    static Parser parser(source);
    return parser;
}
