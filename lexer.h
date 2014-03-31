#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include "types.h"
#include "constants.h"
#include <string>
#include <fstream>
#include <exception>

class LexerException : public std::exception
{
public:
    LexerException(std::string r)
    {
	reason = r;
    }
    const char* what() const  noexcept { return (std::string("Lexer Exception") + reason).c_str(); }

private:
    std::string reason;
};

class Lexer
{
public:
    Lexer(const std::string& sourceFile) throw(LexerException);
    Token GetToken();

private:
    int NextChar();
    int CurChar();
    int PeekChar();

    Token NumberToken();

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

bool IsKeyWord(const std::string& name);

#endif
