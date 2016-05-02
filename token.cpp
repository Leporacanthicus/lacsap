#include "token.h"
#include "utils.h"
#include <cassert>
#include <sstream>
#include <iostream>
#include <algorithm>


Token::Token() : type(Token::Unknown), where("", 0, 0) {}

Token::Token(TokenType t, const Location& w): type(t), where(w)
{
    if (where)
    {
	assert(t != Token::Identifier &&
	       t != Token::StringLiteral &&
	       t != Token::Integer &&
	       t != Token::Real);
    }
}

Token::Token(TokenType t, const Location& w, const std::string& str): type(t), where(w), strVal(str)
{
    assert((t ==  Token::Identifier || Token::StringLiteral) &&
	   "Invalid token for string argument");
    assert((t == Token::StringLiteral || str != "") && "String should not be empty for identifier");
}

Token::Token(TokenType t, const Location& w, uint64_t v) : type(t), where(w), intVal(v)
{
    assert(t == Token::Integer || t == Token::Char);
}

Token::Token(TokenType t, const Location& w, double v) : type(t), where(w), realVal(v)
{
    assert(t == Token::Real);
}

std::string Token::ToString() const
{
    std::stringstream ss;
    dump(ss);

    return ss.str();
}

void Token::dump() const
{
    dump(std::cerr);
}

void Token::dump(std::ostream& out, const char* file, int line) const
{
    if (file)
    {
	out << file << ":" << line << ": ";
    }
    out << "Token { Type: " << TypeStr() << " ";
    switch(type)
    {
    case Token::Identifier:
	out << strVal << " ";
	break;

    case Token::StringLiteral:
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
    bool             isKeyWord;
    int              precedence;
    const char*      str;
};

const TokenEntry tokenTable[] =
{
    { Token::For,           true,  -1, "for" },
    { Token::To,            true,  -1, "to" },
    { Token::Downto,        true,  -1, "downto" },
    { Token::Do,            true,  -1, "do" },
    { Token::Function,      true,  -1, "function" },
    { Token::Procedure,     true,  -1, "procedure" },
    { Token::If,            true,  -1, "if" },
    { Token::Then,          true,  -1, "then" },
    { Token::Else,          true,  -1, "else" },
    { Token::While,         true,  -1, "while", },
    { Token::Repeat,        true,  -1, "repeat", },
    { Token::Until,         true,  -1, "until", },
    { Token::Begin,         true,  -1, "begin" },
    { Token::End,           true,  -1, "end" },
    { Token::Case,          true,  -1, "case" },
    { Token::Otherwise,     true,  -1, "otherwise" },
    { Token::With,          true,  -1, "with" },
    { Token::Program,       true,  -1, "program" },
    { Token::Unit,          true,  -1, "unit" },
    { Token::Write,         true,  -1, "write" },
    { Token::Writeln,       true,  -1, "writeln" },
    { Token::Read,          true,  -1, "read" },
    { Token::Readln,        true,  -1, "readln" },
    { Token::Var,           true,  -1, "var" },
    { Token::Array,         true,  -1, "array" },
    { Token::Of,            true,  -1, "of" },
    { Token::Packed,        true,  -1, "packed" },
    { Token::Record,        true,  -1, "record" },
    { Token::Class,         true,  -1, "class" },
    { Token::Class,         true,  -1, "object" },     /* Synonym! */
    { Token::Type,          true,  -1, "type" },
    { Token::File,          true,  -1, "file" },
    { Token::String,        true,  -1, "string" },
    { Token::Set,           true,  -1, "set" },
    { Token::Forward,       true,  -1, "forward" },
    { Token::Inline ,       true,  -1, "inline" },
    { Token::Implementation,true,  -1, "implementation" },
    { Token::Interface,     true,  -1, "interface" },
    { Token::Const,         true,  -1, "const" },
    { Token::Nil,           true,  -1, "nil" },
    { Token::And,           true,  40, "and" },
    { Token::Or,            true,  10, "or" },
    { Token::Not,           true,  60, "not" },
    { Token::Div,           true,  40, "div" },
    { Token::Mod,           true,  40, "mod" },
    { Token::Xor,           true,  10, "xor" },
    { Token::Shr,           true,  40, "shr" },
    { Token::Shl,           true,  40, "shl" },
    { Token::In,            true,   5, "in" },
    { Token::Plus,          false, 10, "+" },
    { Token::Minus,         false, 10, "-" },
    { Token::Multiply,      false, 40, "*" },
    { Token::Divide,        false, 40, "/" },
    { Token::Assign,        false,  2, ":=" },
    { Token::LessThan,      false,  5, "<" },
    { Token::LessOrEqual,   false,  5, "<=" },
    { Token::GreaterOrEqual,false,  5, ">=" },
    { Token::GreaterThan,   false,  5, ">" },
    { Token::Equal,         false,  5, "=" },
    { Token::NotEqual,      false,  5, "<>" },
    { Token::Integer,       false, -1, "integer" },
    { Token::Real,          false, -1, "real" },
    { Token::Boolean,       false, -1, "boolean" },
    { Token::Char,          false, -1, "char" },
    { Token::StringLiteral, false, -1, "string" },
    { Token::LeftParen,     false, -1, "(" },
    { Token::RightParen,    false, -1, ")" },
    { Token::LeftSquare,    false, -1, "[" },
    { Token::RightSquare,   false, -1, "]" },
    { Token::Comma,         false, -1, "," },
    { Token::Semicolon,     false, -1, ";" },
    { Token::Colon,         false, -1, ":" },
    { Token::Period,        false, -1, "." },
    { Token::DotDot,        false, -1, ".." },
    { Token::Uparrow,       false, -1, "^" },
    { Token::Static,        true,  -1, "static" },
    { Token::Virtual,       true,  -1, "virtual" },
    { Token::Override,      true,  -1, "override" },
    { Token::Private,       true,  -1, "private" },
    { Token::Public,        true,  -1, "public" },
    { Token::Protected,     true,  -1, "protected" },
    { Token::Constructor,   true,  -1, "constructor" },
    { Token::Destructor,    true,  -1, "destructor" },
    { Token::Label,         true,  -1, "label" },
    { Token::Goto,          true,  -1, "goto" },
    { Token::Uses,          true,  -1, "uses" },
    { Token::At,            false, -1, "@" },
    { Token::LineNumber,    true,  -1, "__LINE__" },
    { Token::FileName,      true,  -1, "__FILE__" },
    { Token::SizeOf,        true,  -1, "sizeof" },
    { Token::Identifier,    false, -1, "identifier" },
    { Token::Unknown,       false, -1, "Unknown" },
    { Token::EndOfFile,     false, -1, "EOF" },
};

static const TokenEntry* FindToken(Token::TokenType type)
{
    for(auto &i : tokenTable)
    {
	if (type == i.type)
	{
	    return &i;
	}
    }
    assert(0 && "Expect to find token!");
    return 0;
}

static const TokenEntry* FindToken(std::string kw)
{
    /* Don't "tolower" the keyword if it starts with __ */
    if (kw.substr(0,2) != "__")
    {
	strlower(kw);
    }
    for(auto &i : tokenTable)
    {
	if (kw == i.str)
	{
	    return &i;
	}
    }
    return 0;
}

std::string Token::TypeStr() const
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
    if (t && t->isKeyWord)
    {
	return t->type;
    }
    return Token::Unknown;
}
