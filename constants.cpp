#include "constants.h"

Token Constants::IntConstDecl::Translate()
{
    return Token(Token::Integer, loc, value);
}

Token Constants::RealConstDecl::Translate()
{
    return Token(Token::Real, loc, value);
}

Token Constants::CharConstDecl::Translate()
{
    return Token(Token::Char, loc, (long)value);
}

Token Constants::BoolConstDecl::Translate()
{
    std::string s = (value)?"true":"false";
    return Token(Token::Identifier, loc, s);
}

Token Constants::StringConstDecl::Translate()
{
    return Token(Token::StringLiteral, loc, value);
}
