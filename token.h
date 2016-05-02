#ifndef TOKEN_H
#define TOKEN_H

#include "location.h"

#include <cassert>
#include <string>
#include <fstream>
#include <cstdint>


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

	// Symbols and such
	LeftParen,
	RightParen,
	LeftSquare,
	RightSquare,
	Plus,
	Minus,
	Multiply,
	Divide,
	Comma,
	Semicolon,
	Colon,
	Assign,
	Period,
	DotDot,
	Uparrow,
	Div,
	Mod,
	Xor,
	Shr,
	Shl,

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
	Write,
	Writeln,
	Read,
	Readln,
	Builtin,
	Forward,
	Inline,
	Interface,
	Implementation,
	And,
	Or,
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
	Constructor,
	Destructor,
	Label,
	Goto,
	Uses,
	At,

	// Specials
	LineNumber,
	FileName,
	SizeOf,

	// Errors
	Overflow,
	UntermString,

	EndOfFile  = -1, 
	Unknown    = -1000,
    };

    Token();
	
    Token(TokenType t, const Location& w);
    Token(TokenType t, const Location& w, const std::string& str);
    Token(TokenType t, const Location& w, uint64_t v);
    Token(TokenType t, const Location& w, double v);

    static TokenType KeyWordToToken(const std::string& str);

    TokenType GetToken() const { return type; }
    std::string GetIdentName() const 
    { 
	assert(type == Token::Identifier && "Incorrect type for identname");
	assert(strVal.size() != 0 && "String should not be empty!");
	return strVal; 
    }

    uint64_t GetIntVal() const 
    { 
	assert((type == Token::Integer || type == Token::Char) &&
	       "Request for integer value from wrong type???");
	return intVal; 
    }

    double GetRealVal() const 
    { 
	assert(type == Token::Real && "Request for real from wrong type???");
	return realVal; 
    }

    std::string GetStrVal() const 
    { 
	assert(type == Token::StringLiteral && "Request for string from wrong type???");
	return strVal; 
    }

    // For debug purposes.
    void dump(std::ostream& out, const char* file = 0, int line = 0) const;
    void dump() const;
    std::string ToString() const;
    std::string TypeStr() const;

    void SetWhere(const std::string& file, int line, int col);
    std::string Where();
    const Location& Loc() const { return where; }

    int Precedence() const;
    bool IsCompare() const { return type >= Token::FirstComparison && type <= Token::LastComparison; }

private:
    TokenType   type;
    // Store location where token started.
    Location    where;
    
    // Values. 
    std::string strVal; 
    uint64_t    intVal;
    double      realVal;
};

#endif
