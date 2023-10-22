#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "token.h"
#include "types.h"

namespace llvm
{
    class Constant;
};

namespace Constants
{
    enum ConstKind
    {
	CK_ConstDecl,
	CK_IntConstDecl,
	CK_EnumConstDecl,
	CK_RealConstDecl,
	CK_CharConstDecl,
	CK_BoolConstDecl,
	CK_StringConstDecl,
	CK_CompoundConstDecl,
    };

    class ConstDecl
    {
    public:
	ConstDecl(Types::TypeDecl* t, ConstKind k, const Location& w) : type(t), kind(k), loc(w) {}
	virtual ~ConstDecl() {}
	virtual Token    Translate() const = 0;
	ConstKind        getKind() const { return kind; }
	virtual void     dump() const = 0;
	const Location&  Loc() const { return loc; }
	Types::TypeDecl* Type() const { return type; }

    protected:
	Types::TypeDecl* type;
	const ConstKind  kind;
	Location         loc;
    };

    template<typename T, typename TD, ConstKind ck>
    class ConstDeclBase : public ConstDecl
    {
    public:
	ConstDeclBase(const Location& w, T v) : ConstDecl(Types::Get<TD>(), ck, w), value(v) {}
	Token       Translate() const override;
	static bool classof(const ConstDecl* e) { return e->getKind() == ck; }
	T           Value() const { return value; }
	void        dump() const override;

    protected:
	T value;
    };

    using IntConstDecl = ConstDeclBase<uint64_t, Types::IntegerDecl, CK_IntConstDecl>;
    using RealConstDecl = ConstDeclBase<double, Types::RealDecl, CK_RealConstDecl>;
    using CharConstDecl = ConstDeclBase<char, Types::CharDecl, CK_CharConstDecl>;
    using BoolConstDecl = ConstDeclBase<char, Types::BoolDecl, CK_BoolConstDecl>;

    class EnumConstDecl : public ConstDecl
    {
    public:
	EnumConstDecl(Types::TypeDecl* t, const Location& w, uint64_t v)
	    : ConstDecl(t, CK_EnumConstDecl, w), value(v)
	{
	}
	Token       Translate() const override;
	uint64_t    Value() const { return value; }
	static bool classof(const ConstDecl* e) { return e->getKind() == CK_EnumConstDecl; }
	void        dump() const override;

    private:
	uint64_t value;
    };

    class StringConstDecl : public ConstDecl
    {
    public:
	StringConstDecl(const Location& w, const std::string& v)
	    : ConstDecl(Types::Get<Types::StringDecl>(255), CK_StringConstDecl, w), value(v)
	{
	}
	Token              Translate() const override;
	const std::string& Value() const { return value; }
	static bool        classof(const ConstDecl* e) { return e->getKind() == CK_StringConstDecl; }
	void               dump() const override;

    private:
	std::string value;
    };

    class CompoundConstDecl : public ConstDecl
    {
    public:
	CompoundConstDecl(const Location& w, Types::TypeDecl* ty, ExprAST* e)
	    : ConstDecl(ty, CK_CompoundConstDecl, w), expr(e)
	{
	}
	Token Translate() const override
	{
	    assert(0 && "Should not call this");
	    return Token(Token::Unknown, loc);
	}
	ExprAST*    Value() const { return expr; }
	static bool classof(const ConstDecl* e) { return e->getKind() == CK_CompoundConstDecl; }
	void        dump() const override;

    private:
	ExprAST* expr;
    };

    ConstDecl* ErrorConst(const std::string& msg);
    ConstDecl* operator+(const ConstDecl& lhs, const ConstDecl& rhs);
    ConstDecl* operator-(const ConstDecl& lhs, const ConstDecl& rhs);
    ConstDecl* operator*(const ConstDecl& lhs, const ConstDecl& rhs);
    ConstDecl* operator/(const ConstDecl& lhs, const ConstDecl& rhs);

}; // namespace Constants

#endif
