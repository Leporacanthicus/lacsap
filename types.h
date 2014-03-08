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

    typedef Stack<TypeDecl*> TypeStack;
    typedef StackWrapper<TypeDecl*> TypeWrapper;

    TypeStack& GetTypeStack() { return types; }

    static bool IsTypeName(const std::string& name);
    static llvm::Type* GetType(const TypeDecl* type);
    static llvm::Type* GetType(SimpleTypes type);
    TypeDecl* GetTypeDecl(const std::string& name);
    
    Types();

private:
    TypeStack types;
};

#endif
