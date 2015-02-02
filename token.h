#ifndef TOKEN_H
#define TOKEN_H

#include <cassert>
#include <string>
#include <fstream>


class Location
{
public:
    Location(const std::string& file, int line, int column);
    std::string to_string() const;
    std::string FileName() { return fname; }
    long LineNumber() { return lineNum; }
private:
    std::string fname;
    int lineNum;
    int column;
};

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
	Write,
	Writeln,
	Read,
	Readln,
	Builtin,
	Forward,
	And,
	Or,
	Not,
	Case,
	Otherwise,
	With,

	// Specials
	LineNumber,
	FileName,

	EndOfFile  = -1, 
	Unknown    = -1000,
    };

    Token();
	
    Token(TokenType t, const Location& w);
    Token(TokenType t, const Location& w, const std::string& str);
    Token(TokenType t, const Location& w, long v);
    Token(TokenType t, const Location& w, double v);

    static TokenType KeyWordToToken(const std::string& str);

    TokenType GetToken() const { return type; }
    std::string GetIdentName() const 
    { 
	assert(type == Token::Identifier && 
	       "Incorrect type for identname");
	assert(strVal.size() != 0 && "String should not be empty!");
	return strVal; 
    }

    long GetIntVal() const 
    { 
	assert(type == Token::Integer ||
	       type == Token::Char &&
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
    const char* TypeStr() const;

    void SetWhere(const std::string& file, int line, int col);
    std::string Where();
    const Location& Loc() const { return where; }

    int Precedence() const;

private:
    TokenType   type;
    // Store location where token started.
    Location    where;
    
    // Values. 
    std::string strVal; 
    long        intVal;
    double      realVal;
};

#endif
