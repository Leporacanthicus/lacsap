#ifndef PARSER_H
#define PARSER_H

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
public:
    Parser(const std::string& fname);
    ExprAST* Parse(ParserType type);

    int GetErrors() { return errCnt; }

private:
    // Token handling functions
    const Token& CurrentToken() const;
    const Token& NextToken(const char* file, int line);
    const Token& PeekToken(const char* file, int line);

    // Simple expression parsing
    ExprAST* ParseExpression();
    ExprAST* ParseIdentifierExpr();
    ExprAST* ParseIntegerExpr(Token token);
    ExprAST* ParseStringExpr(Token token);
    ExprAST* ParseParenExpr();
    ExprAST* ParsePrimary();
    ExprAST* ParseBinOpRHS(int exprPrec, ExprAST* lhs);
    ExprAST* ParseUnaryOp();
    ExprAST* ParseSetExpr();
    ExprAST* ParseSizeOfExpr();

    ExprAST* ParseIntegerOrLabel(Token token);

    VariableExprAST* ParseArrayExpr(VariableExprAST* expr, Types::TypeDecl*& type);
    VariableExprAST* ParsePointerExpr(VariableExprAST* expr, Types::TypeDecl*& type);
    ExprAST* ParseFieldExpr(VariableExprAST* expr, Types::TypeDecl*& type);
    VariableExprAST* ParseStaticMember(TypeDef* def, Types::TypeDecl*& type);

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
    BlockAST*     ParseBlock();
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

    Types::RangeDecl*   ParseRange(Types::TypeDecl*& type);
    Types::RangeDecl*   ParseRangeOrTypeRange(Types::TypeDecl*& type);
    Types::TypeDecl*    ParseSimpleType();
    Types::ClassDecl*   ParseClassDecl(const std::string& name);
    Types::TypeDecl*    ParseType(const std::string& name);
    Types::EnumDecl*    ParseEnumDef();
    Types::PointerDecl* ParsePointerType();
    Types::ArrayDecl*   ParseArrayDecl();
    bool                ParseFields(std::vector<Types::FieldDecl*>& fields, 
				    Types::VariantDecl*& variant,
				    Token::TokenType type);
    Types::RecordDecl*  ParseRecordDecl();
    Types::FileDecl*    ParseFileDecl();
    Types::SetDecl*     ParseSetDecl();
    Types::StringDecl*  ParseStringDecl();
    Types::VariantDecl* ParseVariantDecl(Types::TypeDecl*& type);
    int                 ParseConstantValue(Token::TokenType& tt, Types::TypeDecl*& type);
    bool                ParseArgs(const NamedObject* def, std::vector<ExprAST*>& args);

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
    ExprAST* Error(const std::string& msg, const char* file = 0, int line = 0);
    PrototypeAST* ErrorP(const std::string& msg);
    FunctionAST* ErrorF(const std::string& msg);
    Types::TypeDecl* ErrorT(const std::string& msg);
    Types::RangeDecl* ErrorR(const std::string& msg);
    VariableExprAST* ErrorV(const std::string& msg);
    int ErrorI(const std::string& msg);
    Constants::ConstDecl* ErrorC(const std::string& msg);

    // Helper functions for expression evaluation.
    bool     IsCall(const NamedObject* def);
    ExprAST* MakeCallExpr(VariableExprAST* expr,
			  NamedObject* def,
			  const std::string& funcName,
			  std::vector<ExprAST*>& args);
    // Helper functions for identifier access/checking.
    EnumDef* GetEnumValue(const std::string& name);
    Types::TypeDecl* GetTypeDecl(const std::string& name);
    const Constants::ConstDecl* GetConstDecl(const std::string& name);
    bool AddType(const std::string& name, Types::TypeDecl* type);
    bool AddConst(const std::string& name, const Constants::ConstDecl* cd);
    
private:
    typedef StackWrapper<NamedObject*> NameWrapper;

    Lexer                 lexer;
    Token                 curToken;
    Token                 nextToken;
    bool                  nextTokenValid;
    std::string           moduleName;
    std::string           fileName;
    int                   errCnt;
    Stack<NamedObject*>   nameStack;
    Stack<NamedObject*>   usedVariables;
    std::vector<ExprAST*> ast;
};

#endif
