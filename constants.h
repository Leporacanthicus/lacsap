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
	virtual Token Translate() = 0;
	ConstKind getKind() const { return kind; }
    protected:
	const ConstKind kind;
	Location loc;

    };

    class IntConstDecl : public ConstDecl
    {
    public:
	IntConstDecl(const Location& w, long v) 
	    : ConstDecl(CK_IntConstDecl, w), value(v) {}
	virtual Token Translate();
	long Value() const { return value; }
	static bool classof(const ConstDecl *e) { return e->getKind() == CK_IntConstDecl; }
    private:
	long value;
    };

    class RealConstDecl : public ConstDecl
    {
    public:
	RealConstDecl(const Location& w, double v) 
	    : ConstDecl(CK_RealConstDecl, w), value(v) {}
	virtual Token Translate();
	double Value() const { return value; }
	static bool classof(const ConstDecl *e) { return e->getKind() == CK_RealConstDecl; }
    private:
	double value;
    };

    class CharConstDecl : public ConstDecl
    {
    public:
	CharConstDecl(const Location& w, char v) 
	    : ConstDecl(CK_CharConstDecl, w), value(v) {}
	virtual Token Translate();
	static bool classof(const ConstDecl *e) { return e->getKind() == CK_CharConstDecl; }
    private:
	char value;
    };

    class BoolConstDecl : public ConstDecl
    {
    public:
	BoolConstDecl(const Location& w, bool v) 
	    : ConstDecl(CK_BoolConstDecl, w), value(v) {}
	virtual Token Translate();
	bool Value() const { return value; }
	static bool classof(const ConstDecl *e) { return e->getKind() == CK_BoolConstDecl; }
    private:
	bool value;
    };

    class StringConstDecl : public ConstDecl
    {
    public:
	StringConstDecl(const Location& w, const std::string &v) 
	    : ConstDecl(CK_StringConstDecl, w), value(v) {}
	virtual Token Translate();
	const std::string& Value() const { return value; }
	static bool classof(const ConstDecl *e) { return e->getKind() == CK_StringConstDecl; }
    private:
	std::string value;
    };

};



Constants::ConstDecl* operator+(const Constants::ConstDecl& lhs, const Constants::ConstDecl& rhs); 
Constants::ConstDecl* operator-(const Constants::ConstDecl& lhs, const Constants::ConstDecl& rhs); 
Constants::ConstDecl* ErrorConst(const std::string& msg);

#endif
