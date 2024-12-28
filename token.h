#ifndef TOKEN_H
#define TOKEN_H

#include "location.h"
#include "utils.h"

#include <cstdint>
#include <string>

class Token
{
public:
    enum TokenType
    {
	// Values of different types
	Integer,
	Real,
	StringLiteral,
	Char,
	Boolean,

	// Type/var/const declarations:
	Type,
	Var,
	Packed,
	Array,
	Of,
	Record,
	Class,
	Identifier,
	Const,
	File,
	Set,
	String,
	Nil,
	Bindable,
	Value,

	// Symbols and such
	LeftParen,
	RightParen,
	LeftSquare,
	RightSquare,
	Plus,
	Minus,
	Multiply,
	Divide,
	Power,
	Comma,
	Semicolon,
	Colon,
	Assign,
	Period,
	DotDot,
	Uparrow,
	Div,
	Mod,
	Pow,
	Xor,
	Shr,
	Shl,
	SymDiff,

	// Comparison symbols - these return bool in binary ops.
	LessOrEqual,
	FirstComparison = LessOrEqual,
	LessThan,
	GreaterOrEqual,
	GreaterThan,
	Equal,
	NotEqual,
	In,
	LastComparison = In,

	// Keywords
	For,
	To,
	Downto,
	Do,
	If,
	Then,
	Else,
	While,
	Repeat,
	Until,
	Function,
	Procedure,
	Begin,
	End,
	Program,
	Unit,
	Module,
	Write,
	Writeln,
	WriteStr,
	Read,
	Readln,
	ReadStr,
	Builtin,
	Forward,
	Inline,
	Interface,
	Implementation,
	And,
	And_Then,
	Or,
	Or_Else,
	Not,
	Case,
	Otherwise,
	With,
	Virtual,
	Override,
	Static,
	Private,
	Public,
	Protected,
	Restricted,
	Constructor,
	Destructor,
	Label,
	Goto,
	Uses,
	At,
	Default,
	Import,

	// Specials
	LineNumber,
	FileName,
	SizeOf,

	// Errors
	Overflow,
	UntermString,

	EndOfFile = -1,
	Unknown = -1000,
    };

    Token(TokenType t = Unknown, const Location& w = { "", 0, 0 });
    Token(TokenType t, const Location& w, const std::string& str);
    Token(TokenType t, const Location& w, uint64_t v);
    Token(const Location& w, double v);

    static TokenType KeyWordToToken(const std::string& str);

    TokenType   GetToken() const { return type; }
    std::string GetIdentName() const
    {
	ICE_IF(type != Token::Identifier, "Incorrect type for identname");
	ICE_IF(strVal.empty(), "String should not be empty!");
	return strVal;
    }

    uint64_t GetIntVal() const
    {
	ICE_IF(type != Token::Integer && type != Token::Char, "Request for integer value from wrong type???");
	return intVal;
    }

    double GetRealVal() const
    {
	ICE_IF(type != Token::Real, "Request for real from wrong type???");
	return realVal;
    }

    std::string GetStrVal() const
    {
	ICE_IF(type != Token::StringLiteral, "Request for string from wrong type???");
	return strVal;
    }

    // For debug purposes.
    void        dump(std::ostream& out) const;
    void        dump() const;
    std::string ToString() const;
    std::string TypeStr() const;

    void            SetWhere(const std::string& file, int line, int col);
    std::string     Where();
    const Location& Loc() const { return where; }

    int  Precedence() const;
    bool IsCompare() const { return type >= Token::FirstComparison && type <= Token::LastComparison; }

private:
    TokenType type;
    // Store location where token started.
    Location where;

    // Values.
    std::string strVal;
    uint64_t    intVal;
    double      realVal;
};

#endif
