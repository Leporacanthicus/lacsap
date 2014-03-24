#ifndef TYPES_H
#define TYPES_H

#include <llvm/IR/Type.h>
#include <string>


class PrototypeAST;

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
	Procedure,    // Do we need this?
	Record,
        Set,
	SubRange,
	Enum,
	Pointer,
	PointerIncomplete,
	Void,
	Field,
	File,
    };

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
	size_t Size() const { return (size_t) (end - start) + 1; }
	void dump() const;
    private:
	int start;
	int end;
    };

    class TypeDecl
    {
    public:
	TypeDecl(SimpleTypes t)
	    : type(t), ltype(0)
	{
	}

	virtual SimpleTypes Type() const { return type; }
	virtual ~TypeDecl() { }
	virtual std::string to_string() const;
	virtual bool isIntegral() const;
	virtual Range *GetRange() const;
	virtual TypeDecl *SubType() const { return 0; }
	llvm::Type* LlvmType();
	virtual llvm::Type* GetLlvmType() const;
	virtual void dump() const;
    protected:
	SimpleTypes type;
	llvm::Type* ltype;
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
	TypeDecl* SubType() const { return baseType; }
	virtual bool isIntegral() const { return false; }
	virtual llvm::Type* GetLlvmType() const;
	virtual void dump() const;
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
	virtual void dump() const;
	virtual llvm::Type* GetLlvmType() const;
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
	EnumDecl(const std::vector<std::string>& nmv, SimpleTypes ty = Integer)
	    : TypeDecl(Enum), subType(ty)
	{
	    assert(nmv.size() && "Must have names in the enum type.");
	    SetValues(nmv);
	}
    private:
	void SetValues(const std::vector<std::string>& nmv);
    public:
	virtual Range* GetRange() const { return new Range(0, values.size()-1); }
	virtual bool isIntegral() const { return true; }
	const EnumValues& Values() const { return values; }
	virtual llvm::Type* GetLlvmType() const;
	virtual void dump() const;
    private:
	EnumValues  values;
	SimpleTypes subType;
    };

    // Since we need to do "late" binding of pointer types, we just keep
    // the name and resolve the actual type at a later point. If the
    // type is known, store it directly. (Otherwise, when we call the fixup).
    class PointerDecl : public TypeDecl
    {
    public:
	PointerDecl(const std::string& nm)
	    : TypeDecl(PointerIncomplete), name(nm), baseType(0) {}
	PointerDecl(TypeDecl* ty)
	    : TypeDecl(Pointer), name(""), baseType(ty) {}
    public:
	TypeDecl* SubType() const { return baseType; }
	const std::string& Name() { return name; }
	void SetSubType(TypeDecl* t) 
	{
	    assert(t && "Type should be non-NULL");
	    baseType = t; 
	    type = Pointer; 
	}
	virtual bool isIntegral() const { return false; }
	virtual llvm::Type* GetLlvmType() const;
	virtual void dump() const;
    private:
	std::string name;
	TypeDecl* baseType;
    };

    class FieldDecl : public TypeDecl
    {
    public:
	FieldDecl(const std::string& nm, TypeDecl* ty)
	    : TypeDecl(Field), name(nm), baseType(ty) {}
    public:
	const std::string& Name() { return name; }
	TypeDecl* FieldType() const { return baseType; } 
	virtual llvm::Type* GetLlvmType() const;
	virtual void dump() const;
	virtual bool isIntegral() const { return baseType->isIntegral(); }
    private:
	std::string name;
	TypeDecl*   baseType;
    };

    class RecordDecl : public TypeDecl
    {
    public:
	RecordDecl(const std::vector<FieldDecl>& flds)
	    : TypeDecl(Record), fields(flds) { };
	virtual bool isIntegral() const { return false; }
	virtual llvm::Type* GetLlvmType() const;
	virtual void dump() const;
	int Element(const std::string& name) const;
	const FieldDecl& GetElement(int n) { return fields[n]; }
    private:
	std::vector<FieldDecl> fields;
    };

    class FuncPtrDecl : public TypeDecl
    {
    public:
	FuncPtrDecl(PrototypeAST* func);
	virtual bool isIntegral() const { return false; }
	virtual TypeDecl* SubType() const { return baseType; }
	virtual llvm::Type* GetLlvmType() const;
	virtual void dump() const;
	PrototypeAST* Proto() const { return proto; }
    private:
	PrototypeAST* proto;
	TypeDecl*     baseType;
    };

    class FileDecl : public TypeDecl
    {
    public:
	enum
	{
	    Handle,
	    Buffer,
	} FileFields;
	FileDecl(TypeDecl* ty)
	    : TypeDecl(File), baseType(ty) {}
	virtual TypeDecl* SubType() const { return baseType; }
	virtual llvm::Type* GetLlvmType() const;
	virtual void dump() const;
    protected:
	TypeDecl *baseType;
    };

    class TextDecl : public FileDecl
    {
    public:
	TextDecl()
	    : FileDecl(new TypeDecl(Char)) {}
	virtual llvm::Type* GetLlvmType() const;
	virtual void dump() const;
    };

    static llvm::Type* GetType(SimpleTypes type);
    static llvm::Type* GetVoidPtrType();
    static llvm::Type* GetFileType(const std::string& name, TypeDecl* baseType);
};
#endif
