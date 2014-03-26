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
    /* Token handling functions */
    const Token& CurrentToken() const;
    const Token& NextToken(const char* file, int line);
    const Token& PeekToken(const char* file, int line);

    /* Simple expression parsing */
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

    VariableExprAST* ParseArrayExpr(VariableExprAST* expr, Types::TypeDecl*& type);
    VariableExprAST* ParseFieldExpr(VariableExprAST* expr, Types::TypeDecl*& type);

    /* Control flow functionality */
    ExprAST* ParseRepeat();
    ExprAST* ParseIfExpr();
    ExprAST* ParseForExpr();
    ExprAST* ParseWhile();
    ExprAST* ParseCaseExpr();

    /* I/O functions */
    ExprAST*      ParseWrite();
    ExprAST*      ParseRead();

    /* Statements, blocks and calls. */
    ExprAST*      ParseStatement();
    ExprAST*      ParseStmtOrBlock();
    VarDeclAST*   ParseVarDecls();
    BlockAST*     ParseBlock();
    FunctionAST*  ParseDefinition();
    PrototypeAST* ParsePrototype(bool isFunction);

    /* Type declarations and defintitions */
    void                ParseTypeDef();
    void                ParseConstDef();
    Types::Range*       ParseRange();
    Types::Range*       ParseRangeOrTypeRange();
    Types::TypeDecl*    ParseSimpleType();
    Types::TypeDecl*    ParseType();
    Types::EnumDecl*    ParseEnumDef();
    Types::PointerDecl* ParsePointerType();
    Types::ArrayDecl*   ParseArrayDecl();
    Types::RecordDecl*  ParseRecordDecl();
    Types::TypeDecl*    ParseFileDecl();

    /* Helper for syntax checking */
    bool Expect(Token::TokenType type, bool eatIt, const char* file, int line);

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
    typedef Stack<NamedObject*> NameStack;
    typedef StackWrapper<NamedObject*> NameWrapper;
    Lexer&      lexer;
    Token       curToken;
    Token       nextToken;
    bool        nextTokenValid;
    std::string moduleName;
    int         errCnt;
    NameStack   nameStack;
    NameStack   usedVariables;
};

#endif
