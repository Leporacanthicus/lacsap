#ifndef TYPES_H
#define TYPES_H

#include "stack.h"
#include <llvm/IR/Type.h>
#include <string>

class Types
{
public:
    enum SimpleTypes
    {
	Integer,
	Real,
	Char,
	Boolean,
	Array,
	Function,
	Procedure,
	Record,
        Set,
	SubRange,
	Void,
    };

    class Range
    {
    public:
	Range(int s, int e) 
	    : start(s), end(e)
	{ 
	    assert( (e - s) > 0 && "Range should have start before end.");
	}
    public:
	int GetStart() const { return start; }
	int GetEnd() const { return end; }
	size_t Size() const { return (size_t) end - start; }
    private:
	int start;
	int end;
    };

    class TypeDecl
    {
    public:
	TypeDecl(SimpleTypes t)
	    : type(t)
	{
	}

	virtual SimpleTypes GetType() const { return type; }
	virtual ~TypeDecl() { }
	virtual std::string to_string() const;
	virtual bool isIntegral() const;
	Range *GetRange() const;
    private:
	SimpleTypes type;
    };

    class ArrayDecl : public TypeDecl
    {
    public:
	ArrayDecl(TypeDecl *b, const std::vector<Range*>& r)
	    : TypeDecl(Array), baseType(b), ranges(r)
	{
	    assert(r.size() > 0 && "Empty range not allowed");
	}
	const std::vector<Range*>& Ranges() const { return ranges; }
	TypeDecl* BaseType() const { return baseType; }
    private:
	TypeDecl* baseType;
	std::vector<Range*> ranges;
    };

    class RangeDecl : public TypeDecl
    {
    public:
	RangeDecl(Range *r, SimpleTypes base)
	    : TypeDecl(SubRange), range(r), baseType(base)
	{
	    assert(r && "Range should be specified");
	}
    public:
	virtual bool isIntegral() const { return true; }
	virtual SimpleTypes GetType() const { return baseType; }
    private:
	Range* range;
	SimpleTypes baseType;
    };

    typedef Stack<TypeDecl*> TypeStack;
    typedef StackWrapper<TypeDecl*> TypeWrapper;

    TypeStack& GetTypes() { return types; }

    static llvm::Type* GetType(const TypeDecl* type);
    static llvm::Type* GetType(SimpleTypes type);
    

    bool IsTypeName(const std::string& name);
    void Add(const std::string& nm, TypeDecl* ty) { types.Add(nm, ty); }
   TypeDecl* GetTypeDecl(const std::string& name);
    
    Types();

private:
    TypeStack types;
};

#endif
