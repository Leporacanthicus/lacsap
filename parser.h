#ifndef PARSER_H
#define PARSER_H

#include "namedobject.h"
#include "stack.h"
#include "expr.h"

#include <string>

class Parser
{
public:
    Parser(Lexer &l);
    std::vector<ExprAST*> Parse();

    int GetErrors() { return errCnt; } 

private:
    // Token handling functions
    const Token& CurrentToken() const;
    const Token& NextToken(const char* file, int line);
    const Token& PeekToken(const char* file, int line);

    // Simple expression parsing
    ExprAST* ParseExpression();
    ExprAST* ParseIdentifierExpr();
    ExprAST* ParseRealExpr(Token token);
    ExprAST* ParseIntegerExpr(Token token);
    ExprAST* ParseCharExpr(Token token);
    ExprAST* ParseStringExpr(Token token);
    ExprAST* ParseParenExpr();
    ExprAST* ParsePrimary();
    ExprAST* ParseBinOpRHS(int exprPrec, ExprAST* lhs);
    ExprAST* ParseUnaryOp();
    ExprAST* ParseSetExpr();
    ExprAST* ParseNilExpr();

    VariableExprAST* ParseArrayExpr(VariableExprAST* expr, Types::TypeDecl*& type);
    VariableExprAST* ParseFieldExpr(VariableExprAST* expr, Types::TypeDecl*& type);
    VariableExprAST* ParsePointerExpr(VariableExprAST* expr, Types::TypeDecl*& type);

    // Control flow functionality
    ExprAST* ParseRepeat();
    ExprAST* ParseIfExpr();
    ExprAST* ParseForExpr();
    ExprAST* ParseWhile();
    ExprAST* ParseCaseExpr();
    ExprAST* ParseWithBlock();

    // I/O functions
    ExprAST*      ParseWrite();
    ExprAST*      ParseRead();

    // Statements, blocks and calls
    ExprAST*      ParseStatement();
    ExprAST*      ParseStmtOrBlock();
    VarDeclAST*   ParseVarDecls();
    BlockAST*     ParseBlock();
    FunctionAST*  ParseDefinition(int level);
    PrototypeAST* ParsePrototype();
    bool          ParseProgram();

    // Type declarations and defintitions
    void          ParseTypeDef();
    void          ParseConstDef();
    void          TranslateToken(Token& token);

    Constants::ConstDecl* ParseConstExpr();
    Constants::ConstDecl* ParseConstRHS(int exprPrec, Constants::ConstDecl* lhs);
    Constants::ConstDecl* ParseConstEval(const Constants::ConstDecl* lhs, 
					 const Token& token, 
					 const Constants::ConstDecl* rhs);

    Types::Range*       ParseRange();
    Types::Range*       ParseRangeOrTypeRange();
    Types::TypeDecl*    ParseSimpleType();
    Types::TypeDecl*    ParseType();
    Types::EnumDecl*    ParseEnumDef();
    Types::PointerDecl* ParsePointerType();
    Types::ArrayDecl*   ParseArrayDecl();
    Types::RecordDecl*  ParseRecordDecl();
    Types::FileDecl*    ParseFileDecl();
    Types::SetDecl*     ParseSetDecl();
    Types::StringDecl*  ParseStringDecl();
    Types::VariantDecl* ParseVariantDecl();
    int                 ParseConstantValue(Token::TokenType& tt);

    // Helper for syntax checking
    bool Expect(Token::TokenType type, bool eatIt, const char* file, int line);
    bool ExpectSemicolonOrEnd(const char* file, int line);

    // General helper functions
    void ExpandWithNames(const Types::FieldCollection* fields, VariableExprAST* v, int parentCount);

    /* Error functions - all the same except for the return type */
    ExprAST*         Error(const std::string& msg, const char* file = 0, int line = 0);
    PrototypeAST*    ErrorP(const std::string& msg);
    FunctionAST*     ErrorF(const std::string& msg);
    Types::TypeDecl* ErrorT(const std::string& msg);
    Types::Range*    ErrorR(const std::string& msg);
    VariableExprAST* ErrorV(const std::string& msg);

    // Helper functions for expression evaluation.
    bool IsCall(Types::TypeDecl* type);
    
    // Helper functions for identifier access/checking.
    EnumDef* GetEnumValue(const std::string& name);
    Types::TypeDecl* GetTypeDecl(const std::string& name);
    Constants::ConstDecl* GetConstDecl(const std::string& name);
    bool AddType(const std::string& name, Types::TypeDecl* type);

private:
    typedef StackWrapper<NamedObject*> NameWrapper;
    Lexer&      lexer;
    Token       curToken;
    Token       nextToken;
    bool        nextTokenValid;
    std::string moduleName;
    int         errCnt;
    Stack<NamedObject*> nameStack;
    Stack<NamedObject*> usedVariables;
};

#endif
