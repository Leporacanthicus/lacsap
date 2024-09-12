#ifndef PARSER_H
#define PARSER_H

#include "expr.h"
#include "source.h"

enum class ParserType
{
    Program,
    Unit,
    Module,
};

class ParserInterface
{
public:
    virtual ExprAST* Parse(ParserType) = 0;
    virtual int      GetErrors() = 0;
    virtual ~ParserInterface() {}
};

ParserInterface& GetParser(Source& source);

#endif
