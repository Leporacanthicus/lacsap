#ifndef PARSER_H
#define PARSER_H

#include "source.h"
#include "namedobject.h"
#include "stack.h"
#include "expr.h"

#include <string>

class Parser
{
public:
    enum ParserType
    {
	Program,
	Unit
    };

    class CommaConsumer
    {
    public:
	virtual bool Consume(Parser& parser) = 0;
	virtual ~CommaConsumer() {}
    };

public:
    Parser(Source &source);
    ExprAST* Parse(ParserType type);

    int GetErrors() { return errCnt; }

    // Token handling functions
    const Token& CurrentToken() const;
    const Token& NextToken(const char* file, int line);
    const Token& PeekToken(const char* file, int line);

    // Simple expression parsing
    ExprAST* ParseExpression();
    ExprAST* ParseExprElement();
    ExprAST* ParseIdentifierExpr(Token token);
    ExprAST* ParseVariableExpr(const NamedObject* def);
    ExprAST* ParseIntegerExpr(Token token);
    ExprAST* ParseStringExpr(Token token);
    ExprAST* ParseParenExpr();
    ExprAST* ParsePrimary();
    ExprAST* ParseBinOpRHS(int exprPrec, ExprAST* lhs);
    ExprAST* ParseUnaryOp();
    ExprAST* ParseSetExpr();
    ExprAST* ParseSizeOfExpr();
    bool ParseCommaList(CommaConsumer& cc, Token::TokenType end, bool allowEmpty);

    VariableExprAST* ParseArrayExpr(VariableExprAST* expr, Types::TypeDecl*& type);
    VariableExprAST* ParsePointerExpr(VariableExprAST* expr, Types::TypeDecl*& type);
    VariableExprAST* FindVariant(VariableExprAST* expr, Types::TypeDecl*& type, int fc,
				 Types::VariantDecl* v, const std::string& name);
    ExprAST* ParseFieldExpr(VariableExprAST* expr, Types::TypeDecl*& type);
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

    // Type declarations and defintitions
    void ParseTypeDef();
    void ParseConstDef();
    Token TranslateToken(const Token& token);

    const Constants::ConstDecl* ParseConstExpr();
    const Constants::ConstDecl* ParseConstRHS(int exprPrec, const Constants::ConstDecl* lhs);
    Constants::ConstDecl* ParseConstEval(const Constants::ConstDecl* lhs,
					 const Token& binOp,
					 const Constants::ConstDecl* rhs);
    const Constants::ConstDecl* ParseConstTerm(Location loc);

    Types::RangeDecl*   ParseRange(Types::TypeDecl*& type);
    Types::RangeDecl*   ParseRangeOrTypeRange(Types::TypeDecl*& type);
    Types::TypeDecl*    ParseSimpleType();
    Types::ClassDecl*   ParseClassDecl(const std::string& name);
    Types::TypeDecl*    ParseType(const std::string& name, bool maybeForwarded);
    Types::EnumDecl*    ParseEnumDef();
    Types::PointerDecl* ParsePointerType(bool maybeForwarded);
    Types::ArrayDecl*   ParseArrayDecl();
    bool                ParseFields(std::vector<Types::FieldDecl*>& fields, 
				    Types::VariantDecl*& variant,
				    Token::TokenType type);
    Types::RecordDecl*  ParseRecordDecl();
    Types::FileDecl*    ParseFileDecl();
    Types::SetDecl*     ParseSetDecl();
    Types::StringDecl*  ParseStringDecl();
    Types::VariantDecl* ParseVariantDecl(Types::FieldDecl*& markerField);
    int64_t ParseConstantValue(Token::TokenType& tt, Types::TypeDecl*& type);
    bool ParseArgs(const NamedObject* def, std::vector<ExprAST*>& args);

    // Helper for syntax checking
    bool Expect(Token::TokenType type, bool eatIt, const char* file, int line);
    void AssertToken(Token::TokenType type, const char* file, int line);
    bool AcceptToken(Token::TokenType type, const char* file, int line);
    bool IsSemicolonOrEnd();
    bool ExpectSemicolonOrEnd(const char* file, int line);
    unsigned ParseStringSize();

    // General helper functions
    void ExpandWithNames(const Types::FieldCollection* fields, VariableExprAST* v, int parentCount);

    /* Error functions - all the same except for the return type */
    ExprAST* Error(Token t, const std::string& msg);
    PrototypeAST* ErrorP(Token t, const std::string& msg);
    FunctionAST* ErrorF(Token t, const std::string& msg);
    Types::TypeDecl* ErrorT(Token t, const std::string& msg);
    Types::RangeDecl* ErrorR(Token t, const std::string& msg);
    VariableExprAST* ErrorV(Token t, const std::string& msg);
    Constants::ConstDecl* ErrorC(Token t, const std::string& msg);
    int ErrorI(Token t, const std::string& msg);

    // Helper functions for expression evaluation.
    bool     IsCall(const NamedObject* def);
    ExprAST* MakeCallExpr(const NamedObject* def,
			  const std::string& funcName,
			  std::vector<ExprAST*>& args);
    ExprAST* MakeSimpleCall(ExprAST* expr, const PrototypeAST* proto, std::vector<ExprAST*>& args);
    ExprAST* MakeSelfCall(VariableExprAST* self, Types::MemberFuncDecl* mf, Types::ClassDecl* cd,
			  std::vector<ExprAST*>& args);

    // Helper functions for identifier access/checking.
    const EnumDef* GetEnumValue(const std::string& name);
    Types::TypeDecl* GetTypeDecl(const std::string& name);
    const Constants::ConstDecl* GetConstDecl(const std::string& name);
    bool AddType(const std::string& name, Types::TypeDecl* type);
    bool AddConst(const std::string& name, const Constants::ConstDecl* cd);

    std::vector<VarDef> CalculateUsedVars(FunctionAST* fn,
					  const std::vector<const NamedObject*>& varsUsed,
					  const Stack<const NamedObject*>& nameStack);

private:
    typedef StackWrapper<const NamedObject*> NameWrapper;

    Lexer                 lexer;
    Token                 curToken;
    Token                 nextToken;
    bool                  nextTokenValid;
    std::string           moduleName;
    int                   errCnt;
    Stack<const NamedObject*> nameStack;
    std::vector<ExprAST*> ast;
};

void AddClosureArg(FunctionAST* fn, std::vector<ExprAST*>& args);

#endif
