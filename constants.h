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
	virtual ConstDecl* doCopy(const Location& w) const = 0;
	ConstDecl* Copy(const Location& w) const
	{
	    return doCopy(w);
	}
    protected:
	Location loc;
    };

    class IntConstDecl : public ConstDecl
    {
    public:
	IntConstDecl(const Location& w, long v) 
	    : ConstDecl(w), value(v) {}
	virtual Token Translate();
	virtual ConstDecl* doCopy(const Location& w) const
	{
	    return new IntConstDecl(w, value);
	}
    private:
	long value;
    };

    class RealConstDecl : public ConstDecl
    {
    public:
	RealConstDecl(const Location& w, double v) 
	    : ConstDecl(w), value(v) {}
	virtual Token Translate();
	virtual ConstDecl* doCopy(const Location& w) const
	{
	    return new RealConstDecl(w, value);
	}
    private:
	double value;
    };

    class CharConstDecl : public ConstDecl
    {
    public:
	CharConstDecl(const Location& w, char v) 
	    : ConstDecl(w), value(v) {}
	virtual Token Translate();
	virtual ConstDecl* doCopy(const Location& w) const
	{
	    return new CharConstDecl(w, value);
	}
    private:
	char value;
    };

    class BoolConstDecl : public ConstDecl
    {
    public:
	BoolConstDecl(const Location& w, bool v) 
	    : ConstDecl(w), value(v) {}
	virtual Token Translate();
	virtual ConstDecl* doCopy(const Location& w) const
	{
	    return new BoolConstDecl(w, value);
	}
    private:
	bool value;
    };

    class StringConstDecl : public ConstDecl
    {
    public:
	StringConstDecl(const Location& w, const std::string &v) 
	    : ConstDecl(w), value(v) {}
	virtual Token Translate();
	virtual ConstDecl* doCopy(const Location& w) const
	{
	    return new StringConstDecl(w, value);
	}
    private:
	std::string value;
    };
    
};

#endif
