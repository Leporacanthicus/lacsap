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
	Enum,
	Pointer,
	PointerIncomplete,
	Void,
    };

    class TypeDecl;
    struct EnumValue;
    typedef Stack<TypeDecl*> TypeStack;
    typedef StackWrapper<TypeDecl*> TypeWrapper;
    typedef Stack<EnumValue*> EnumStack;
    typedef StackWrapper<EnumValue*> EnumWrapper;


    /* Range is either created by the user, or calculated on basetype */
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

	virtual SimpleTypes Type() const { return type; }
	virtual ~TypeDecl() { }
	virtual std::string to_string() const;
	virtual bool isIntegral() const;
	virtual Range *GetRange() const;

    protected:
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
	virtual SimpleTypes Type() const { return baseType; }
	virtual Range* GetRange() const { return range; }
    private:
	Range* range;
	SimpleTypes baseType;
    };

    struct EnumValue
    {
	EnumValue(const std::string& nm, int v)
	    : name(nm), value(v) {}
	EnumValue(const EnumValue &e)
	    : name(e.name), value(e.value) {}
	std::string name;
	int value;
    };

    typedef std::vector<EnumValue> EnumValues;

    class EnumDecl : public TypeDecl
    {
    public:
	EnumDecl(const std::vector<std::string>& nmv)
	    : TypeDecl(Enum)
	{
	    assert(nmv.size() && "Must have names in the enum type.");
	    SetValues(nmv);
	}
    private:
	void SetValues(const std::vector<std::string>& nmv);
    public:
	virtual Range* GetRange() const { return new Range(0, values.size()-1); }
	const EnumValues& Values() const { return values; }
    private:
	EnumValues values;
    };

    // Since we need do "late" binding of pointer types, we just keep
    // the name and resolve the actual type at a later point. If the
    // type is known, store it directly. (Otherwise, on lookup).
    class PointerDecl : public TypeDecl
    {
    public:
	PointerDecl(const std::string& nm)
	    : TypeDecl(PointerIncomplete), name(nm), baseType(0) {}
	PointerDecl(TypeDecl* ty)
	    : TypeDecl(Pointer), name(""), baseType(ty) {}
    public:
	TypeDecl* BaseType() const;
	const std::string& Name() { return name; }
	void SetBaseType(TypeDecl* t) 
	{
	    assert(t && "Type should be non-NULL");
	    baseType = t; 
	    type = Pointer; 
	}
    private:
	std::string name;
	TypeDecl* baseType;
    };

    static llvm::Type* GetType(const TypeDecl* type);
    static llvm::Type* GetType(SimpleTypes type);

    bool IsTypeName(const std::string& name);
    bool IsEnumValue(const std::string& name);
    void Add(const std::string& nm, TypeDecl* ty);
    TypeDecl* GetTypeDecl(const std::string& name);
    EnumValue* FindEnumValue(const std::string& name);

    void FixUpIncomplete(PointerDecl* p);
    
    Types();

private:
    TypeStack types;
    EnumStack enums;
};

#endif
