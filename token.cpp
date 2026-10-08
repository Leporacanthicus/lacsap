#include "token.h"
#include "options.h"
#include "utils.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

Token::Token(TokenType t, const Location& w) : type(t), where(w)
{
    if (where)
    {
	ICE_IF(t == Token::Identifier || t == Token::StringLiteral || t == Token::Integer || t == Token::Real,
	       "Incorrect token type");
    }
}

Token::Token(TokenType t, const Location& w, const std::string& str) : type(t), where(w), strVal(str)
{
    ICE_IF(t != Token::Identifier && t != Token::StringLiteral, "Invalid token for string argument");
    ICE_IF(t == Token::Identifier && str.empty(), "String should not be empty for identifier");
}

Token::Token(TokenType t, const Location& w, uint64_t v) : type(t), where(w), intVal(v)
{
    ICE_IF(t != Token::Integer && t != Token::Char, "Invalid token construction");
}

Token::Token(const Location& w, double v) : type(Token::Real), where(w), realVal(v) {}

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

void Token::dump(std::ostream& out) const
{
    out << "Token { Type: " << TypeStr() << " ";
    switch (type)
    {
    case Token::Identifier:
	out << strVal << " ";
	break;

    case Token::StringLiteral:
	out << "'" << strVal << "' ";
	break;

    case Token::Integer:
	out << intVal << " ";
	break;

    case Token::Real:
	out << realVal << " ";
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
    const char*      str;
};

static std::vector<TokenEntry> tokenTable = {
    { Token::For, true, "for" },
    { Token::To, true, "to" },
    { Token::Downto, true, "downto" },
    { Token::Do, true, "do" },
    { Token::Function, true, "function" },
    { Token::Procedure, true, "procedure" },
    { Token::If, true, "if" },
    { Token::Then, true, "then" },
    { Token::Else, true, "else" },
    // clang-format off
    { Token::While, true, "while", },
    { Token::Repeat, true, "repeat", },
    { Token::Until, true, "until", },
    // clang-format on
    { Token::Begin, true, "begin" },
    { Token::End, true, "end" },
    { Token::Case, true, "case" },
    { Token::Otherwise, true, "otherwise" },
    { Token::With, true, "with" },
    { Token::Program, true, "program" },
    { Token::Unit, true, "unit" },
    { Token::Module, true, "module" },
    { Token::Write, true, "write" },
    { Token::Writeln, true, "writeln" },
    { Token::WriteStr, true, "writestr" },
    { Token::Read, true, "read" },
    { Token::Readln, true, "readln" },
    { Token::ReadStr, true, "readstr" },
    { Token::Var, true, "var" },
    { Token::Array, true, "array" },
    { Token::Of, true, "of" },
    { Token::Packed, true, "packed" },
    { Token::Record, true, "record" },
    { Token::Type, true, "type" },
    { Token::File, true, "file" },
    { Token::String, true, "string" },
    { Token::Set, true, "set" },
    { Token::Forward, true, "forward" },
    { Token::Inline, true, "inline" },
    { Token::Implementation, true, "implementation" },
    { Token::Interface, true, "interface" },
    { Token::Const, true, "const" },
    { Token::Nil, true, "nil" },
    { Token::And, true, "and" },
    { Token::And_Then, true, "and_then" },
    { Token::Or, true, "or" },
    { Token::Or_Else, true, "or_else" },
    { Token::Not, true, "not" },
    { Token::Div, true, "div" },
    { Token::Mod, true, "mod" },
    { Token::Xor, true, "xor" },
    { Token::Shr, true, "shr" },
    { Token::Shl, true, "shl" },
    { Token::SymDiff, false, "><" },
    { Token::Pow, true, "pow" },
    { Token::In, true, "in" },
    { Token::Plus, false, "+" },
    { Token::Minus, false, "-" },
    { Token::Multiply, false, "*" },
    { Token::Divide, false, "/" },
    { Token::Power, false, "**" },
    { Token::Assign, false, ":=" },
    { Token::LessThan, false, "<" },
    { Token::LessOrEqual, false, "<=" },
    { Token::GreaterOrEqual, false, ">=" },
    { Token::GreaterThan, false, ">" },
    { Token::Equal, false, "=" },
    { Token::NotEqual, false, "<>" },
    { Token::Integer, false, "integer" },
    { Token::Real, false, "real" },
    { Token::Boolean, false, "boolean" },
    { Token::Char, false, "char" },
    { Token::StringLiteral, false, "string" },
    { Token::LeftParen, false, "(" },
    { Token::RightParen, false, ")" },
    { Token::LeftSquare, false, "[" },
    { Token::RightSquare, false, "]" },
    { Token::Comma, false, "," },
    { Token::Semicolon, false, ";" },
    { Token::Colon, false, ":" },
    { Token::Period, false, "." },
    { Token::DotDot, false, ".." },
    { Token::Uparrow, false, "^" },
    { Token::Static, true, "static" },
    { Token::Virtual, true, "virtual" },
    { Token::Override, true, "override" },
    { Token::Private, true, "private" },
    { Token::Public, true, "public" },
    { Token::Protected, true, "protected" },
    { Token::Restricted, true, "restricted" },
    { Token::Constructor, true, "constructor" },
    { Token::Destructor, true, "destructor" },
    { Token::Label, true, "label" },
    { Token::Goto, true, "goto" },
    { Token::Uses, true, "uses" },
    { Token::At, false, "@" },
    { Token::Bindable, true, "bindable" },
    { Token::Default, true, "default" },
    { Token::Value, true, "value" },
    { Token::Import, true, "import" },
    { Token::LineNumber, true, "__LINE__" },
    { Token::FileName, true, "__FILE__" },
    { Token::SizeOf, true, "sizeof" },
    { Token::Identifier, false, "identifier" },
    { Token::Unknown, false, "Unknown" },
    { Token::EndOfFile, false, "EOF" },
};

static const TokenEntry* FindToken(Token::TokenType type)
{
    for (auto& i : tokenTable)
    {
	if (type == i.type)
	{
	    return &i;
	}
    }
    ICE("Could not find token!");
}

static const TokenEntry* FindToken(std::string kw)
{
    /* Don't "tolower" the keyword if it starts with __ */
    if (kw.substr(0, 2) != "__")
    {
	strlower(kw);
    }
    for (auto& i : tokenTable)
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
    const TokenEntry* t = FindToken(type);
    return t->str;
}

int Token::Precedence() const
{
    switch (type)
    {
    case Token::Not:
	return 60;

    case Token::Pow:
    case Token::Power:
	return 50;

    case Token::And:
    case Token::And_Then:
    case Token::SymDiff:
    case Token::Multiply:
    case Token::Shr:
    case Token::Shl:
    case Token::Div:
    case Token::Mod:
    case Token::Divide:
	return 40;

    case Token::Or_Else:
    case Token::Or:
    case Token::Xor:
    case Token::Plus:
    case Token::Minus:
	return 10;

    case Token::In:
    case Token::LessThan:
    case Token::LessOrEqual:
    case Token::GreaterOrEqual:
    case Token::GreaterThan:
    case Token::Equal:
    case Token::NotEqual:
	return 5;

    case Token::Assign:
	return 2;

    default:
	return -1;
    }
}

Token::TokenType Token::KeyWordToToken(const std::string& str)
{
    const TokenEntry* t = FindToken(str);
    if (t && t->isKeyWord)
    {
	return t->type;
    }
    return Token::Unknown;
}

void Token::Initialize()
{
    if (standard == none)
    {
	tokenTable.push_back({ Token::Class, true, "class" });
	tokenTable.push_back({ Token::Object, true, "object" });
    }
}
