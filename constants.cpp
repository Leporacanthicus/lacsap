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

static bool GetAsReal(double& lValue, double& rValue, 
		 const Constants::RealConstDecl* lhsR,
		 const Constants::RealConstDecl* rhsR,
		 const Constants::IntConstDecl* lhsI,
		 const Constants::IntConstDecl* rhsI)
{
    bool ok = rhsR && lhsR;
    if (lhsR)
    {
	lValue = lhsR->Value();
    }
    if (rhsR)
    {
	rValue = rhsR->Value();
    }
    if (rhsI)
    {
	rValue = rhsI->Value();
	ok = true;
    }
    if (lhsI)
    {
	lValue = lhsI->Value();
	ok = true;
    }
    return ok;
}


Constants::ConstDecl* operator+(const Constants::ConstDecl& lhs, const Constants::ConstDecl& rhs)
{
    const std::string errMsg = "Invalid operand for +";
    const Constants::RealConstDecl* lhsR = llvm::dyn_cast<Constants::RealConstDecl>(&lhs);
    const Constants::RealConstDecl* rhsR = llvm::dyn_cast<Constants::RealConstDecl>(&rhs);
    const Constants::IntConstDecl* lhsI = llvm::dyn_cast<Constants::IntConstDecl>(&lhs);
    const Constants::IntConstDecl* rhsI = llvm::dyn_cast<Constants::IntConstDecl>(&rhs);

    if (lhsR || rhsR)
    {
	double rValue;
	double lValue;
	if (!GetAsReal(rValue, lValue, lhsR, rhsR, lhsI, rhsI))
	{
	    return ErrorConst(errMsg);
	}
	return new Constants::RealConstDecl(Location("", 0,0), lValue + rValue);
    }

    if (lhsI && rhsI)
    {
	return new Constants::IntConstDecl(Location("", 0,0), lhsI->Value() + rhsI->Value());
    }

    const Constants::StringConstDecl* lhsS = llvm::dyn_cast<Constants::StringConstDecl>(&lhs);
    const Constants::StringConstDecl* rhsS = llvm::dyn_cast<Constants::StringConstDecl>(&rhs);

    if (lhsS && rhsS)
    {
	return new Constants::StringConstDecl(Location("", 0, 0), lhsS->Value() + rhsS->Value());
    }
	
    return ErrorConst("Invalid operand for +");
}


Constants::ConstDecl* operator-(const Constants::ConstDecl& lhs, const Constants::ConstDecl& rhs)
{
    const std::string errMsg = "Invalid operand for -";
    const Constants::RealConstDecl* lhsR = llvm::dyn_cast<Constants::RealConstDecl>(&lhs);
    const Constants::RealConstDecl* rhsR = llvm::dyn_cast<Constants::RealConstDecl>(&rhs);
    const Constants::IntConstDecl* lhsI = llvm::dyn_cast<Constants::IntConstDecl>(&lhs);
    const Constants::IntConstDecl* rhsI = llvm::dyn_cast<Constants::IntConstDecl>(&rhs);
    if (lhsR || rhsR)
    {
	double rValue;
	double lValue;
	if (!GetAsReal(rValue, lValue, lhsR, rhsR, lhsI, rhsI))
	{
	    return ErrorConst(errMsg);
	}
	return new Constants::RealConstDecl(Location("", 0,0), lValue - rValue);
    }

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
