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
/*	Record,
        Set,
 */
	Void,
    };
    class TypeDecl;
    struct IndexSpec
    {
	int start;
	int end;
	TypeDecl* type;
    };
    class TypeDecl
    {
    public:
	TypeDecl(SimpleTypes b, size_t nSub = 0, TypeDecl *sub = 0, size_t aDims = 0)
	    : base(b), nSubTypes(nSub), subTypes(sub), arrayDims(aDims) 
	{
	    assert((nSub && sub) || (!nSub && !sub) && 
		   "Must have either zero sub and nsub, or non-zero of both");
	    assert((!aDims || sub) &&
		   "Must have sub if adims!");
	};

        size_t NumSubtypes() const { return nSubTypes; }
	size_t ArrayDims() { return arrayDims; }
	const TypeDecl* SubType(size_t n)
	{
	    assert(n < nSubTypes && "Subtype out of range!");
	    return &subTypes[n];
	}
	const IndexSpec* IndexSpecForDim(size_t dim)
	{
	    assert(dim < arrayDims && "Array dimension out of range");
	    return &indexSpec[dim];
	}
	SimpleTypes GetType() const { return base; }

	std::string to_string() const;
    private:
	SimpleTypes base;
	/* if Base is Array, Record or Set, we store number of subtypes and  */
	size_t nSubTypes;
	TypeDecl *subTypes;
	size_t arrayDims;
	IndexSpec* indexSpec;
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
