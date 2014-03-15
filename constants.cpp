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

bool Constants::IsConstant(const std::string& name)
{
    return !!constants.Find(name);
}

bool Constants::Add(const std::string& nm, ConstDecl* c)
{
    return constants.Add(nm, c);
}

Constants::ConstDecl* Constants::GetConstDecl(const std::string& name)
{
    Constants::ConstDecl* d = constants.Find(name);
    return d;
}

Constants::Constants()
{
    constants.NewLevel();
};
