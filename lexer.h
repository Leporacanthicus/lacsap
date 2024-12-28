#ifndef LEXER_H
#define LEXER_H

#include "location.h"
#include "source.h"
#include "token.h"
#include <string>

class Lexer
{
public:
    Lexer(Source& source);
    Token   GetToken();
    Source& GetSource() { return source; }

private:
    int NextChar();
    int CurChar();
    int PeekChar();
    int GetChar();

    Token NumberToken();
    Token StringToken();

    Location Where() const { return source; }

private:
    Source& source;
    int     curChar;
    int     nextChar;
    int     curValid;
};

#endif
