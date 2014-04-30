#ifndef CONSTANTS_H
#define CONSTANTS_H

// Need token for "location". 
#include "token.h"

class Constants
{
public:
    class ConstDecl
    {
    public:
	ConstDecl(const Location& w)
	    : loc(w) {}
	virtual ~ConstDecl() {}
	virtual Token Translate() = 0;
    protected:
	Location loc;
    };

    class IntConstDecl : public ConstDecl
    {
    public:
	IntConstDecl(const Location& w, long v) 
	    : ConstDecl(w), value(v) {}
	virtual Token Translate();
    private:
	long value;
    };

    class RealConstDecl : public ConstDecl
    {
    public:
	RealConstDecl(const Location& w, double v) 
	    : ConstDecl(w), value(v) {}
	virtual Token Translate();
    private:
	double value;
    };

    class CharConstDecl : public ConstDecl
    {
    public:
	CharConstDecl(const Location& w, char v) 
	    : ConstDecl(w), value(v) {}
	virtual Token Translate();
    private:
	char value;
    };

    class BoolConstDecl : public ConstDecl
    {
    public:
	BoolConstDecl(const Location& w, bool v) 
	    : ConstDecl(w), value(v) {}
	virtual Token Translate();
    private:
	bool value;
    };

    class StringConstDecl : public ConstDecl
    {
    public:
	StringConstDecl(const Location& w, const std::string &v) 
	    : ConstDecl(w), value(v) {}
	virtual Token Translate();
    private:
	std::string value;
    };
};

#endif
