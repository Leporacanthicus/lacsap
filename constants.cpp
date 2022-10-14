#include "constants.h"
#include "token.h"
#include <iostream>
#include <llvm/Support/Casting.h>

namespace Constants
{

    Token IntConstDecl::Translate() const { return Token(Token::Integer, loc, value); }

    void IntConstDecl::dump() const { std::cerr << "IntConstDecl: " << Value() << std::endl; }

    Token EnumConstDecl::Translate() const { return Token(Token::Integer, loc, value); }

    void EnumConstDecl::dump() const { std::cerr << "EnumConstDecl: " << Value() << std::endl; }

    Token RealConstDecl::Translate() const { return Token(Token::Real, loc, value); }

    void RealConstDecl::dump() const { std::cerr << "RealConstDelc: " << Value() << std::endl; }

    Token CharConstDecl::Translate() const { return Token(Token::Char, loc, static_cast<uint64_t>(value)); }

    void CharConstDecl::dump() const { std::cerr << "CharConstDecl: " << Value() << std::endl; }

    Token BoolConstDecl::Translate() const
    {
	std::string s = (value) ? "true" : "false";
	return Token(Token::Identifier, loc, s);
    }

    void BoolConstDecl::dump() const { std::cerr << "BoolConstDecl: " << Value() << std::endl; }

    Token StringConstDecl::Translate() const { return Token(Token::StringLiteral, loc, value); }

    void StringConstDecl::dump() const { std::cerr << "StringConstDecl: " << Value() << std::endl; }

    static bool GetAsReal(double& lValue, double& rValue, const RealConstDecl* lhsR,
                          const RealConstDecl* rhsR, const IntConstDecl* lhsI, const IntConstDecl* rhsI)
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

    static bool GetAsString(std::string& lValue, std::string& rValue, const ConstDecl* lhs,
                            const ConstDecl* rhs)
    {
	const StringConstDecl* lhsS = llvm::dyn_cast<StringConstDecl>(lhs);
	const StringConstDecl* rhsS = llvm::dyn_cast<StringConstDecl>(rhs);
	const CharConstDecl*   lhsC = llvm::dyn_cast<CharConstDecl>(lhs);
	const CharConstDecl*   rhsC = llvm::dyn_cast<CharConstDecl>(rhs);
	bool                   ok = lhsS && rhsS;
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

    ConstDecl* ErrorConst(const std::string& msg)
    {
	std::cerr << "Error: " << msg << std::endl;
	return 0;
    }

    ConstDecl* operator+(const ConstDecl& lhs, const ConstDecl& rhs)
    {
	const std::string    errMsg = "Invalid operand for +";
	const RealConstDecl* lhsR = llvm::dyn_cast<RealConstDecl>(&lhs);
	const RealConstDecl* rhsR = llvm::dyn_cast<RealConstDecl>(&rhs);
	const IntConstDecl*  lhsI = llvm::dyn_cast<IntConstDecl>(&lhs);
	const IntConstDecl*  rhsI = llvm::dyn_cast<IntConstDecl>(&rhs);

	if (lhsR || rhsR)
	{
	    double rValue;
	    double lValue;
	    if (!GetAsReal(lValue, rValue, lhsR, rhsR, lhsI, rhsI))
	    {
		return ErrorConst(errMsg);
	    }
	    return new RealConstDecl(Location("", 0, 0), lValue + rValue);
	}

	if (lhsI && rhsI)
	{
	    return new IntConstDecl(Location("", 0, 0), lhsI->Value() + rhsI->Value());
	}

	std::string rValue;
	std::string lValue;
	if (GetAsString(lValue, rValue, &lhs, &rhs))
	{
	    return new StringConstDecl(Location("", 0, 0), lValue + rValue);
	}

	return ErrorConst("Invalid operand for +");
    }

    ConstDecl* operator-(const ConstDecl& lhs, const ConstDecl& rhs)
    {
	const std::string    errMsg = "Invalid operand for -";
	const RealConstDecl* lhsR = llvm::dyn_cast<RealConstDecl>(&lhs);
	const RealConstDecl* rhsR = llvm::dyn_cast<RealConstDecl>(&rhs);
	const IntConstDecl*  lhsI = llvm::dyn_cast<IntConstDecl>(&lhs);
	const IntConstDecl*  rhsI = llvm::dyn_cast<IntConstDecl>(&rhs);
	if (lhsR || rhsR)
	{
	    double rValue;
	    double lValue;
	    if (!GetAsReal(lValue, rValue, lhsR, rhsR, lhsI, rhsI))
	    {
		return ErrorConst(errMsg);
	    }
	    return new RealConstDecl(Location("", 0, 0), lValue - rValue);
	}

	if (lhsI && rhsI)
	{
	    return new IntConstDecl(Location("", 0, 0), lhsI->Value() - rhsI->Value());
	}

	return ErrorConst(errMsg);
    }

    ConstDecl* operator*(const ConstDecl& lhs, const ConstDecl& rhs)
    {
	const std::string    errMsg = "Invalid operand for *";
	const RealConstDecl* lhsR = llvm::dyn_cast<RealConstDecl>(&lhs);
	const RealConstDecl* rhsR = llvm::dyn_cast<RealConstDecl>(&rhs);
	const IntConstDecl*  lhsI = llvm::dyn_cast<IntConstDecl>(&lhs);
	const IntConstDecl*  rhsI = llvm::dyn_cast<IntConstDecl>(&rhs);
	if (lhsR || rhsR)
	{
	    double rValue;
	    double lValue;
	    if (!GetAsReal(lValue, rValue, lhsR, rhsR, lhsI, rhsI))
	    {
		return ErrorConst(errMsg);
	    }
	    return new RealConstDecl(Location("", 0, 0), lValue * rValue);
	}

	if (lhsI && rhsI)
	{
	    return new IntConstDecl(Location("", 0, 0), lhsI->Value() * rhsI->Value());
	}

	return ErrorConst(errMsg);
    }

    ConstDecl* operator/(const ConstDecl& lhs, const ConstDecl& rhs)
    {
	const std::string    errMsg = "Invalid operand for /";
	const RealConstDecl* lhsR = llvm::dyn_cast<RealConstDecl>(&lhs);
	const RealConstDecl* rhsR = llvm::dyn_cast<RealConstDecl>(&rhs);
	const IntConstDecl*  lhsI = llvm::dyn_cast<IntConstDecl>(&lhs);
	const IntConstDecl*  rhsI = llvm::dyn_cast<IntConstDecl>(&rhs);
	if (lhsR || rhsR)
	{
	    double rValue;
	    double lValue;
	    if (!GetAsReal(lValue, rValue, lhsR, rhsR, lhsI, rhsI))
	    {
		return ErrorConst(errMsg);
	    }
	    return new RealConstDecl(Location("", 0, 0), lValue / rValue);
	}

	if (lhsI && rhsI)
	{
	    return new IntConstDecl(Location("", 0, 0), lhsI->Value() / rhsI->Value());
	}

	return ErrorConst(errMsg);
    }

} // namespace Constants
