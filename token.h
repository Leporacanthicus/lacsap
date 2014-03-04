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
	Unused     = 0,
	Identifier,
	Integer,
	Real,
	String,
	Char,
	Boolean,
	LeftParen,
	RightParen,
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
	TypeName,
	For,
	To,
	Downto,
	Do, 
	If,
	Then,
	Else,
	While,
	Function,
	Procedure,
	Begin,
	End,
	Program,
	Write,
	Writeln,
	Read,
	Readln,
	Var,
	Builtin,
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
	assert(type == Token::Identifier || type == Token::TypeName);
	return strVal; 
    }
    int GetIntVal() const { return intVal; }
    double GetRealVal() const { return realVal; }
    std::string GetStrVal() const { return strVal; }
    // For debug purposes.
    void Dump(std::ostream& out, const char *file = NULL, int line = 0) const;
    std::string ToString() const;
    const char *TypeStr() const;

    void SetWhere(const std::string& file, int line, int col);
    std::string Where();

    int Precedence() const;

private:

private:
    TokenType   type;
    // Store location where token started.
    Location where;
    
    // Values. 
    std::string strVal; 
    int intVal;
    double realVal;

};

#endif
