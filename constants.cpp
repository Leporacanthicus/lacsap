#include "constants.h"
#include "token.h"
#include <llvm/Support/Casting.h>
#include <iostream>

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

Constants::ConstDecl* operator+(const Constants::ConstDecl& lhs, const Constants::ConstDecl& rhs)
{
    const std::string errMsg = "Invalid operand for +";
    const Constants::RealConstDecl* lhsR = llvm::dyn_cast<Constants::RealConstDecl>(&lhs);
    const Constants::RealConstDecl* rhsR = llvm::dyn_cast<Constants::RealConstDecl>(&rhs);

    if (lhsR)
    {
	if (llvm::isa<Constants::IntConstDecl>(&rhs))
	{
	    rhsR = new Constants::RealConstDecl(Location("", 0,0), 
						llvm::dyn_cast<Constants::IntConstDecl>(&rhs)->Value());
	}
    }

    if (rhsR)
    {
	if (llvm::isa<Constants::IntConstDecl>(&lhs))
	{
	    lhsR = new Constants::RealConstDecl(Location("", 0,0), 
						llvm::dyn_cast<Constants::IntConstDecl>(&lhs)->Value());
	}
    }

    if (rhsR && lhsR)
    {
	return new Constants::RealConstDecl(Location("", 0,0), lhsR->Value() + rhsR->Value());
    }

    const Constants::IntConstDecl* lhsI = llvm::dyn_cast<Constants::IntConstDecl>(&lhs);
    const Constants::IntConstDecl* rhsI = llvm::dyn_cast<Constants::IntConstDecl>(&rhs);
    if (lhsI && rhsI)
    {
	return new Constants::IntConstDecl(Location("", 0,0), lhsI->Value() + rhsI->Value());
    }
	
    return ErrorConst("Invalid operand for +");
}


Constants::ConstDecl* operator-(const Constants::ConstDecl& lhs, const Constants::ConstDecl& rhs)
{
    const std::string errMsg = "Invalid operand for -";
    const Constants::RealConstDecl* lhsR = llvm::dyn_cast<Constants::RealConstDecl>(&lhs);
    const Constants::RealConstDecl* rhsR = llvm::dyn_cast<Constants::RealConstDecl>(&rhs);
    if (lhsR)
    {
	if (llvm::isa<Constants::IntConstDecl>(&rhs))
	{
	    rhsR = new Constants::RealConstDecl(Location("", 0,0), 
						llvm::dyn_cast<Constants::IntConstDecl>(&rhs)->Value());
	}
    }

    if (rhsR)
    {
	if (llvm::isa<Constants::IntConstDecl>(&lhs))
	{
	    lhsR = new Constants::RealConstDecl(Location("", 0,0), 
						llvm::dyn_cast<Constants::IntConstDecl>(&lhs)->Value());
	}
    }
    if (rhsR && lhsR)
    {
	return new Constants::RealConstDecl(Location("", 0,0), lhsR->Value() - rhsR->Value());
    }

    const Constants::IntConstDecl* lhsI = llvm::dyn_cast<Constants::IntConstDecl>(&lhs);
    const Constants::IntConstDecl* rhsI = llvm::dyn_cast<Constants::IntConstDecl>(&rhs);
    if (lhsI && rhsI)
    {
	return new Constants::IntConstDecl(Location("", 0,0), lhsI->Value() - rhsI->Value());
    }
	
    return ErrorConst(errMsg);
}

Constants::ConstDecl* ErrorConst(const std::string& msg)
{
    std::cerr << "Error: " << msg << std::endl; 
    return 0;
}
