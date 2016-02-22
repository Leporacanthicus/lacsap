#include "lexer.h"
#include "types.h"
#include "constants.h"

#include <cassert>
#include <cctype>
#include <iostream>
#include <cstdlib>

Lexer::Lexer(Source& source) : source(source), curValid(0)
{
}

int Lexer::GetChar()
{
    return source.Get();
}

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
	return curChar = nextChar;
    }

    return curChar = GetChar();
}

int Lexer::PeekChar()
{
    if (curValid > 1)
    {
	return nextChar;
    }
    curValid++;
    return nextChar = GetChar();
}

static Token ConvertFloat(std::string &num, Location w)
{
    double v = 0;
    char *endPtr = 0;
    v = strtod(num.c_str(), &endPtr);
    if (*endPtr != 0)
    {
	return Token(Token::Overflow, w);
    }
    return Token(Token::Real, w, v);
}

static Token ConvertInt(std::string &num, Location w, int base)
{
    char *endPtr = 0;
    uint64_t v = strtoull(num.c_str(), &endPtr, base);
    if (*endPtr != 0 || (v == std::numeric_limits<unsigned long long>::max() && errno == ERANGE))
    {
	return Token(Token::Overflow, w);
    }
    return Token(Token::Integer, w, v);
}

Token Lexer::NumberToken()
{
    int ch = CurChar();
    Location w = Where();
    std::string num;
    int base = 10;

    if (ch == '$')
    {
	base = 16;
	ch = NextChar();
    }
    num = static_cast<char>(ch);

    enum State
    {
	Intpart,
	Fraction,
	Exponent,
	Done
    } state = Intpart;

    bool isFloat = false;
    while(state != Done)
    {
	switch(state)
	{
	case Intpart:
	    ch = NextChar();
	    while((base == 10 && isdigit(ch)) ||
		  (base == 16 && isxdigit(ch)))
	    {
		num += ch;
		ch = NextChar();
	    }
	    break;

	case Fraction:
	    assert(ch == '.' && "Fraction should start with '.'");
	    if (PeekChar() == '.' || PeekChar() == ')')
	    {
		break;
	    }
	    isFloat = true;
	    num += ch;
	    while(isdigit(ch = NextChar()))
	    {
		num += ch;
	    }
	    break;

	case Exponent:
	    isFloat = true;
	    assert((ch == 'e' || ch == 'E') && "Fraction should start with '.'");
	    num += ch;
	    ch = NextChar();
	    if (ch == '+' || ch == '-')
	    {
		num += ch;
		ch = NextChar();
	    }
	    while(isdigit(ch))
	    {
		num += ch;
		ch = NextChar();
	    }
	    break;

	default:
	    assert(0 && "Huh? We should not be here...");
	    break;
	}

	// If the next char is a dot or an 'e'/'E', we have a floating point number.
	if (ch == '.' && state != Fraction && base != 16)
	{
	    state = Fraction;
	}
	else if (state != Exponent && (ch == 'E' || ch == 'e'))
	{
	    state = Exponent;
	}
	else
	{
	    state = Done;
	}
    }
    if (isFloat)
    {
	return ConvertFloat(num, w);
    }
    return ConvertInt(num, w, base);
}

// String or character.
// Needs to deal with '' in the middle of string and '''' as a char constant.
Token Lexer::StringToken()
{
    std::string str;
    Location w = Where();
    int quote = CurChar();
    int ch = NextChar();
    for(;;)
    {
	if (ch == quote)
	{
	    if (PeekChar() != quote)
	    {
		break;
	    }
	    NextChar();
	}
	if (ch == '\n' || ch == EOF)
	{
	    return Token(Token::UntermString, w);
	}
	str += ch;
	ch = NextChar();
    }
    NextChar();
    if (str.size() == 1)
    {
	return Token(Token::Char, w, (uint64_t)str[0]);
    }
    return Token(Token::StringLiteral, w, str);
}

struct SingleCharToken
{
    char ch;
    Token::TokenType t;
};

static const SingleCharToken singleCharTokenTable[] =
{
    { '(', Token::LeftParen },
    { ')', Token::RightParen },
    { '+', Token::Plus },
    { '-', Token::Minus },
    { '*', Token::Multiply },
    { '/', Token::Divide },
    { ',', Token::Comma },
    { ';', Token::Semicolon },
    { '=', Token::Equal },
    { '[', Token::LeftSquare },
    { ']', Token::RightSquare },
    { '^', Token::Uparrow },
    { '@', Token::At },
};

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
	    NextChar(); /* Skip first * */
	    while ((ch = NextChar()) != EOF && !(ch == '*' && PeekChar() == ')'))
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
    case '.':
	tt = Token::Period;
	if (PeekChar() == '.')
	{
	    NextChar();
	    tt = Token::DotDot;
	}
	else if (PeekChar() == ')')
	{
	    NextChar();
	    tt = Token::RightSquare;
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

    case ':':
	tt = Token::Colon;
	if (PeekChar() == '=')
	{
	    NextChar();
	    tt = Token::Assign;
	}
	break;
    case '(':
	if (PeekChar() == '.')
	{

	    NextChar();
	    tt = Token::LeftSquare;
	}
	break;
    }
    if (tt != Token::Unknown)
    {
	NextChar();
	return Token(tt, w);
    }
    for(auto i : singleCharTokenTable)
    {
	if (i.ch == ch)
	{
	    NextChar();
	    return Token(i.t, w);
	}
    }

    if (ch == '\'' || ch == '"')
    {
	return StringToken();
    }

    // Identifiers start with alpha characters, or underscore.
    if (std::isalpha(ch) || ch == '_')
    {
	std::string str;
	// Default to the "most likely".
	str = static_cast<char>(ch);	
	// Allow alphanumeric and underscore.
	while(std::isalnum(ch = NextChar()) || ch == '_')
	{
	    str += static_cast<char>(ch);
	}
	Token::TokenType tt = Token::KeyWordToToken(str);
	if (tt != Token::Unknown)
	{
	    if (tt == Token::LineNumber)
	    {
	      return Token(Token::Integer, w, (uint64_t)w.LineNumber());
	    }
	    else if (tt == Token::FileName)
	    {
		return Token(Token::StringLiteral, w, w.FileName());
	    }
	    return Token(tt, w);
	}
	else
	{
	    tt = Token::Identifier;
	}

	return Token(tt, w, str);
    }

    // Digit, so a number. Either "real" or "integer".
    if (std::isdigit(ch) || (ch == '$' && std::isxdigit(PeekChar())))
    {
	return NumberToken();
    }
    // We really shouldn't get here!
    std::cerr << "ch=" << ch << std::endl;
    return Token(Token::Unknown, w);
}
