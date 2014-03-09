#include "lexer.h"
#include "types.h"
#include <cassert>
#include <cctype>
#include <iostream>

int Lexer::CurChar()
{
    if (!curValid)
    {
	NextChar();
	curValid++;
    }
    return curChar;
}

int Lexer::NextChar()
{
    if (curValid > 1)
    {
	curValid--;
	curChar = nextChar;
	return curChar;
    }

    curChar = inFile.get();
    if (curChar == '\n')
    {
	lineNo++;
	column = 0; 
    }
    else
    {
	column++;
    }
    return curChar;
}

int Lexer::PeekChar()
{
    if (curValid > 1)
    {
	return nextChar;
    }
    curValid++;
    nextChar = inFile.get();
    return nextChar;
}    

Token Lexer::GetToken()
{
    int ch = CurChar();
    Location w = Where();

    do
    {
	while(isspace(ch))
	{
	    ch = NextChar();
	}
	
	if (ch == '{')
	{
	    while ((ch = NextChar()) != EOF && ch != '}')
		;
	    ch = NextChar();
	}
	if (ch == '(' && PeekChar() == '*')
	{
	    ch = NextChar(); /* Skip first * */
	    while ((ch = NextChar()) != EOF && (ch != '*' && PeekChar() != ')'))
		;
	    NextChar();
	    ch = NextChar();
	}
    } while(isspace(ch));

    // EOF -> return now...
    if (ch == EOF)
    {
	return Token(Token::EndOfFile, w);
    }

    Token::TokenType tt = Token::Unknown;
    switch(ch)
    {
    case '(':
	tt = Token::LeftParen;
	break;

    case ')':
	tt = Token::RightParen;
	break;

    case '+':
	tt = Token::Plus;
	break;

    case '-':
	tt = Token::Minus;
	break;

    case '*':
	tt = Token::Multiply;
	break;

    case '/':
	tt = Token::Divide;
	break;
	
    case ',':
	tt = Token::Comma;
	break;

    case ';':
	tt = Token::Semicolon;
	break;

    case '.':
	tt = Token::Period;
	if (PeekChar() == '.')
	{
	    NextChar();
	    tt = Token::DotDot;
	}
	break;

    case '<':
	tt = Token::LessThan;
	switch (PeekChar())
	{
	case '=':
	    tt = Token::LessOrEqual;
	    NextChar();
	    break;

	case '>':
	    tt = Token::NotEqual;
	    NextChar();
	    break;
	}
	break;

    case '>':
	tt = Token::GreaterThan;
	if (PeekChar() == '=')
	{
	    tt = Token::GreaterOrEqual;
	    NextChar();
	}
	break;

    case '=':
	tt = Token::Equal;
	break;

    case ':':
	tt = Token::Colon;
	if (PeekChar() == '=')
	{
	    NextChar();
	    tt = Token::Assign;
	}
	break;

    case '[':
	tt = Token::LeftSquare;
	break;

    case ']':
	tt = Token::RightSquare;
	break;
    }
    if (tt != Token::Unknown)
    {
	NextChar();
	return Token(tt, w);
    }

    if (ch == '\'')
    {
	int len = 0;
	// TODO: Deal with quoted quotes... 
	std::string str;
	ch = NextChar();
	while (ch != '\'')
	{
	    len++;
	    str += ch;
	    ch = NextChar();
	}
	NextChar();
	if (len == 1)
	{
	    return Token(Token::Char, w, str[0]);
	}
	return Token(Token::String, w, str); 
    }

    // Identifiers start with alpha characters.
    if (std::isalpha(ch))
    {
	std::string str; 
	// Default to the "most likely". 
	str = static_cast<char>(ch);	
	while(std::isalnum(ch = NextChar()))
	{
	    str += static_cast<char>(ch);
	}
	Token::TokenType tt = Token::KeyWordToToken(str); 
	if (tt != Token::Unknown)
	{
	    return Token(tt, w);
	}
	if (types.IsTypeName(str))
	{
	    tt = Token::TypeName;
	}
	else if (types.IsEnumValue(str))
	{
	    tt = Token::EnumValue;
	    Types::EnumValue* e = types.FindEnumValue(str); 
	    assert(e && "Expected to find enum value...");
	    return Token(tt, w, e->value);
	}
	else 
	{
	    tt = Token::Identifier;
	}
	return Token(tt, w, str);
    }

    // Digit, so a number. Either "real" or "integer".
    if (std::isdigit(ch))
    {
	std::string num;
	num = static_cast<char>(ch);
	while(isdigit(ch = NextChar()))
	{
	    num += static_cast<char>(ch);
	}
	if ((ch == '.' && PeekChar() != '.') ||  ch == 'E' || ch == 'e')
	{
	    bool wasDot = (ch == '.');
	    num += ch;
	    while(isdigit(ch = NextChar()) || (wasDot && ((ch == 'E') || ch == 'e')))
	    {
		num += ch;
	    }
	    return Token(Token::Real, w, std::stod(num));
	}
	else
	{
	    return Token(Token::Integer, w, std::stoi(num));
	}
    }
    // We really shouldn't get here!
    std::cerr << "ch=" << ch << std::endl;
    assert(0);
    return Token(Token::Unknown, w);
}


Lexer::Lexer(const std::string& sourceFile, Types& ty):
    fName(sourceFile), lineNo(1),column(0), curValid(0), types(ty)
{
    inFile.open(fName);
    if (!inFile.good())
    {
	throw LexerException(std::string("File ") + fName + " could not be opened...");
    }
}
