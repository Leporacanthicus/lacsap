#ifndef PARSER_H
#define PARSER_H

#include "expr.h"
#include "lexer.h"
#include "namedobject.h"
#include "source.h"
#include "stack.h"

#include <string>

using TerminatorList = std::vector<Token::TokenType>;

class Parser
{
public:
    enum ParserType
    {
	Program,
	Unit
    };

public:
    Parser(Source& source);
    ExprAST* Parse(ParserType type);

    int GetErrors() { return errCnt; }

    // Token handling functions
    const Token& CurrentToken() const;
    const Token& NextToken(const char* file, int line);
    const Token& PeekToken(const char* file, int line);

    // Simple expression parsing
    ExprAST* ParseExpression();
    ExprAST* ParseExprElement();
    ExprAST* ParseCallOrVariableExpr(Token token);
    ExprAST* ParseIdentifierExpr(Token token);
    ExprAST* ParseVariableExpr(const NamedObject* def);
    ExprAST* ParseIntegerExpr(Token token);
    ExprAST* ParseStringExpr(Token token);
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

    ExprAST*      ConstDeclToExpr(Location loc, const Constants::ConstDecl* c);
    ExprAST*      ParseInitValue(Types::TypeDecl* ty);

    // Type declarations and defintitions
    void  ParseTypeDef();
    void  ParseConstDef();
    Token TranslateToken(const Token& token);

    const Constants::ConstDecl* ParseConstExpr(const TerminatorList& terminators);
    const Constants::ConstDecl* ParseConstRHS(int exprPrec, const Constants::ConstDecl* lhs);
    Constants::ConstDecl*       ParseConstEval(const Constants::ConstDecl* lhs, const Token& binOp,
                                               const Constants::ConstDecl* rhs);
    const Constants::ConstDecl* ParseConstTerm(Location loc);

    Types::RangeDecl*   ParseRange(Types::TypeDecl*& type, Token::TokenType endToken,
                                   Token::TokenType altToken);
    Types::RangeDecl*   ParseRangeOrTypeRange(Types::TypeDecl*& type, Token::TokenType endToken,
                                              Token::TokenType altToken);
    Types::TypeDecl*    ParseSimpleType();
    Types::ClassDecl*   ParseClassDecl(const std::string& name);
    Types::TypeDecl*    ParseType(const std::string& name, bool maybeForwarded);
    Types::EnumDecl*    ParseEnumDef();
    Types::PointerDecl* ParsePointerType(bool maybeForwarded);
    Types::ArrayDecl*   ParseArrayDecl();
    bool                ParseFields(std::vector<Types::FieldDecl*>& fields, Types::VariantDecl*& variant,
                                    Token::TokenType type);
    Types::RecordDecl*  ParseRecordDecl();
    Types::FileDecl*    ParseFileDecl();
    Types::SetDecl*     ParseSetDecl();
    Types::StringDecl*  ParseStringDecl();
    Types::VariantDecl* ParseVariantDecl(Types::FieldDecl*& markerField);
    int64_t             ParseConstantValue(Token::TokenType& tt, Types::TypeDecl*& type);
    bool                ParseArgs(const NamedObject* def, std::vector<ExprAST*>& args);

    // Helper for syntax checking
    bool     Expect(Token::TokenType type, bool eatIt, const char* file, int line);
    void     AssertToken(Token::TokenType type, const char* file, int line);
    bool     AcceptToken(Token::TokenType type, const char* file, int line);
    bool     IsSemicolonOrEnd();
    bool     ExpectSemicolonOrEnd(const char* file, int line);
    unsigned ParseStringSize(Token::TokenType end);

    // General helper functions
    void ExpandWithNames(const Types::FieldCollection* fields, ExprAST* v, int parentCount);

    // Error functions - all the same except for the return type
    ExprAST*              Error(Token t, const std::string& msg);
    PrototypeAST*         ErrorP(Token t, const std::string& msg);
    FunctionAST*          ErrorF(Token t, const std::string& msg);
    Types::TypeDecl*      ErrorT(Token t, const std::string& msg);
    Types::RangeDecl*     ErrorR(Token t, const std::string& msg);
    VariableExprAST*      ErrorV(Token t, const std::string& msg);
    Constants::ConstDecl* ErrorC(Token t, const std::string& msg);
    int                   ErrorI(Token t, const std::string& msg);

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

void AddClosureArg(FunctionAST* fn, std::vector<ExprAST*>& args);

#endif
