#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "token.h"
#include "stack.h"

class Constants
{
public:
    class ConstDecl;

    typedef Stack<ConstDecl*> ConstStack;
    typedef StackWrapper<ConstDecl*> ConstWrapper;

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
	IntConstDecl(const Location& w, int v) 
	    : ConstDecl(w), value(v) {}
	virtual Token Translate();
    private:
	int value;
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

    bool IsConstant(const std::string& name);

    bool Add(const std::string& nm, ConstDecl* ty);
    ConstDecl* GetConstDecl(const std::string& name);

    ConstStack& GetConsts() { return constants; }

    Constants();
    
private:
    ConstStack constants;
};

#endif
