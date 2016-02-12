#ifndef CONSTANTS_H
#define CONSTANTS_H

// Need token for "location". 
#include "token.h"

class Constants
{
public:
    enum ConstKind
    {
	CK_ConstDecl,
	CK_IntConstDecl,
	CK_RealConstDecl,
	CK_CharConstDecl,
	CK_BoolConstDecl, 
	CK_StringConstDecl,
    };

    class ConstDecl
    {
    public:
	ConstDecl(ConstKind k, const Location& w)
	    : kind(k), loc(w) {}
	virtual ~ConstDecl() {}
	virtual Token Translate() const = 0;
	ConstKind getKind() const { return kind; }
	virtual void dump() const = 0;
	Location Loc() const { return loc; }
    protected:
	const ConstKind kind;
	Location loc;

    };

    class IntConstDecl : public ConstDecl
    {
    public:
	IntConstDecl(const Location& w, uint64_t v) 
	    : ConstDecl(CK_IntConstDecl, w), value(v) {}
	Token Translate() const override;
	uint64_t Value() const { return value; }
	static bool classof(const ConstDecl *e) { return e->getKind() == CK_IntConstDecl; }
	void dump() const override;
    private:
	uint64_t value;
    };

    class RealConstDecl : public ConstDecl
    {
    public:
	RealConstDecl(const Location& w, double v) 
	    : ConstDecl(CK_RealConstDecl, w), value(v) {}
	Token Translate() const override;
	double Value() const { return value; }
	static bool classof(const ConstDecl *e) { return e->getKind() == CK_RealConstDecl; }
	void dump() const override;
    private:
	double value;
    };

    class CharConstDecl : public ConstDecl
    {
    public:
	CharConstDecl(const Location& w, char v) 
	    : ConstDecl(CK_CharConstDecl, w), value(v) {}
	Token Translate() const override;
	char Value() const { return value; }
	static bool classof(const ConstDecl *e) { return e->getKind() == CK_CharConstDecl; }
	void dump() const override;
    private:
	char value;
    };

    class BoolConstDecl : public ConstDecl
    {
    public:
	BoolConstDecl(const Location& w, bool v) 
	    : ConstDecl(CK_BoolConstDecl, w), value(v) {}
	Token Translate() const override;
	bool Value() const { return value; }
	static bool classof(const ConstDecl *e) { return e->getKind() == CK_BoolConstDecl; }
	void dump() const override;
    private:
	bool value;
    };

    class StringConstDecl : public ConstDecl
    {
    public:
	StringConstDecl(const Location& w, const std::string &v) 
	    : ConstDecl(CK_StringConstDecl, w), value(v) {}
        Token Translate() const override;
	const std::string& Value() const { return value; }
	static bool classof(const ConstDecl *e) { return e->getKind() == CK_StringConstDecl; }
	void dump() const override;
    private:
	std::string value;
    };

};

Constants::ConstDecl* ErrorConst(const std::string& msg);
Constants::ConstDecl* operator+(const Constants::ConstDecl& lhs, const Constants::ConstDecl& rhs); 
Constants::ConstDecl* operator-(const Constants::ConstDecl& lhs, const Constants::ConstDecl& rhs); 
Constants::ConstDecl* operator*(const Constants::ConstDecl& lhs, const Constants::ConstDecl& rhs); 

#endif
