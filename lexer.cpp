#include "lexer.h"
#include "constants.h"
#include "types.h"

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <iostream>

Lexer::Lexer(Source& source) : source(source), curValid(0) {}

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

static Token ConvertFloat(const std::string& num, const Location& w)
{
    double v = 0;
    char*  endPtr = 0;
    v = strtod(num.c_str(), &endPtr);
    if (*endPtr != 0)
    {
	return Token(Token::Overflow, w);
    }
    return Token(Token::Real, w, v);
}

template<typename T, typename FN>
static T SafeConvert(const std::string& num, bool& err, FN fn, int base)
{
    err = false;
    char* endPtr = 0;
    T     v = fn(num.c_str(), &endPtr, base);
    if (*endPtr != 0 || (v == std::numeric_limits<T>::max() && errno == ERANGE))
    {
	err = true;
    }
    return v;
}

static Token ConvertInt(const std::string& num, const Location& w, int base)
{
    bool     err;
    uint64_t v = SafeConvert<uint64_t>(num, err, strtoull, base);
    if (err)
    {
	return Token(Token::Overflow, w);
    }
    return Token(Token::Integer, w, v);
}

bool ValidForBase(char c, int base)
{
    if (base == 10)
	return isdigit(c);
    if (base == 16)
	return isxdigit(c);

    if (base < 10 && (c >= '0' && c < '0' + base))
    {
	return true;
    }

    c = std::tolower(static_cast<unsigned char>(c));
    // We have base > 10, as base = 10 has already been done.
    return (isdigit(c) || (c >= 'a' && c < 'a' + (base - 10)));
}

Token Lexer::NumberToken()
{
    int         ch = CurChar();
    const Location& w = Where();
    std::string num;
    int         base = 10;

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
    while (state != Done)
    {
	switch (state)
	{
	case Intpart:
	    ch = NextChar();
	    while (ValidForBase(ch, base) || ch == '#')
	    {
		if (ch == '#' && base == 10)
		{
		    ch = NextChar();
		    bool err;
		    base = SafeConvert<int>(num, err, strtol, 10);
		    if (err || base < 2 || base > 36)
		    {
			return Token(Token::Unknown, w);
		    }
		    num = "";
		}
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
	    while (isdigit(ch = NextChar()))
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
	    while (isdigit(ch))
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
    const Location& w = Where();
    int         quote = CurChar();
    int         ch = NextChar();
    for (;;)
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
    char             ch;
    Token::TokenType t;
};

static const SingleCharToken singleCharTokenTable[] = {
    { '(', Token::LeftParen },  { ')', Token::RightParen },  { '+', Token::Plus },
    { '-', Token::Minus },      { '*', Token::Multiply },    { '/', Token::Divide },
    { ',', Token::Comma },      { ';', Token::Semicolon },   { '=', Token::Equal },
    { '[', Token::LeftSquare }, { ']', Token::RightSquare }, { '^', Token::Uparrow },
    { '@', Token::At },
};

Token Lexer::GetToken()
{
    int      ch = CurChar();
    const Location& w = Where();

    do
    {
	while (isspace(ch))
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
    } while (isspace(ch));

    // EOF -> return now...
    if (ch == EOF)
    {
	return Token(Token::EndOfFile, w);
    }

    Token::TokenType tt = Token::Unknown;
    switch (ch)
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
	else if (PeekChar() == '<')
	{
	    tt = Token::SymDiff;
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
    case '*':
	if (PeekChar() == '*')
	{
	    NextChar();
	    tt = Token::Power;
	}
	break;
    }
    if (tt != Token::Unknown)
    {
	NextChar();
	return Token(tt, w);
    }
    for (auto i : singleCharTokenTable)
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
	while (std::isalnum(ch = NextChar()) || ch == '_')
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
