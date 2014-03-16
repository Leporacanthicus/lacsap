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
    return Token(Token::Char, loc, value);
}

Token Constants::BoolConstDecl::Translate()
{
    return Token(Token::Boolean, loc, value);
}

Token Constants::StringConstDecl::Translate()
{
    return Token(Token::String, loc, value);
}
