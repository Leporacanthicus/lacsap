#ifndef TYPES_H
#define TYPES_H

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <string>

class PrototypeAST;
class ExprAST;
class InitializerAST;

namespace Types
{
    class TypeDecl;

    TypeDecl* GetIntegerType();
    TypeDecl* GetLongIntType();
    TypeDecl* GetCharType();
    TypeDecl* GetBooleanType();
    TypeDecl* GetRealType();
    TypeDecl* GetVoidType();
    TypeDecl* GetTextType();
    TypeDecl* GetStringType();
    TypeDecl* GetTimeStampType();

    // Range is either created by the user, or calculated on basetype
    class Range
    {
    public:
	Range(int64_t s, int64_t e) : start(s), end(e)
	{
	    assert((e - s) > 0 && "Range should have start before end.");
	}

    public:
	int64_t Start() const { return start; }
	int64_t End() const { return end; }
	size_t  Size() const { return (size_t)(end - start) + 1; }
	void    dump() const;
	void    DoDump(std::ostream& out) const;

    private:
	int64_t start;
	int64_t end;
    };

    using InitializerList = std::vector<std::pair<int, InitializerAST*>>;

    class TypeDecl
    {
    public:
	enum TypeKind
	{
	    TK_Type,
	    TK_Char,
	    TK_Integer,
	    TK_LongInt,
	    TK_Real,
	    TK_Void,
	    TK_Array,
	    TK_String,
	    TK_LastArray,
	    TK_Range,
	    TK_Enum,
	    TK_Boolean,
	    TK_Pointer,
	    TK_Field,
	    TK_Record,
	    TK_FuncPtr,
	    TK_Function,
	    TK_File,
	    TK_Text,
	    TK_Set,
	    TK_Variant,
	    TK_Class,
	    TK_MemberFunc,
	    TK_Forward,
	};

	TypeDecl(TypeKind k) : kind(k), lType(0), diType(0), name("") {}

	virtual TypeKind Type() const { return kind; }
	virtual ~TypeDecl() {}
	virtual bool            IsIncomplete() const { return false; }
	virtual bool            IsIntegral() const { return false; }
	virtual bool            IsCompound() const { return false; }
	virtual bool            IsStringLike() const { return false; }
	virtual bool            IsUnsigned() const { return false; }
	virtual Range*          GetRange() const;
	virtual TypeDecl*       SubType() const { return 0; }
	virtual unsigned        Bits() const { return 0; }
	virtual bool            SameAs(const TypeDecl* ty) const = 0;
	virtual const TypeDecl* CompatibleType(const TypeDecl* ty) const;
	virtual const TypeDecl* AssignableType(const TypeDecl* ty) const { return CompatibleType(ty); }
	llvm::Type*             LlvmType() const;
	llvm::DIType*           DebugType(llvm::DIBuilder* builder) const;
	llvm::DIType*           DiType() const { return diType; }
	void                    DiType(llvm::DIType* d) const { diType = d; }
	void                    ResetDebugType() const { diType = 0; }
	virtual bool            HasLlvmType() const = 0;
	void                    dump(std::ostream& out) const { DoDump(out); }
	void                    dump() const;
	virtual void            DoDump(std::ostream& out) const = 0;
	TypeKind                getKind() const { return kind; }
	static bool             classof(const TypeDecl* e) { return e->getKind() == TK_Type; }
	virtual size_t          Size() const;
	size_t                  AlignSize() const;
	std::string             Name() const { return name; }
	void                    Name(const std::string& nm) { name = nm; }

    protected:
	virtual llvm::Type*   GetLlvmType() const = 0;
	virtual llvm::DIType* GetDIType(llvm::DIBuilder* builder) const = 0;

    protected:
	const TypeKind        kind;
	mutable llvm::Type*   lType;
	mutable llvm::DIType* diType;
	std::string           name;
    };

    class ForwardDecl : public TypeDecl
    {
    public:
	ForwardDecl(const std::string& nm) : TypeDecl(TK_Forward) { Name(nm); }
	bool        IsIncomplete() const override { return true; }
	bool        HasLlvmType() const override { return false; }
	bool        SameAs(const TypeDecl* ty) const override { return false; }
	void        DoDump(std::ostream& out) const override { out << std::string("Forward ") << Name(); }
	llvm::Type* GetLlvmType() const override
	{
	    assert(0 && "No llvm type for forward decls");
	    return 0;
	}
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override
	{
	    assert(0 && "No debug type for forward decls");
	    return 0;
	}
    };

    class BasicTypeDecl : public TypeDecl
    {
    public:
	using TypeDecl::TypeDecl;
	bool SameAs(const TypeDecl* ty) const override { return kind == ty->Type(); }
    };

    class CharDecl : public BasicTypeDecl
    {
    public:
	CharDecl() : BasicTypeDecl(TK_Char) {}
	bool            IsIntegral() const override { return true; }
	bool            IsUnsigned() const override { return true; }
	bool            IsStringLike() const override { return true; }
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	const TypeDecl* AssignableType(const TypeDecl* ty) const override;
	unsigned        Bits() const override { return 8; }
	void            DoDump(std::ostream& out) const override;
	bool            HasLlvmType() const override { return true; }
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Char; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
    };

    class IntegerDecl : public BasicTypeDecl
    {
    public:
	IntegerDecl() : BasicTypeDecl(TK_Integer) {}
	bool            IsIntegral() const override { return true; }
	unsigned        Bits() const override { return 32; }
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	const TypeDecl* AssignableType(const TypeDecl* ty) const override;
	bool            HasLlvmType() const override { return true; }
	void            DoDump(std::ostream& out) const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Integer; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
    };

    class Int64Decl : public BasicTypeDecl
    {
    public:
	Int64Decl() : BasicTypeDecl(TK_LongInt) {}
	bool            IsIntegral() const override { return true; }
	unsigned        Bits() const override { return 64; }
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	const TypeDecl* AssignableType(const TypeDecl* ty) const override;
	bool            HasLlvmType() const override { return true; }
	void            DoDump(std::ostream& out) const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_LongInt; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
    };

    class RealDecl : public BasicTypeDecl
    {
    public:
	RealDecl() : BasicTypeDecl(TK_Real) {}
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	const TypeDecl* AssignableType(const TypeDecl* ty) const override;
	unsigned        Bits() const override { return 64; }
	bool            HasLlvmType() const override { return true; }
	void            DoDump(std::ostream& out) const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Real; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
    };

    class VoidDecl : public BasicTypeDecl
    {
    public:
	VoidDecl() : BasicTypeDecl(TK_Void) {}
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override { return 0; }
	bool            HasLlvmType() const override { return true; }
	void            DoDump(std::ostream& out) const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Void; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override { return 0; }
    };

    class CompoundDecl : public TypeDecl
    {
    public:
	CompoundDecl(TypeKind tk, TypeDecl* b) : TypeDecl(tk), baseType(b) {}
	bool        SameAs(const TypeDecl* ty) const override;
	bool        IsCompound() const override { return true; }
	TypeDecl*   SubType() const override { return baseType; }
	bool        HasLlvmType() const override { return baseType->HasLlvmType(); }
	static bool classof(const TypeDecl* e);

    protected:
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override
	{
	    return baseType->DebugType(builder);
	}

    protected:
	TypeDecl* baseType;
    };

    class RangeDecl : public CompoundDecl
    {
    public:
	RangeDecl(Range* r, TypeDecl* base) : CompoundDecl(TK_Range, base), range(r)
	{
	    assert(r && "Range should be specified");
	}

    public:
	void            DoDump(std::ostream& out) const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Range; }
	bool            SameAs(const TypeDecl* ty) const override;
	int             Start() const { return range->Start(); }
	int             End() const { return range->End(); }
	TypeKind        Type() const override { return SubType()->Type(); }
	bool            IsCompound() const override { return false; }
	bool            IsIntegral() const override { return true; }
	bool            IsUnsigned() const override { return Start() >= 0; }
	unsigned        Bits() const override;
	Range*          GetRange() const override { return range; }
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	const TypeDecl* AssignableType(const TypeDecl* ty) const override;

    protected:
	llvm::Type* GetLlvmType() const override { return baseType->LlvmType(); }

    private:
	Range* range;
    };

    class ArrayDecl : public CompoundDecl
    {
    public:
	ArrayDecl(TypeDecl* b, const std::vector<RangeDecl*>& r) : CompoundDecl(TK_Array, b), ranges(r)
	{
	    assert(r.size() > 0 && "Empty range not allowed");
	}
	ArrayDecl(TypeKind tk, TypeDecl* b, const std::vector<RangeDecl*>& r) : CompoundDecl(tk, b), ranges(r)
	{
	    assert(tk == TK_String && "Expected this to be a string...");
	    assert(r.size() > 0 && "Empty range not allowed");
	}
	const std::vector<RangeDecl*>& Ranges() const { return ranges; }
	bool                           IsStringLike() const override { return (baseType->Type() == TK_Char); }
	void                           DoDump(std::ostream& out) const override;
	bool                           SameAs(const TypeDecl* ty) const override;
	const TypeDecl*                CompatibleType(const TypeDecl* ty) const override;
	static bool                    classof(const TypeDecl* e)
	{
	    return e->getKind() >= TK_Array && e->getKind() <= TK_LastArray;
	}

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	std::vector<RangeDecl*> ranges;
    };

    struct EnumValue
    {
	EnumValue(const std::string& nm, int v) : name(nm), value(v) {}
	EnumValue(const EnumValue& e) : name(e.name), value(e.value) {}
	std::string name;
	int         value;
    };

    typedef std::vector<EnumValue> EnumValues;

    class EnumDecl : public CompoundDecl
    {
    public:
	EnumDecl(TypeKind tk, const std::vector<std::string>& nmv, TypeDecl* ty) : CompoundDecl(tk, ty)
	{
	    assert(nmv.size() && "Must have names in the enum type.");
	    SetValues(nmv);
	}
	EnumDecl(const std::vector<std::string>& nmv, TypeDecl* ty) : EnumDecl(TK_Enum, nmv, ty) {}

    private:
	void SetValues(const std::vector<std::string>& nmv);

    public:
	Range*            GetRange() const override { return new Range(0, values.size() - 1); }
	const EnumValues& Values() const { return values; }
	bool              IsIntegral() const override { return true; }
	bool              IsUnsigned() const override { return true; }
	bool              IsCompound() const override { return false; }
	void              DoDump(std::ostream& out) const override;
	unsigned          Bits() const override;
	static bool       classof(const TypeDecl* e) { return e->getKind() == TK_Enum; }
	bool              SameAs(const TypeDecl* ty) const override;

    protected:
	llvm::Type*   GetLlvmType() const override { return baseType->LlvmType(); }
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	EnumValues values;
    };

    class BoolDecl : public EnumDecl
    {
    public:
	BoolDecl() : EnumDecl(TK_Boolean, std::vector<std::string>{ "false", "true" }, this) {}
	void DoDump(std::ostream& out) const override;
	bool SameAs(const TypeDecl* ty) const override { return ty == this; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
    };

    // Since we need to do "late" binding of pointer types, we just keep
    // the name and resolve the actual type at a later point. If the
    // type is known, store it directly. (Otherwise, when we call the fixup).
    class PointerDecl : public CompoundDecl
    {
    public:
	PointerDecl(ForwardDecl* fwd) : CompoundDecl(TK_Pointer, fwd), incomplete(true), forward(true) {}
	PointerDecl(TypeDecl* ty) : CompoundDecl(TK_Pointer, ty), incomplete(false), forward(false) {}

    public:
	void SetSubType(TypeDecl* t)
	{
	    assert(t && "Type should be non-NULL");
	    baseType = t;
	    incomplete = false;
	}
	bool        IsIncomplete() const override { return incomplete; }
	bool        IsForward() const { return forward; }
	bool        IsCompound() const override { return false; }
	void        DoDump(std::ostream& out) const override;
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Pointer; }
	bool        HasLlvmType() const override { return baseType->HasLlvmType(); }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	bool incomplete;
	bool forward;
    };

    class FunctionDecl : public CompoundDecl
    {
    public:
	FunctionDecl(PrototypeAST* proto);
	void            DoDump(std::ostream& out) const override;
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override
	{
	    return baseType->CompatibleType(ty);
	}
	const TypeDecl* AssignableType(const TypeDecl* ty) const override
	{
	    return baseType->AssignableType(ty);
	}
	bool          IsCompound() const override { return false; }
	bool          HasLlvmType() const override { return false; }
	static bool   classof(const TypeDecl* e) { return e->getKind() == TK_Function; }
	PrototypeAST* Proto() const { return proto; }

    protected:
	llvm::Type* GetLlvmType() const override { return 0; }

    private:
	PrototypeAST* proto;
    };

    class FieldDecl : public CompoundDecl
    {
    public:
	enum Access
	{
	    Private,
	    Protected,
	    Public
	};
	FieldDecl(const std::string& nm, TypeDecl* ty, bool stat, Access ac = Public)
	    : CompoundDecl(TK_Field, ty), isStatic(stat)
	{
	    Name(nm);
	}

    public:
	TypeDecl*   FieldType() const { return baseType; }
	void        DoDump(std::ostream& out) const override;
	bool        IsIntegral() const override { return baseType->IsIntegral(); }
	bool        IsCompound() const override { return baseType->IsCompound(); }
	bool        IsStatic() const { return isStatic; }
	bool        SameAs(const TypeDecl* ty) const override { return baseType->SameAs(ty); }
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Field; }
	            operator Access() { return access; }

    protected:
	llvm::Type* GetLlvmType() const override { return baseType->LlvmType(); }

    private:
	bool   isStatic;
	Access access;
    };

    class FieldCollection : public TypeDecl
    {
    public:
	FieldCollection(TypeKind k, const std::vector<FieldDecl*>& flds)
	    : TypeDecl(k), fields(flds), opaqueType(0)
	{
	}
	virtual int              Element(const std::string& name) const;
	virtual const FieldDecl* GetElement(unsigned int n) const
	{
	    assert(n < fields.size() && "Out of range field");
	    return fields[n];
	}
	void        EnsureSized() const;
	virtual int FieldCount() const { return fields.size(); }
	bool        IsCompound() const override { return true; }
	bool        SameAs(const TypeDecl* ty) const override;
	bool        HasLlvmType() const override { return lType; }
	static bool classof(const TypeDecl* e)
	{
	    return e->getKind() == TK_Variant || e->getKind() == TK_Record || e->getKind() == TK_Class;
	}

    protected:
	std::vector<FieldDecl*>   fields;
	mutable llvm::StructType* opaqueType;
    };

    class VariantDecl : public FieldCollection
    {
    public:
	VariantDecl(const std::vector<FieldDecl*>& flds) : FieldCollection(TK_Variant, flds){};
	void        DoDump(std::ostream& out) const override;
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Variant; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
    };

    class RecordDecl : public FieldCollection
    {
    public:
	RecordDecl(const std::vector<FieldDecl*>& flds, VariantDecl* v)
	    : FieldCollection(TK_Record, flds), variant(v){};
	void         DoDump(std::ostream& out) const override;
	size_t       Size() const override;
	VariantDecl* Variant() const { return variant; }
	bool         SameAs(const TypeDecl* ty) const override { return this == ty; }
	static bool  classof(const TypeDecl* e) { return e->getKind() == TK_Record; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	VariantDecl* variant;
    };

    // Objects can have member functions/procedures
    class MemberFuncDecl : public TypeDecl
    {
    public:
	enum Flags
	{
	    Static = 1 << 0,
	    Virtual = 1 << 1,
	    Override = 1 << 2,
	};
	MemberFuncDecl(PrototypeAST* p, int f) : TypeDecl(TK_MemberFunc), proto(p), flags(f), index(-1) {}

	void          DoDump(std::ostream& out) const override;
	bool          SameAs(const TypeDecl* ty) const override { return this == ty; }
	PrototypeAST* Proto() { return proto; }
	std::string   LongName() const { return longname; }
	void          LongName(const std::string& name) { longname = name; }
	bool          IsStatic() { return flags & Static; }
	bool          IsVirtual() { return flags & Virtual; }
	bool          IsOverride() { return flags & Override; }
	int           VirtIndex() const { return index; }
	void          VirtIndex(int n) { index = n; }
	bool          HasLlvmType() const override { return false; }
	static bool   classof(const TypeDecl* e) { return e->getKind() == TK_MemberFunc; }

    protected:
	// We don't actually have an LLVM type for member functions.
	llvm::Type*   GetLlvmType() const override { return 0; }
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override { return 0; }

    private:
	PrototypeAST* proto;
	int           flags;
	int           index;
	std::string   longname;
    };

    class ClassDecl : public FieldCollection
    {
    public:
	ClassDecl(const std::string& nm, const std::vector<FieldDecl*>& flds,
	          const std::vector<MemberFuncDecl*> mf, VariantDecl* v, ClassDecl* base);

	void             DoDump(std::ostream& out) const override;
	int              Element(const std::string& name) const override;
	const FieldDecl* GetElement(unsigned int n) const override;
	const FieldDecl* GetElement(unsigned int n, std::string& objname) const;
	int              FieldCount() const override;
	size_t           Size() const override;
	VariantDecl*     Variant() { return variant; }
	bool             SameAs(const TypeDecl* ty) const override { return this == ty; }
	size_t           MembFuncCount() const;
	int              MembFunc(const std::string& nm) const;
	MemberFuncDecl*  GetMembFunc(size_t index) const;
	size_t           NumVirtFuncs() const;
	const TypeDecl*  CompatibleType(const TypeDecl* ty) const override;
	llvm::Type*      VTableType(bool opaque) const;
	static bool      classof(const TypeDecl* e) { return e->getKind() == TK_Class; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	ClassDecl*                   baseobj;
	VariantDecl*                 variant;
	std::vector<MemberFuncDecl*> membfuncs;
	mutable llvm::StructType*    vtableType;
    };

    class FuncPtrDecl : public CompoundDecl
    {
    public:
	FuncPtrDecl(PrototypeAST* func);
	void          DoDump(std::ostream& out) const override;
	PrototypeAST* Proto() const { return proto; }
	bool          IsCompound() const override { return false; }
	bool          SameAs(const TypeDecl* ty) const override;
	bool          HasLlvmType() const override { return true; }
	static bool   classof(const TypeDecl* e) { return e->getKind() == TK_FuncPtr; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	PrototypeAST* proto;
    };

    class FileDecl : public CompoundDecl
    {
    public:
	enum
	{
	    Handle,
	    Buffer,
	    RecordSize,
	    IsText
	} FileFields;
	FileDecl(TypeDecl* ty) : CompoundDecl(TK_File, ty) {}
	FileDecl(TypeKind k, TypeDecl* ty) : CompoundDecl(k, ty) {}
	void        DoDump(std::ostream& out) const override;
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_File || e->getKind() == TK_Text; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
    };

    class TextDecl : public FileDecl
    {
    public:
	TextDecl() : FileDecl(TK_Text, new CharDecl) {}
	void        DoDump(std::ostream& out) const override;
	bool        HasLlvmType() const override { return true; }
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Text; }
    };

    class SetDecl : public CompoundDecl
    {
    public:
	typedef unsigned int ElemType;
	// Must match with "runtime".
	enum
	{
	    MaxSetWords = 16,
	    SetBits = 32,
	    MaxSetSize = MaxSetWords * SetBits,
	    SetMask = SetBits - 1,
	    SetPow2Bits = 5
	};
	SetDecl(RangeDecl* r, TypeDecl* ty);
	void            DoDump(std::ostream& out) const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Set; }
	size_t          SetWords() const { return (range->GetRange()->Size() + SetMask) >> SetPow2Bits; }
	Range*          GetRange() const override;
	void            UpdateRange(RangeDecl* r) { range = r; }
	void            UpdateSubtype(TypeDecl* ty);
	bool            SameAs(const TypeDecl* ty) const override;
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	bool            HasLlvmType() const override { return true; }

    private:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	RangeDecl* range;
    };

    class StringDecl : public ArrayDecl
    {
    public:
	StringDecl(unsigned size)
	    : ArrayDecl(TK_String, new CharDecl,
	                std::vector<RangeDecl*>(1, new RangeDecl(new Range(0, size), GetIntegerType())))
	{
	    assert(size > 0 && "Zero size not allowed");
	}
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_String; }
	bool            IsStringLike() const override { return true; }
	void            DoDump(std::ostream& out) const override;
	bool            HasLlvmType() const override { return true; }
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
    };

    llvm::Type* GetVoidPtrType();

    void Finalize(llvm::DIBuilder* builder);
} // Namespace Types

bool        operator==(const Types::TypeDecl& lty, const Types::TypeDecl& rty);
inline bool operator!=(const Types::TypeDecl& lty, const Types::TypeDecl& rty)
{
    return !(lty == rty);
}

bool        operator==(const Types::Range& a, const Types::Range& b);
inline bool operator!=(const Types::Range& a, const Types::Range& b)
{
    return !(b == a);
}

bool        operator==(const Types::EnumValue& a, const Types::EnumValue& b);
inline bool operator!=(const Types::EnumValue& a, const Types::EnumValue& b)
{
    return !(a == b);
}

#endif
