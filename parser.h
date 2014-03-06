#ifndef PARSER_H
#define PARSER_H

#include "variables.h"
#include "namedobject.h"
#include "stack.h"
#include "expr.h"

#include <string>

class Parser
{
public:
    Parser(Lexer &l);
    ExprAST* Parse();

    int GetErrors() { return errCnt; } 

private:
    const Token& CurrentToken() const;
    const Token& NextToken(const char* file, int line);
    const Token& PeekToken(const char* file, int line);

    ExprAST* ParseExpression();
    ExprAST* ParseIdentifierExpr();
    ExprAST* ParseRealExpr();
    ExprAST* ParseIntegerExpr();
    ExprAST* ParseCharExpr();
    ExprAST* ParseStringExpr();
    ExprAST* ParseParenExpr();
    ExprAST* ParsePrimary();
    ExprAST* ParseStatement();
    ExprAST* ParseReturnExpr();
    ExprAST* ParseBinOpRHS(int exprPrec, ExprAST* lhs);
    ExprAST* ParseIfExpr();
    ExprAST* ParseForExpr();
    ExprAST* ParseWhile();
    ExprAST* ParseRepeat();
    ExprAST* ParseWrite();
    ExprAST* ParseRead();
    ExprAST* ParseUnaryOp();
    ExprAST* ParseStmtOrBlock();
    VarDeclAST* ParseVarDecls();
    BlockAST* ParseBlock();
    FunctionAST* ParseDefinition();
    PrototypeAST* ParsePrototype(bool isFunction);

    Types::TypeDecl* ParseSimpleType();
    Types::TypeDecl* ParseType();

    bool Expect(Token::TokenType type, bool eatIt, const char* file, int line);

    ExprAST* Error(const std::string& msg, const char* file = 0, int line = 0);
    PrototypeAST* ErrorP(const std::string& msg);
    FunctionAST* ErrorF(const std::string& msg);
    Types::TypeDecl* ErrorT(const std::string& msg);

private:
    typedef Stack<NamedObject*> NameStack;
    typedef StackWrapper<NamedObject*> NameWrapper;
    Lexer& lexer;
    Token curToken;
    Token nextToken;
    bool nextTokenValid;
    std::string moduleName;
    int errCnt;
    NameStack nameStack;
    Types types;
};

#endif
