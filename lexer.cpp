#include "lexer.h"
#include "types.h"
#include "constants.h"

#include <cassert>
#include <cctype>
#include <iostream>

Lexer::Lexer(const std::string& sourceFile) 
    : fName(sourceFile), lineNo(1),column(1), curValid(0)
{
    inFile.open(fName);
}

int Lexer::GetChar()
{
    int ch = inFile.get();
    if (ch == '\n')
    {
	lineNo++;
	column = 0;
    }
    else
    {
	column++;
    }
    return ch;
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
	curChar = nextChar;
	return curChar;
    }

    curChar = GetChar();
    return curChar;
}

int Lexer::PeekChar()
{
    if (curValid > 1)
    {
	return nextChar;
    }
    curValid++;
    nextChar = GetChar();
    return nextChar;
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
	    if (PeekChar() == '.')
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
	    ch = PeekChar();
	    if (ch == '+' || ch == '-')
	    {
		num += ch;
		NextChar();
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
    // If the next char is a dot or an 'e'/'E', we have a floating point number.
    if (isFloat)
    {
	return Token(Token::Real, w, std::stod(num));
    }
    return Token(Token::Integer, w, (long)std::stoul(num, 0, base));
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
	str += ch;
	ch = NextChar();
    }
    NextChar();
    if (str.size() == 1)
    {
	return Token(Token::Char, w, (long)str[0]);
    }
    return Token(Token::StringLiteral, w, str);
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
	
    case '^':
	tt = Token::Uparrow;
	break;
    }
    if (tt != Token::Unknown)
    {
	NextChar();
	return Token(tt, w);
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
		return Token(Token::Integer, w, w.LineNumber());
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
