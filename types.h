#ifndef TYPES_H
#define TYPES_H

#include <iostream>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Type.h>
#include <string>

#include "utils.h"

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
    // Internal type, used to write enum values as strings.
    TypeDecl* GetEnumToStrType();

    bool IsNumeric(const TypeDecl* t);
    bool IsCharArray(const TypeDecl* t);
    bool IsStringLike(const TypeDecl* t);
    bool IsIntegral(const TypeDecl* t);
    bool IsUnsigned(const TypeDecl* t);
    bool IsCompound(const TypeDecl* t);
    bool HasLlvmType(const TypeDecl* t);

    // Range is either created by the user, or calculated on basetype
    class Range
    {
    public:
	Range(int64_t s, int64_t e) : start(s), end(e)
	{
	    ICE_IF((e - s) < 0, "Range should have start before end.");
	}

    public:
	int64_t  Start() const { return start; }
	int64_t  End() const { return end; }
	uint64_t Size() const { return (uint64_t)(end - start) + 1; }
	void     DoDump() const;

    private:
	int64_t start;
	int64_t end;
    };

    class TypeDecl
    {
	friend bool HasLlvmType(const TypeDecl* t);

    public:
	enum TypeKind
	{
	    TK_Char,
	    TK_Integer,
	    TK_LongInt,
	    TK_Real,
	    TK_Void,
	    TK_Array,
	    TK_SchArray,
	    TK_String,
	    TK_LastArray,
	    TK_DynArray,
	    TK_Range,
	    TK_DynRange,
	    TK_SchRange,
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
	    TK_SchSet,
	    TK_Variant,
	    TK_Class,
	    TK_MemberFunc,
	    TK_Forward,
	    TK_Complex,
	};

	TypeDecl(TypeKind k) : kind(k), lType(0), diType(0), name(""), init(0) {}

	virtual TypeKind Type() const { return kind; }
	virtual ~TypeDecl() {}
	virtual Range*          GetRange() const;
	virtual bool            SameAs(const TypeDecl* ty) const { return kind == ty->Type(); }
	virtual const TypeDecl* CompatibleType(const TypeDecl* ty) const;
	virtual const TypeDecl* AssignableType(const TypeDecl* ty) const { return CompatibleType(ty); }
	llvm::Type*             LlvmType() const;
	llvm::DIType*           DebugType(llvm::DIBuilder* builder) const;
	llvm::DIType*           DiType() const { return diType; }
	void                    DiType(llvm::DIType* d) const { diType = d; }
	void                    ResetDebugType() const { diType = 0; }
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
	    ICE_IF(init, "Don't set init twice");
	    init = i;
	};
	virtual TypeDecl* Clone() const { ICE("Unimplemented clone of type"); }

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
	bool SameAs(const TypeDecl* ty) const override { return false; }
	void DoDump() const override { std::cerr << std::string("Forward ") << Name(); }
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Forward; }

    protected:
	llvm::Type*   GetLlvmType() const override { ICE("No llvm type for forward decls"); }
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override
	{
	    ICE("No debug type for forward decls");
	}
    };

    class CharDecl : public TypeDecl
    {
    public:
	CharDecl() : TypeDecl(TK_Char) {}
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	const TypeDecl* AssignableType(const TypeDecl* ty) const override;
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
	TypeDecl*   SubType() const { return baseType; }
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

    public:
	const TypeDecl* AssignableType(const TypeDecl* ty) const override;
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	virtual size_t  RangeSize() const = 0;
	static bool     classof(const TypeDecl* e)
	{
	    return e->getKind() == TK_Range || e->getKind() == TK_DynRange || e->getKind() == TK_SchRange;
	}
    };

    class RangeDecl : public RangeBaseDecl
    {
    public:
	RangeDecl(Range* r, TypeDecl* base) : RangeBaseDecl(TK_Range, base), range(r)
	{
	    ICE_IF(!r, "Range should be specified");
	}

    public:
	void        DoDump() const override;
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Range; }
	bool        SameAs(const TypeDecl* ty) const override;
	int         Start() const { return range->Start(); }
	int         End() const { return range->End(); }
	size_t      RangeSize() const override { return range->Size(); }
	TypeKind    Type() const override { return baseType->Type(); }
	Range*      GetRange() const override { return range; }

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
	size_t             RangeSize() const override
	{
	    ICE("Huh? Dynamic range size?");
	    return 0;
	}
	TypeKind           Type() const override { return baseType->Type(); }
	void               DoDump() const override;

    private:
	std::string lowName;
	std::string highName;
    };

    class ArrayDecl : public CompoundDecl
    {
    public:
	ArrayDecl(TypeDecl* b, const std::vector<RangeBaseDecl*>& r) : CompoundDecl(TK_Array, b), ranges(r)
	{
	    ICE_IF(r.empty(), "Empty range not allowed");
	}
	ArrayDecl(TypeKind tk, TypeDecl* b, const std::vector<RangeBaseDecl*>& r)
	    : CompoundDecl(tk, b), ranges(r)
	{
	    ICE_IF(tk != TK_String && tk != TK_SchArray, "Expected this to be a string or schema array...");
	    ICE_IF(r.empty(), "Empty range not allowed");
	}
	const std::vector<RangeBaseDecl*>& Ranges() const { return ranges; }
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
	std::vector<RangeBaseDecl*> ranges;
    };

    class DynArrayDecl : public CompoundDecl
    {
    public:
	DynArrayDecl(TypeDecl* b, DynRangeDecl* r) : CompoundDecl(TK_DynArray, b), range(r) {}
	void               DoDump() const override;
	DynRangeDecl*      Range() { return range; }
	static bool        classof(const TypeDecl* e) { return e->getKind() == TK_DynArray; }
	const TypeDecl*    CompatibleType(const TypeDecl* ty) const override;
	static llvm::Type* GetArrayType(TypeDecl* baseType);

    protected:
	llvm::Type* GetLlvmType() const override;

    private:
	DynRangeDecl* range;
    };

    class EnumDecl : public CompoundDecl
    {
    public:
	struct EnumValue
	{
	    EnumValue(const std::string& nm, int v) : name(nm), value(v) {}
	    EnumValue(const EnumValue& e) : name(e.name), value(e.value) {}
	    std::string name;
	    int         value;
	};

	using EnumValues = std::vector<EnumValue>;
	static int staticID;
	EnumDecl(TypeKind tk, const std::vector<std::string>& nmv, TypeDecl* ty) : CompoundDecl(tk, ty)
	{
	    ICE_IF(nmv.empty(), "Must have names in the enum type.");
	    SetValues(nmv);
	}
	EnumDecl(const std::vector<std::string>& nmv, TypeDecl* ty) : EnumDecl(TK_Enum, nmv, ty)
	{
	    uniqueID = staticID++;
	}
	EnumDecl(TypeKind tk, const EnumValues& vals, TypeDecl* ty) : CompoundDecl(tk, ty), values(vals)
	{
	    uniqueID = staticID++;
	}

    private:
	void SetValues(const std::vector<std::string>& nmv);

    public:
	Range*            GetRange() const override { return new Range(0, values.size() - 1); }
	const EnumValues& Values() const { return values; }
	void              DoDump() const override;
	static bool       classof(const TypeDecl* e) { return e->getKind() == TK_Enum; }
	bool              SameAs(const TypeDecl* ty) const override;
	TypeDecl*         Clone() const override { return new EnumDecl(kind, values, SubType()); }
	int               UniqueId() { return uniqueID; }

    protected:
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	EnumValues values;
	int        uniqueID;
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
	    ICE_IF(!t, "Type should be non-NULL");
	    baseType = t;
	    incomplete = false;
	}
	bool            IsIncomplete() const { return incomplete; }
	bool            IsForward() const { return forward; }
	void            DoDump() const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Pointer; }
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	TypeDecl*       Clone() const override;

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
	    ICE_IF(n >= fields.size(), "Out of range field");
	    return fields[n];
	}
	void        EnsureSized() const;
	virtual int FieldCount() const { return fields.size(); }
	bool        SameAs(const TypeDecl* ty) const override;
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
	const TypeDecl*  DerivedFrom(const TypeDecl* ty) const;

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
	FuncPtrDecl(const PrototypeAST* func) : CompoundDecl(TK_FuncPtr, 0), proto(func) {}
	void                DoDump() const override;
	const PrototypeAST* Proto() const { return proto; }
	bool                SameAs(const TypeDecl* ty) const override;
	static bool         classof(const TypeDecl* e) { return e->getKind() == TK_FuncPtr; }

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;

    private:
	const PrototypeAST* proto;
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
	TextDecl() : FileDecl(TK_Text, Get<CharDecl>()) {}
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
	SetDecl(RangeBaseDecl* r, TypeDecl* ty) : SetDecl(TK_Set, r, ty) {}
	SetDecl(TypeKind k, RangeBaseDecl* r, TypeDecl* ty);
	void            DoDump() const override;
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_Set || e->getKind() == TK_SchSet; }
	size_t          SetWords() const { return (range->GetRange()->Size() + SetMask) >> SetPow2Bits; }
	Range*          GetRange() const override;
	void            UpdateRange(RangeDecl* r) { range = r; }
	void            UpdateSubtype(TypeDecl* ty);
	bool            SameAs(const TypeDecl* ty) const override;
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;

    protected:
	llvm::Type*   GetLlvmType() const override;
	llvm::DIType* GetDIType(llvm::DIBuilder* builder) const override;
	RangeBaseDecl* range;
    };

    class StringDecl : public ArrayDecl
    {
    public:
	StringDecl(unsigned size)
	    : ArrayDecl(
	          TK_String, Get<CharDecl>(),
	          std::vector<RangeBaseDecl*>(1, new RangeDecl(new Range(0, size + 1), Get<IntegerDecl>())))
	{
	    ICE_IF(!size, "Zero size not allowed");
	}
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_String; }
	void            DoDump() const override;
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	int             Capacity() const { return Ranges()[0]->GetRange()->Size() - 2; }
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
	const TypeDecl* CompatibleType(const TypeDecl* ty) const override;
	static bool     classof(const TypeDecl* e) { return e->getKind() == TK_Complex; }
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

bool        operator==(const Types::EnumDecl::EnumValue& a, const Types::EnumDecl::EnumValue& b);
inline bool operator!=(const Types::EnumDecl::EnumValue& a, const Types::EnumDecl::EnumValue& b)
{
    return !(a == b);
}

#endif
