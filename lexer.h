#ifndef LEXER_H
#define LEXER_H

#include "location.h"
#include "token.h"
#include "types.h"
#include "constants.h"
#include "source.h"
#include <string>
#include <fstream>
#include <exception>

class Lexer
{
public:
    Lexer(Source &source);
    Token GetToken();

private:
    int NextChar();
    int CurChar();
    int PeekChar();
    int GetChar();

    Token NumberToken();
    Token StringToken();

    Location Where() const { return Location(source); }

private:
    Source &source;
    int     curChar;
    int     nextChar;
    int     curValid;
};

#endif
