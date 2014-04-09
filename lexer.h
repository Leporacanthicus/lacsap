#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include "types.h"
#include "constants.h"
#include <string>
#include <fstream>
#include <exception>

class Lexer
{
public:
    Lexer(const std::string& sourceFile);
    Token GetToken();

    bool Good() { return inFile.good(); }

private:
    int NextChar();
    int CurChar();
    int PeekChar();

    Token NumberToken();
    Token StringToken();

    Location Where() const { return Location(fName, lineNo, column); }

private:
    std::string   fName;
    int           lineNo;
    int           column;
    std::ifstream inFile;
    int           curChar;
    int           nextChar;
    int           curValid;
};

#endif
