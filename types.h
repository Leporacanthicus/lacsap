#ifndef TYPES_H
#define TYPES_H

#include <iostream>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <string>

class PrototypeAST;
class ExprAST;

extern llvm::LLVMContext theContext;

namespace Types
{
    class TypeDecl;

    template<typename T, typename... Args>
    TypeDecl* Get(Args... args)
    {
	static TypeDecl* typePtr;
	if (!typePtr)
	{
	    typePtr = new T(std::forward<Args>(args)...);
	}
	return typePtr;
    }

    TypeDecl* GetTimeStampType();
    TypeDecl* GetBindingType();

    bool IsNumeric(const TypeDecl* t);
    bool IsCharArray(const TypeDecl* t);

    // Range is either created by the user, or calculated on basetype
    class Range
    {
    public:
	Range(int64_t s, int64_t e) : start(s), end(e)
	{
	    assert((e - s) > 0 && "Range should have start before end.");
	}

    public:
	int64_t  Start() const { return start; }
	int64_t  End() const { return end; }
	uint64_t Size() const { return (uint64_t)(end - start) + 1; }
	void     dump() const;
	void     DoDump() const;

    private:
	int64_t start;
	int64_t end;
    };

    class TypeDecl
    {
    public:
	enum TypeKind
	{
	    TK_Char,
	    TK_Integer,
	    TK_LongInt,
	    TK_Real,
	    TK_Void,
	    TK_Array,
	    TK_String,
	    TK_LastArray,
	    TK_DynArray,
	    TK_Range,
	    TK_DynRange,
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
	    TK_Complex,
	};

	TypeDecl(TypeKind k) : kind(k), lType(0), diType(0), name(""), init(0) {}

	virtual TypeKind Type() const { return kind; }
	virtual ~TypeDecl() {}
	virtual bool            IsIncomplete() const { return false; }
	virtual bool            IsIntegral() const { return false; }
	virtual bool            IsCompound() const { return false; }
	virtual bool            IsStringLike() const { return false; }
	virtual bool            IsUnsigned() const { return false; }
	virtual Range*          GetRange() const;
	virtual unsigned        Bits() const { return 0; }
	virtual bool            SameAs(const TypeDecl* ty) const { return kind == ty->Type(); }
	virtual const TypeDecl* CompatibleType(const TypeDecl* ty) const;
	virtual const TypeDecl* AssignableType(const TypeDecl* ty) const { return CompatibleType(ty); }
	llvm::Type*             LlvmType() const;
	llvm::DIType*           DebugType(llvm::DIBuilder* builder) const;
	llvm::DIType*           DiType() const { return diType; }
	void                    DiType(llvm::DIType* d) const { diType = d; }
	void                    ResetDebugType() const { diType = 0; }
	virtual bool            HasLlvmType() const { return true; };
	void                    dump() const;
	virtual void            DoDump() const = 0;
	TypeKind                getKind() const { return kind; }
	virtual size_t          Size() const;
	size_t                  AlignSize() const;
	std::string             Name() const { return name; }
	void                    Name(const std::string& nm) { name = nm; }
	ExprAST*                Init() { return init; }
	void                    SetInit(ExprAST* i)
	{
	    assert(!init && "Don't set init twice");
	    init = i;
	};
	virtual TypeDecl* Clone() const
	{
	    assert(0 && "Unimplemented clone");
	    return 0;
	}

    protected:
	virtual llvm::Type*   GetLlvmType() const = 0;
	virtual llvm::DIType* GetDIType(llvm::DIBuilder* builder) const = 0;

    protected:
	const TypeKind        kind;
	mutable llvm::Type*   lType;
	mutable llvm::DIType* diType;
	std::string           name;
	ExprAST*              init;
    };

    class ForwardDecl : public TypeDecl
    {
    public:
	ForwardDecl(const std::string& nm) : TypeDecl(TK_Forward) { Name(nm); }
	bool IsIncomplete() const override { return true; }
	bool HasLlvmType() const override { return false; }
	bool SameAs(const TypeDecl* ty) const override { return false; }
	void DoDump() const override { std::cerr << std::string("Forward ") << Name(); }

    protected:
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

    class CharDecl : public TypeDecl
    {
    public:
	CharDecl() : TypeDecl(TK_Char) {}
	bool            IsIntegral() const override { return true; }
	bool            IsUnsigned() const override { return true; }
	bool            IsStringLike() const override { return true; }
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	const TypeDecl* AssignableType(const TypeDecl* ty) const override;
	unsigned        Bits() const override { return 8; }
	void            DoDump() const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Char; }
	TypeDecl*       Clone() const override { return new CharDecl(); }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
    };

    template<int bits, TypeDecl::TypeKind tk>
    class IntegerXDecl : public TypeDecl
    {
    public:
	IntegerXDecl() : TypeDecl(tk) {}
	bool            IsIntegral() const override { return true; }
	unsigned        Bits() const override { return bits; }
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	const TypeDecl* AssignableType(const TypeDecl* ty) const override;
	void            DoDump() const override { std::cerr << "Type: Integer<" << bits << ">"; }
	static bool     classof(const TypeDecl* e) { return e->getKind() == tk; }
	TypeDecl*       Clone() const override { return new IntegerXDecl<bits, tk>(); }

    protected:
	llvm::Type*   GetLlvmType() const override { return llvm::Type::getIntNTy(theContext, bits); }
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override
	{
	    return builder->createBasicType("INTEGER" + std::to_string(bits), bits,
	                                    llvm::dwarf::DW_ATE_signed);
	}
    };

    using IntegerDecl = IntegerXDecl<32, TypeDecl::TK_Integer>;
    using Int64Decl = IntegerXDecl<64, TypeDecl::TK_LongInt>;

    class RealDecl : public TypeDecl
    {
    public:
	RealDecl() : TypeDecl(TK_Real) {}
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	const TypeDecl* AssignableType(const TypeDecl* ty) const override;
	unsigned        Bits() const override { return 64; }
	void            DoDump() const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Real; }
	TypeDecl*       Clone() const override { return new RealDecl(); }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
    };

    class VoidDecl : public TypeDecl
    {
    public:
	VoidDecl() : TypeDecl(TK_Void) {}
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override { return 0; }
	void            DoDump() const override;
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
	TypeDecl*   SubType() const { return baseType; }
	bool        HasLlvmType() const override { return baseType->HasLlvmType(); }
	static bool classof(const TypeDecl* e);

    protected:
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override
	{
	    return baseType->DebugType(builder);
	}
	llvm::Type* GetLlvmType() const override { return baseType->LlvmType(); }

    protected:
	TypeDecl* baseType;
    };

    class RangeBaseDecl : public CompoundDecl
    {
	using CompoundDecl::CompoundDecl;
    };

    class RangeDecl : public RangeBaseDecl
    {
    public:
	RangeDecl(Range* r, TypeDecl* base) : RangeBaseDecl(TK_Range, base), range(r)
	{
	    assert(r && "Range should be specified");
	}

    public:
	void            DoDump() const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Range; }
	bool            SameAs(const TypeDecl* ty) const override;
	int             Start() const { return range->Start(); }
	int             End() const { return range->End(); }
	TypeKind        Type() const override { return baseType->Type(); }
	bool            IsCompound() const override { return false; }
	bool            IsIntegral() const override { return true; }
	bool            IsUnsigned() const override { return Start() >= 0; }
	unsigned        Bits() const override;
	Range*          GetRange() const override { return range; }
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	const TypeDecl* AssignableType(const TypeDecl* ty) const override;

    private:
	Range* range;
    };

    class DynRangeDecl : public RangeBaseDecl
    {
    public:
	DynRangeDecl(const std::string& lName, const std::string& hName, TypeDecl* base)
	    : RangeBaseDecl(TK_DynRange, base), lowName(lName), highName(hName)
	{
	}
	static bool        classof(const TypeDecl* e) { return e->getKind() == TK_DynRange; }
	const std::string& LowName() { return lowName; }
	const std::string& HighName() { return highName; }
	void               DoDump() const override;
	TypeKind           Type() const override { return baseType->Type(); }

    private:
	std::string lowName;
	std::string highName;
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
	void                           DoDump() const override;
	bool                           SameAs(const TypeDecl* ty) const override;
	const TypeDecl*                CompatibleType(const TypeDecl* ty) const override;
	static bool                    classof(const TypeDecl* e)
	{
	    return e->getKind() >= TK_Array && e->getKind() <= TK_LastArray;
	}
	TypeDecl* Clone() const override;

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	std::vector<RangeDecl*> ranges;
    };

    class DynArrayDecl : public CompoundDecl
    {
    public:
	DynArrayDecl(TypeDecl* b, DynRangeDecl* r) : CompoundDecl(TK_DynArray, b), range(r) {}
	void            DoDump() const override;
	DynRangeDecl*   Range() { return range; }
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_DynArray; }
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;

    protected:
	llvm::Type* GetLlvmType() const override;

    private:
	DynRangeDecl* range;
    };

    struct EnumValue
    {
	EnumValue(const std::string& nm, int v) : name(nm), value(v) {}
	EnumValue(const EnumValue& e) : name(e.name), value(e.value) {}
	std::string name;
	int         value;
    };

    using EnumValues = std::vector<EnumValue>;

    class EnumDecl : public CompoundDecl
    {
    public:
	EnumDecl(TypeKind tk, const std::vector<std::string>& nmv, TypeDecl* ty) : CompoundDecl(tk, ty)
	{
	    assert(nmv.size() && "Must have names in the enum type.");
	    SetValues(nmv);
	}
	EnumDecl(const std::vector<std::string>& nmv, TypeDecl* ty) : EnumDecl(TK_Enum, nmv, ty) {}
	EnumDecl(TypeKind tk, const EnumValues& vals, TypeDecl* ty) : CompoundDecl(tk, ty), values(vals) {}

    private:
	void SetValues(const std::vector<std::string>& nmv);

    public:
	Range*            GetRange() const override { return new Range(0, values.size() - 1); }
	const EnumValues& Values() const { return values; }
	bool              IsIntegral() const override { return true; }
	bool              IsUnsigned() const override { return true; }
	bool              IsCompound() const override { return false; }
	void              DoDump() const override;
	unsigned          Bits() const override;
	static bool       classof(const TypeDecl* e) { return e->getKind() == TK_Enum; }
	bool              SameAs(const TypeDecl* ty) const override;
	TypeDecl*         Clone() const override { return new EnumDecl(kind, values, SubType()); }

    protected:
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	EnumValues values;
    };

    class BoolDecl : public EnumDecl
    {
    public:
	BoolDecl() : EnumDecl(TK_Boolean, std::vector<std::string>{ "false", "true" }, this) {}
	void        DoDump() const override;
	bool        SameAs(const TypeDecl* ty) const override { return ty == this; }
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Boolean; }

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
	void        DoDump() const override;
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Pointer; }

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
	void            DoDump() const override;
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
	void        DoDump() const override;
	bool        IsIntegral() const override { return baseType->IsIntegral(); }
	bool        IsCompound() const override { return baseType->IsCompound(); }
	bool        IsStatic() const { return isStatic; }
	bool        SameAs(const TypeDecl* ty) const override { return baseType->SameAs(ty); }
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Field; }
	            operator Access() { return access; }

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
	void        DoDump() const override;
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Variant; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
    };

    class RecordDecl : public FieldCollection
    {
    public:
	RecordDecl(TypeKind type, const std::vector<FieldDecl*>& flds, VariantDecl* v)
	    : FieldCollection(type, flds), variant(v), clonedFrom(nullptr){};
	RecordDecl(const std::vector<FieldDecl*>& flds, VariantDecl* v)
	    : FieldCollection(TK_Record, flds), variant(v), clonedFrom(nullptr){};
	void         DoDump() const override;
	size_t       Size() const override;
	VariantDecl* Variant() const { return variant; }
	bool         SameAs(const TypeDecl* ty) const override;
	static bool  classof(const TypeDecl* e) { return e->getKind() == TK_Record; }
	TypeDecl*    Clone() const override
	{
	    RecordDecl* rd = new RecordDecl(getKind(), fields, variant);
	    if (this->clonedFrom)
	    {
		rd->clonedFrom = this->clonedFrom;
	    }
	    else
	    {
		rd->clonedFrom = this;
	    }
	    return rd;
	}

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	VariantDecl*      variant;
	const RecordDecl* clonedFrom;
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

	void          DoDump() const override;
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

	void             DoDump() const override;
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
	void          DoDump() const override;
	PrototypeAST* Proto() const { return proto; }
	bool          IsCompound() const override { return false; }
	bool          SameAs(const TypeDecl* ty) const override;
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
	void        DoDump() const override;
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_File || e->getKind() == TK_Text; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
    };

    class TextDecl : public FileDecl
    {
    public:
	TextDecl() : FileDecl(TK_Text, new CharDecl) {}
	void        DoDump() const override;
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
	void            DoDump() const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Set; }
	size_t          SetWords() const { return (range->GetRange()->Size() + SetMask) >> SetPow2Bits; }
	Range*          GetRange() const override;
	void            UpdateRange(RangeDecl* r) { range = r; }
	void            UpdateSubtype(TypeDecl* ty);
	bool            SameAs(const TypeDecl* ty) const override;
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	RangeDecl* range;
    };

    class StringDecl : public ArrayDecl
    {
    public:
	StringDecl(unsigned size)
	    : ArrayDecl(
	          TK_String, new CharDecl,
	          std::vector<RangeDecl*>(1, new RangeDecl(new Range(0, size), Get<Types::IntegerDecl>())))
	{
	    assert(size > 0 && "Zero size not allowed");
	}
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_String; }
	bool            IsStringLike() const override { return true; }
	void            DoDump() const override;
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	int             Capacity() const { return Ranges()[0]->GetRange()->Size() - 1; }
    };

    class ComplexDecl : public RecordDecl
    {
    public:
	ComplexDecl()
	    : RecordDecl(
	          TK_Complex,
	          { new FieldDecl("r", Get<RealDecl>(), false), new FieldDecl("i", Get<RealDecl>(), false) },
	          nullptr)
	{
	}
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Complex; }
	bool        IsCompound() const override { return false; }
    };

    llvm::Type* GetVoidPtrType();

    void Finalize(llvm::DIBuilder* builder);

    TypeDecl* CloneWithInit(const TypeDecl* ty, ExprAST* init);
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
