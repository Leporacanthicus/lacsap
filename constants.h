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
    };

    class ConstDecl
    {
    public:
	ConstDecl(Types::TypeDecl* t, ConstKind k, const Location& w) : type(t), kind(k), loc(w) {}
	virtual ~ConstDecl() {}
	virtual Token    Translate() const = 0;
	ConstKind        getKind() const { return kind; }
	virtual void     dump() const = 0;
	Location         Loc() const { return loc; }
	Types::TypeDecl* Type() const { return type; }

    protected:
	Types::TypeDecl* type;
	const ConstKind  kind;
	Location         loc;
    };

    class IntConstDecl : public ConstDecl
    {
    public:
	IntConstDecl(const Location& w, uint64_t v)
	    : ConstDecl(Types::GetIntegerType(), CK_IntConstDecl, w), value(v)
	{
	}
	Token       Translate() const override;
	uint64_t    Value() const { return value; }
	static bool classof(const ConstDecl* e) { return e->getKind() == CK_IntConstDecl; }
	void        dump() const override;

    private:
	uint64_t value;
    };

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

    class RealConstDecl : public ConstDecl
    {
    public:
	RealConstDecl(const Location& w, double v)
	    : ConstDecl(Types::GetRealType(), CK_RealConstDecl, w), value(v)
	{
	}
	Token       Translate() const override;
	double      Value() const { return value; }
	static bool classof(const ConstDecl* e) { return e->getKind() == CK_RealConstDecl; }
	void        dump() const override;

    private:
	double value;
    };

    class CharConstDecl : public ConstDecl
    {
    public:
	CharConstDecl(const Location& w, char v)
	    : ConstDecl(Types::GetCharType(), CK_CharConstDecl, w), value(v)
	{
	}
	Token       Translate() const override;
	char        Value() const { return value; }
	static bool classof(const ConstDecl* e) { return e->getKind() == CK_CharConstDecl; }
	void        dump() const override;

    private:
	char value;
    };

    class BoolConstDecl : public ConstDecl
    {
    public:
	BoolConstDecl(const Location& w, bool v)
	    : ConstDecl(Types::GetBooleanType(), CK_BoolConstDecl, w), value(v)
	{
	}
	Token       Translate() const override;
	bool        Value() const { return value; }
	static bool classof(const ConstDecl* e) { return e->getKind() == CK_BoolConstDecl; }
	void        dump() const override;

    private:
	bool value;
    };

    class StringConstDecl : public ConstDecl
    {
    public:
	StringConstDecl(const Location& w, const std::string& v)
	    : ConstDecl(Types::GetStringType(), CK_StringConstDecl, w), value(v)
	{
	}
	Token              Translate() const override;
	const std::string& Value() const { return value; }
	static bool        classof(const ConstDecl* e) { return e->getKind() == CK_StringConstDecl; }
	void               dump() const override;

    private:
	std::string value;
    };

    ConstDecl* ErrorConst(const std::string& msg);
    ConstDecl* operator+(const ConstDecl& lhs, const ConstDecl& rhs);
    ConstDecl* operator-(const ConstDecl& lhs, const ConstDecl& rhs);
    ConstDecl* operator*(const ConstDecl& lhs, const ConstDecl& rhs);
    ConstDecl* operator/(const ConstDecl& lhs, const ConstDecl& rhs);

    llvm::Constant* ConstDeclToLLVMConst(const ConstDecl* cd);

}; // namespace Constants

#endif
