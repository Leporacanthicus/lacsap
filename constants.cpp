#include "constants.h"
#include "expr.h"
#include "token.h"
#include <iostream>
#include <llvm/Support/Casting.h>

namespace Constants
{

    template<>
    Token IntConstDecl::Translate() const
    {
	return Token(Token::Integer, loc, value);
    }
    template<>
    void IntConstDecl::dump() const
    {
	std::cerr << "IntConstDecl: " << Value() << std::endl;
    }

    Token EnumConstDecl::Translate() const { return Token(Token::Integer, loc, value); }

    void EnumConstDecl::dump() const { std::cerr << "EnumConstDecl: " << Value() << std::endl; }
    template<>
    Token RealConstDecl::Translate() const
    {
	return Token(Token::Real, loc, value);
    }
    template<>
    void RealConstDecl::dump() const
    {
	std::cerr << "RealConstDelc: " << Value() << std::endl;
    }
    template<>
    Token CharConstDecl::Translate() const
    {
	return Token(Token::Char, loc, static_cast<uint64_t>(value));
    }
    template<>
    void CharConstDecl::dump() const
    {
	std::cerr << "CharConstDecl: " << Value() << std::endl;
    }
    template<>
    Token BoolConstDecl::Translate() const
    {
	std::string s = (value) ? "true" : "false";
	return Token(Token::Identifier, loc, s);
    }

    template<>
    void BoolConstDecl::dump() const
    {
	std::cerr << "BoolConstDecl: " << Value() << std::endl;
    }

    Token StringConstDecl::Translate() const { return Token(Token::StringLiteral, loc, value); }

    void StringConstDecl::dump() const { std::cerr << "StringConstDecl: " << Value() << std::endl; }

    void CompoundConstDecl::dump() const
    {
	std::cerr << "CompoundConstDecl: ";
	expr->dump();
	std::cerr << std::endl;
    }

    static bool GetAsReal(double& lValue, double& rValue, const ConstDecl& lhs, const ConstDecl& rhs)
    {

	const RealConstDecl* lhsR = llvm::dyn_cast<RealConstDecl>(&lhs);
	const RealConstDecl* rhsR = llvm::dyn_cast<RealConstDecl>(&rhs);
	const IntConstDecl*  lhsI = llvm::dyn_cast<IntConstDecl>(&lhs);
	const IntConstDecl*  rhsI = llvm::dyn_cast<IntConstDecl>(&rhs);

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

    template<typename FN>
    ConstDecl* DoRealMath(const ConstDecl& lhs, const ConstDecl& rhs, FN fn)
    {
	double rValue;
	double lValue;
	if (!GetAsReal(lValue, rValue, lhs, rhs))
	{
	    return 0;
	}
	return new RealConstDecl(Location("", 0, 0), fn(lValue, rValue));
    }

    template<typename FN>
    ConstDecl* DoIntegerMath(const ConstDecl& lhs, const ConstDecl& rhs, FN fn)
    {
	const IntConstDecl* lhsI = llvm::dyn_cast<IntConstDecl>(&lhs);
	const IntConstDecl* rhsI = llvm::dyn_cast<IntConstDecl>(&rhs);
	if (lhsI && rhsI)
	{
	    return new IntConstDecl(Location("", 0, 0), fn(lhsI->Value(), rhsI->Value()));
	}
	return 0;
    }

    ConstDecl* operator+(const ConstDecl& lhs, const ConstDecl& rhs)
    {
	ConstDecl* v = DoRealMath(rhs, lhs, [](double lv, double rv) { return lv + rv; });
	if (!v)
	{
	    v = DoIntegerMath(rhs, lhs, [](uint64_t lv, uint64_t rv) { return lv + rv; });
	}
	if (!v)
	{
	    std::string rValue;
	    std::string lValue;
	    if (GetAsString(lValue, rValue, &lhs, &rhs))
	    {
		return new StringConstDecl(Location("", 0, 0), lValue + rValue);
	    }
	    return ErrorConst("Invalid operand for +");
	}
	return v;
    }

    ConstDecl* operator-(const ConstDecl& lhs, const ConstDecl& rhs)
    {
	ConstDecl* v = DoRealMath(lhs, rhs, [](double lv, double rv) { return lv - rv; });
	if (!v)
	{
	    v = DoIntegerMath(lhs, rhs, [](uint64_t lv, uint64_t rv) { return lv - rv; });
	}
	if (!v)
	{
	    return ErrorConst("Invalid operand for -");
	}
	return v;
    }

    ConstDecl* operator*(const ConstDecl& lhs, const ConstDecl& rhs)
    {
	ConstDecl* v = DoRealMath(lhs, rhs, [](double lv, double rv) { return lv * rv; });
	if (!v)
	{
	    v = DoIntegerMath(lhs, rhs, [](uint64_t lv, uint64_t rv) { return lv * rv; });
	}
	if (!v)
	{
	    return ErrorConst("Invalid operand for *");
	}
	return v;
    }

    ConstDecl* operator/(const ConstDecl& lhs, const ConstDecl& rhs)
    {
	ConstDecl* v = DoRealMath(lhs, rhs, [](double lv, double rv) { return lv / rv; });
	if (!v)
	{
	    v = DoIntegerMath(lhs, rhs, [](uint64_t lv, uint64_t rv) { return lv / rv; });
	}
	if (!v)
	{
	    return ErrorConst("Invalid operand for /");
	}
	return v;
    }

    llvm::Constant* ConstDeclToLLVMConst(const ConstDecl* cd)
    {
	if (auto i = llvm::dyn_cast<IntConstDecl>(cd))
	{
	    return llvm::ConstantInt::get(i->Type()->LlvmType(), i->Value());
	}
	if (auto e = llvm::dyn_cast<EnumConstDecl>(cd))
	{
	    return llvm::ConstantInt::get(e->Type()->LlvmType(), e->Value());
	}
	if (auto r = llvm::dyn_cast<RealConstDecl>(cd))
	{
	    return llvm::ConstantFP::get(r->Type()->LlvmType(), r->Value());
	}
	assert(0 && "Unsupported constant conversion");
	return 0;
    }

} // namespace Constants
