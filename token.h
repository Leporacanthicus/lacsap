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
private:
    std::string fname;
    int lineNo;
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
	String,
	Char,
	Boolean,
	True,
	False,

	// Type/var/const declarations:
	Type,
	Var,
	Packed,
	Array,
	Of,
	Record,
	Identifier,
	TypeName,
	Const,
	ConstName,
	EnumValue,

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
	LessOrEqual,
	LessThan,
	GreaterOrEqual,
	GreaterThan,
	Equal,
	NotEqual,
	Period,
	DotDot,
	Uparrow,

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

	EndOfFile  = -1, 
	Unknown    = -1000,
    };

    Token();
	
    Token(TokenType t, const Location& w);
    Token(TokenType t, const Location& w, const std::string& str);
    Token(TokenType t, const Location& w, int v);
    Token(TokenType t, const Location& w, double v);

    static TokenType KeyWordToToken(const std::string& str);

    TokenType GetType() const { return type; }
    std::string GetIdentName() const 
    { 
	assert(type == Token::Identifier || 
	       type == Token::TypeName || 
	       type == Token::ConstName &&
	       "Incorrect type for identname");
	assert(strVal.size() != 0 && "String should not be empty!");
	return strVal; 
    }
    int GetIntVal() const 
    { 
	assert(type == Token::EnumValue || 
	       type == Token::Integer ||
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
	assert(type == Token::String && "Request for string from wrong type???");
	return strVal; 
    }
    // For debug purposes.
    void Dump(std::ostream& out, const char* file = 0, int line = 0) const;
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
    int         intVal;
    double      realVal;
};

#endif
