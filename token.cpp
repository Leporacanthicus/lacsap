#include "token.h"
#include <cassert>
#include <sstream>
#include <iostream>
#include <algorithm>


Location::Location(const std::string& file, int line, int col)
{
    fname = file; 
    lineNo = line;
    column = col;
}

std::string Location::to_string() const
{ 
    return fname+ ":" + std::to_string(lineNo) + ":" + std::to_string(column); 
}


Token::Token() : type(Token::Unknown), where("", 0, 0) {}

Token::Token(TokenType t, const Location& w): type(t), where(w)
{
    assert(t != Token::Identifier && 
	   t != Token::TypeName && 
	   t != Token::String &&
	   t != Token::Integer && 
	   t != Token::Real);
}

Token::Token(TokenType t, const Location& w, const std::string& str): type(t), where(w), strVal(str)
{
    assert(t ==  Token::Identifier || t == Token::TypeName || Token::String &&
	   "Invalid token for string argument");
    assert((t != Token::String || str != "") && "String should not be empty for identifier or typename");
}

Token::Token(TokenType t, const Location& w, int v) : type(t), where(w), intVal(v)
{
    assert(t == Token::Integer);
}


Token::Token(TokenType t, const Location& w, double v) : type(t), where(w), realVal(v)
{
    assert(t == Token::Real);
}

std::string Token::ToString() const
{
    std::stringstream ss;
    Dump(ss);
    
    return ss.str();
}

void Token::Dump(std::ostream& out, const char *file, int line) const
{
    if (file)
    {
	out << file << ":" << line << ": "; 
    }
    out << "Token { Type: " << TypeStr() << " ";
    switch(type)
    {
    case Token::Identifier:
    case Token::TypeName:
	out << strVal << " ";
	break;

    case Token::String:
	out << "'" << strVal << "' ";
	break;

    case Token::Integer:
	out  << intVal << " ";
	break;

    case Token::Real:
	out  << realVal << " ";
	break;

    case Token::Boolean:
	out << std::boolalpha << (bool)intVal << " ";
	break;
    default:
	break;
    }

    out << "} @ " << where.to_string() << std::endl;
}

struct TokenEntry
{
    Token::TokenType type;
    int              precedence;
    const char *     str;
};

const TokenEntry tokenTable[] =
{
    { Token::Unknown,       -1, "&Unknown" },
    { Token::Unused,        -1, "&Unused" },
    { Token::Identifier,    -1, "identifier" },
    { Token::Integer,       -1, "integer:" },
    { Token::Real,          -1, "real" },
    { Token::Boolean,       -1, "boolean" },
    { Token::String,        -1, "string" },
    { Token::Char,          -1, "char" },
    { Token::LeftParen,     -1, "(" },
    { Token::RightParen,    -1, ")" },
    { Token::Plus,          10, "+" },
    { Token::Minus,         10, "-" },
    { Token::Multiply,      40, "*" },
    { Token::Divide,        40, "/" },
    { Token::Assign,         2, ":=" },
    { Token::LessThan,       5, "<" },
    { Token::LessOrEqual,    5, "<=" },
    { Token::GreaterOrEqual, 5, ">=" },
    { Token::GreaterThan,    5, ">" },
    { Token::Equal,          5, "=" },
    { Token::NotEqual,       5, "<>" },
    { Token::Comma,         -1, "," },
    { Token::Semicolon,     -1, ";" },
    { Token::Colon,         -1, ":" },
    { Token::Period,        -1, "." },
    { Token::TypeName,      -1, "type" },
    { Token::For,           -2, "for" },
    { Token::To,            -2, "to" },
    { Token::Downto,        -2, "downto" },
    { Token::Do,            -2, "do" },
    { Token::Function,      -2, "function" },
    { Token::Procedure,     -2, "procedure" },
    { Token::If,            -2, "if" },
    { Token::Then,          -2, "then" },
    { Token::Else,          -2, "else" },
    { Token::Begin,         -2, "begin" },
    { Token::End,           -2, "end" },
    { Token::Program,       -2, "program" },
    { Token::Write,         -2, "write" },
    { Token::Writeln,       -2, "writeln" }, 
    { Token::Read,          -2, "read" },
    { Token::Readln,        -2, "readln" }, 
    { Token::Var,           -2, "var" },
    { Token::EndOfFile,     -1, "EOF" },
};

static const TokenEntry* FindToken(Token::TokenType type)
{
    for(size_t i = 0; i < sizeof(tokenTable)/sizeof(tokenTable[0]); i++)
    {
	if (type == tokenTable[i].type)
	{
	    return &tokenTable[i];
	}
    }
    assert(0 && "Expect to find token!");
    return NULL;
}

static const TokenEntry* FindToken(const std::string& kw)
{
    std::string kwlower = kw;
    std::transform(kwlower.begin(), kwlower.end(), kwlower.begin(), ::tolower);
    for(size_t i = 0; i < sizeof(tokenTable)/sizeof(tokenTable[0]); i++)
    {
	if (kwlower == tokenTable[i].str)
	{
	    return &tokenTable[i];
	}
    }
    return NULL;
}

const char *Token::TypeStr() const
{
    const TokenEntry* t  = FindToken(type);
    return t->str;
}

int Token::Precedence() const
{
    const TokenEntry* t  = FindToken(type);
    return t->precedence;
}


Token::TokenType Token::KeyWordToToken(const std::string &str) 
{
    const TokenEntry* t = FindToken(str);
    if (t && t->precedence == -2)
    {
	return t->type;
    }
    return Token::Unknown;
}
