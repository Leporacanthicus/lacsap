#include "constants.h"
#include "token.h"
#include <llvm/Support/Casting.h>
#include <iostream>

Token Constants::IntConstDecl::Translate() const
{
    return Token(Token::Integer, loc, value);
}

void Constants::IntConstDecl::dump() const
{
    std::cerr << "IntConstDecl: " << Value() << std::endl;
}

Token Constants::RealConstDecl::Translate() const
{
    return Token(Token::Real, loc, value);
}

void Constants::RealConstDecl::dump() const
{
    std::cerr << "RealConstDelc: " << Value() << std::endl;
}

Token Constants::CharConstDecl::Translate() const
{
    return Token(Token::Char, loc, static_cast<uint64_t>(value));
}

void Constants::CharConstDecl::dump() const
{
    std::cerr << "CharConstDecl: " << Value() << std::endl;
}

Token Constants::BoolConstDecl::Translate() const
{
    std::string s = (value)?"true":"false";
    return Token(Token::Identifier, loc, s);
}

void Constants::BoolConstDecl::dump() const
{
    std::cerr << "BoolConstDecl: " << Value() << std::endl;
}

Token Constants::StringConstDecl::Translate() const
{
    return Token(Token::StringLiteral, loc, value);
}

void Constants::StringConstDecl::dump() const
{
    std::cerr << "StringConstDecl: " << Value() << std::endl;
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
    if (lhsR || rhsR)
    {
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
    }
    return ok;
}

static bool GetAsString(std::string& lValue, std::string& rValue,
			const Constants::ConstDecl* lhs,
			const Constants::ConstDecl* rhs)
{
    const Constants::StringConstDecl* lhsS = llvm::dyn_cast<Constants::StringConstDecl>(lhs);
    const Constants::StringConstDecl* rhsS = llvm::dyn_cast<Constants::StringConstDecl>(rhs);
    const Constants::CharConstDecl*   lhsC = llvm::dyn_cast<Constants::CharConstDecl>(lhs);
    const Constants::CharConstDecl*   rhsC = llvm::dyn_cast<Constants::CharConstDecl>(rhs);
    bool ok = lhsS && rhsS;
    if (lhsS)
    {
	lValue = lhsS->Value();
    }
    if (rhsS)
    {
	rValue = rhsS->Value();
    }
    if (lhsS || rhsS)
    {
	if (lhsC)
	{
	    lValue = lhsC->Value();
	    ok = true;
	}
	if (rhsC)
	{
	    rValue = rhsC->Value();
	    ok = true;
	}
    }
    return ok;
}

Constants::ConstDecl* ErrorConst(const std::string& msg)
{
    std::cerr << "Error: " << msg << std::endl;
    return 0;
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
	if (!GetAsReal(lValue, rValue, lhsR, rhsR, lhsI, rhsI))
	{
	    return ErrorConst(errMsg);
	}
	return new Constants::RealConstDecl(Location("", 0,0), lValue + rValue);
    }

    if (lhsI && rhsI)
    {
	return new Constants::IntConstDecl(Location("", 0,0), lhsI->Value() + rhsI->Value());
    }

    std::string rValue;
    std::string lValue;
    if (GetAsString(lValue, rValue, &lhs, &rhs))
    {
	return new Constants::StringConstDecl(Location("", 0, 0), lValue + rValue);
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
	if (!GetAsReal(lValue, rValue, lhsR, rhsR, lhsI, rhsI))
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

Constants::ConstDecl* operator*(const Constants::ConstDecl& lhs, const Constants::ConstDecl& rhs)
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
	if (!GetAsReal(lValue, rValue, lhsR, rhsR, lhsI, rhsI))
	{
	    return ErrorConst(errMsg);
	}
	return new Constants::RealConstDecl(Location("", 0,0), lValue * rValue);
    }

    if (lhsI && rhsI)
    {
	return new Constants::IntConstDecl(Location("", 0,0), lhsI->Value() * rhsI->Value());
    }

    return ErrorConst(errMsg);
}
