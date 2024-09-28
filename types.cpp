#include "types.h"
#include "expr.h"
#include "runtime/runtime.h"
#include "schema.h"
#include "trace.h"
#include <climits>
#include <llvm/IR/LLVMContext.h>
#include <sstream>

extern llvm::Module* theModule;

namespace Types
{
    static std::vector<std::pair<TypeDecl*, llvm::TrackingMDRef>> fwdMap;

    size_t TypeDecl::Size() const
    {
	const llvm::DataLayout& dl = theModule->getDataLayout();
	return dl.getTypeAllocSize(LlvmType());
    }

    size_t TypeDecl::AlignSize() const
    {
	const llvm::DataLayout& dl = theModule->getDataLayout();
	return dl.getPrefTypeAlign(LlvmType()).value();
    }

    Range* TypeDecl::GetRange() const
    {
	ICE_IF(!IsIntegral(this), "Expected integral type for GetRange");
	switch (kind)
	{
	case TK_Char:
	    return new Range(0, UCHAR_MAX);
	case TK_Integer:
	    return new Range(INT_MIN, INT_MAX);
	default:
	    ICE("Range on unsupported type");
	}
    }

    const TypeDecl* TypeDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (SameAs(ty))
	{
	    return this;
	}
	return 0;
    }

    void TypeDecl::dump() const
    {
	DoDump();
	std::cerr << std::endl;
    }

    llvm::Type* TypeDecl::LlvmType() const
    {
	if (!lType)
	{
	    lType = GetLlvmType();
	}
	return lType;
    }

    llvm::DIType* TypeDecl::DebugType(llvm::DIBuilder* builder) const
    {
	if (!diType)
	{
	    diType = GetDIType(builder);
	}
	return diType;
    }

    void CharDecl::DoDump() const
    {
	std::cerr << "Type: Char";
    }

    llvm::Type* CharDecl::GetLlvmType() const
    {
	return llvm::Type::getInt8Ty(theContext);
    }

    llvm::DIType* CharDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	return builder->createBasicType("CHAR", 8, llvm::dwarf::DW_ATE_unsigned_char);
    }

    const TypeDecl* CharDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (*this == *ty || ty->Type() == TK_String)
	{
	    return this;
	}
	return 0;
    }

    const TypeDecl* CharDecl::AssignableType(const TypeDecl* ty) const
    {
	if (*this == *ty)
	{
	    return this;
	}
	return 0;
    }

    template<>
    const TypeDecl* IntegerDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (ty->Type() == TK_Integer)
	{
	    return this;
	}
	if (ty->Type() == TK_LongInt || ty->Type() == TK_Real || ty->Type() == TK_Complex)
	{
	    return ty;
	}

	return 0;
    }

    template<>
    const TypeDecl* IntegerDecl::AssignableType(const TypeDecl* ty) const
    {
	if (SameAs(ty))
	{
	    return ty;
	}
	return 0;
    }

    template<>
    const TypeDecl* Int64Decl::CompatibleType(const TypeDecl* ty) const
    {
	if (ty->Type() == TK_LongInt || ty->Type() == TK_Integer)
	{
	    return this;
	}
	if (ty->Type() == TK_Real)
	{
	    return ty;
	}
	if (ty->Type() == TK_Complex)
	{
	    return ty;
	}
	return 0;
    }

    template<>
    const TypeDecl* Int64Decl::AssignableType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == TK_Integer)
	{
	    return this;
	}
	return 0;
    }

    void RealDecl::DoDump() const
    {
	std::cerr << "Type: Real";
    }

    llvm::Type* RealDecl::GetLlvmType() const
    {
	return llvm::Type::getDoubleTy(theContext);
    }

    llvm::DIType* RealDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	return builder->createBasicType("REAL", 64, llvm::dwarf::DW_ATE_float);
    }

    const TypeDecl* RealDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == TK_LongInt || ty->Type() == TK_Integer)
	{
	    return this;
	}
	if (ty->Type() == TK_Complex)
	{
	    return ty;
	}
	return 0;
    }

    const TypeDecl* ComplexDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || IsNumeric(ty))
	{
	    return this;
	}
	return 0;
    }

    const TypeDecl* RealDecl::AssignableType(const TypeDecl* ty) const
    {
	return CompatibleType(ty);
    }

    void VoidDecl::DoDump() const
    {
	std::cerr << "Void";
    }

    llvm::Type* VoidDecl::GetLlvmType() const
    {
	return llvm::Type::getVoidTy(theContext);
    }

    void BoolDecl::DoDump() const
    {
	std::cerr << "Type: Bool";
    }

    llvm::Type* BoolDecl::GetLlvmType() const
    {
	return llvm::Type::getInt1Ty(theContext);
    }

    llvm::DIType* BoolDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	return builder->createBasicType("BOOLEAN", 1, llvm::dwarf::DW_ATE_boolean);
    }

    void PointerDecl::DoDump() const
    {
	std::cerr << "Pointer to: " << name;
	if (baseType)
	{
	    std::cerr << " type " << baseType;
	}
	else
	{
	    std::cerr << " (forward declared)";
	}
    }

    TypeDecl* PointerDecl::Clone() const
    {
	if (auto fwd = llvm::dyn_cast<ForwardDecl>(baseType))
	{
	    return new PointerDecl(fwd);
	}

	ICE_IF(forward, "Shouldn't be forward declared pointer here");
	return new PointerDecl(SubType());
    }

    const TypeDecl* PointerDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (SameAs(ty))
	{
	    return this;
	}
	if (auto ptrTy = llvm::dyn_cast<PointerDecl>(ty))
	{
	    if (baseType->SameAs(ptrTy->baseType))
	    {
		return this;
	    }
	    if (auto classTy = llvm::dyn_cast<ClassDecl>(ptrTy->baseType))
	    {
		if (auto thisClassTy = llvm::dyn_cast<ClassDecl>(baseType))
		{
		    if (classTy->DerivedFrom(thisClassTy))
		    {
			return this;
		    }
		}
	    }
	}
	return 0;
    }

    bool CompoundDecl::classof(const TypeDecl* e)
    {
	switch (e->getKind())
	{
	case TK_Array:
	case TK_String:
	case TK_Pointer:
	case TK_Field:
	case TK_FuncPtr:
	case TK_File:
	case TK_Text:
	case TK_Set:
	    return true;
	default:
	    break;
	}
	return false;
    }

    bool CompoundDecl::SameAs(const TypeDecl* ty) const
    {
	if (this == ty)
	{
	    return true;
	}
	// Both need to be pointers!
	if (Type() != ty->Type())
	{
	    return false;
	}
	const auto cty = llvm::dyn_cast<CompoundDecl>(ty);
	return cty && *cty->SubType() == *baseType;
    }

    llvm::Type* PointerDecl::GetLlvmType() const
    {
	return llvm::PointerType::getUnqual(baseType->LlvmType());
    }

    llvm::DIType* PointerDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	llvm::DIType* pointeeType = baseType->DebugType(builder);
	if (!pointeeType)
	{
	    return 0;
	}
	uint64_t size = Size() * CHAR_BIT;
	uint64_t align = AlignSize() * CHAR_BIT;
	return builder->createPointerType(pointeeType, size, align);
    }

    void ArrayDecl::DoDump() const
    {
	std::cerr << "Array ";
	for (auto r : ranges)
	{
	    r->DoDump();
	}
	std::cerr << " of ";
	baseType->DoDump();
    }

    llvm::Type* ArrayDecl::GetLlvmType() const
    {
	ICE_IF(!ranges.size(), "Expect ranges to contain something");
	size_t nelems = 1;
	for (auto r : ranges)
	{
	    ICE_IF(!r->GetRange()->Size(), "Expectig range to have a non-zero size!");
	    nelems *= r->GetRange()->Size();
	}

	llvm::Type* ty = baseType->LlvmType();
	ICE_IF(!nelems, "Expect number of elements to be non-zero!");
	ICE_IF(!ty, "Expected to get a type back!");
	return llvm::ArrayType::get(ty, nelems);
    }

    llvm::DIType* ArrayDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	std::vector<llvm::Metadata*> subscripts;
	for (auto r : ranges)
	{
	    Range* rr = r->GetRange();
	    subscripts.push_back(builder->getOrCreateSubrange(rr->Start(), rr->End()));
	}
	llvm::DIType* bd = baseType->DebugType(builder);
	if (!bd)
	{
	    return 0;
	}
	llvm::DINodeArray subsArray = builder->getOrCreateArray(subscripts);
	return builder->createArrayType(baseType->Size() * CHAR_BIT, baseType->AlignSize() * CHAR_BIT, bd,
	                                subsArray);
    }

    bool ArrayDecl::SameAs(const TypeDecl* ty) const
    {
	if (CompoundDecl::SameAs(ty))
	{
	    if (const auto aty = llvm::dyn_cast<ArrayDecl>(ty))
	    {
		if (ranges.size() != aty->Ranges().size())
		{
		    return false;
		}
		for (size_t i = 0; i < ranges.size(); i++)
		{
		    if (*ranges[i] != *aty->Ranges()[i])
		    {
			return false;
		    }
		}

		return true;
	    }
	}
	return false;
    }

    const TypeDecl* ArrayDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (SameAs(ty))
	{
	    return this;
	}
	if (const auto aty = llvm::dyn_cast<ArrayDecl>(ty))
	{
	    if (aty->SubType() == SubType() && ranges.size() == aty->Ranges().size())
	    {
		for (size_t i = 0; i < ranges.size(); i++)
		{
		    if (ranges[i]->Size() != aty->Ranges()[i]->Size())
		    {
			return 0;
		    }
		}
	    }
	}
	return this;
    }

    TypeDecl* ArrayDecl::Clone() const
    {
	return new Types::ArrayDecl(baseType, ranges);
    }

    llvm::Type* DynArrayDecl::GetArrayType(TypeDecl* baseType)
    {
	static llvm::Type* dynTy = 0;
	if (!dynTy)
	{
	    llvm::Type* ty = baseType->LlvmType();
	    llvm::Type* ptrTy = llvm::PointerType::getUnqual(ty);
	    llvm::Type* intTy = Get<IntegerDecl>()->LlvmType();
	    dynTy = llvm::StructType::create({ ptrTy, intTy, intTy }, "confArray");
	}
	return dynTy;
    }

    llvm::Type* DynArrayDecl::GetLlvmType() const
    {
	return GetArrayType(baseType);
    }

    void DynArrayDecl::DoDump() const
    {
	std::cerr << "Array ";
	range->DoDump();
	std::cerr << " of ";
	baseType->DoDump();
    }

    const TypeDecl* DynArrayDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (SameAs(ty))
	{
	    return this;
	}
	if (const auto aty = llvm::dyn_cast<ArrayDecl>(ty))
	{
	    if (aty->SubType() != SubType() || aty->Ranges().size() != 1)
	    {
		return 0;
	    }
	    return aty;
	}
	return 0;
    }

    void Range::DoDump() const
    {
	std::cerr << "[" << start << ".." << end << "]";
    }

    void RangeDecl::DoDump() const
    {
	std::cerr << "RangeDecl: ";
	baseType->DoDump();
	std::cerr << " ";
	range->DoDump();
    }

    bool RangeDecl::SameAs(const TypeDecl* ty) const
    {
	if (const auto rty = llvm::dyn_cast<RangeDecl>(ty))
	{
	    return rty->Type() == Type() && *range == *rty->range;
	}
	return Type() == ty->Type();
    }

    const TypeDecl* RangeBaseDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (*this == *ty)
	{
	    return this;
	}
	if (ty->Type() == Type())
	{
	    return ty;
	}
	return 0;
    }

    const TypeDecl* RangeBaseDecl::AssignableType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == Type())
	{
	    return ty;
	}
	return 0;
    }

    void DynRangeDecl::DoDump() const
    {
	std::cerr << "DynRangeDecl: ";
	baseType->DoDump();
	std::cerr << "[" << lowName << ".." << highName << "]";
    }

    void EnumDecl::SetValues(const std::vector<std::string>& nmv)
    {
	unsigned int v = 0;
	for (auto n : nmv)
	{
	    values.push_back(EnumValue(n, v));
	    v++;
	}
    }

    void EnumDecl::DoDump() const
    {
	std::cerr << "EnumDecl:";
	for (auto v : values)
	{
	    std::cerr << "   " << v.name << ": " << v.value;
	}
    }

    bool EnumDecl::SameAs(const TypeDecl* ty) const
    {
	if (const auto ety = llvm::dyn_cast<EnumDecl>(ty))
	{
	    if (ety->Type() != Type() || values.size() != ety->values.size())
	    {
		return false;
	    }
	    for (size_t i = 0; i < values.size(); i++)
	    {
		if (values[i] != ety->values[i])
		{
		    return false;
		}
	    }
	}
	else
	{
	    return false;
	}
	return true;
    }

    llvm::DIType* EnumDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	const int                    size = 32;
	const int                    align = 32;
	std::vector<llvm::Metadata*> enumerators;
	for (const auto& i : values)
	{
	    enumerators.push_back(builder->createEnumerator(i.name, i.value));
	}
	llvm::DINodeArray eltArray = builder->getOrCreateArray(enumerators);
	int               line = 0;
	return builder->createEnumerationType(0, "", 0, line, size, align, eltArray, 0);
    }

    FunctionDecl::FunctionDecl(PrototypeAST* p) : CompoundDecl(TK_Function, p->Type()), proto(p) {}

    void FunctionDecl::DoDump() const
    {
	std::cerr << "Function " << baseType;
    }

    void FieldDecl::DoDump() const
    {
	std::cerr << "Field " << name << ": ";
	baseType->DoDump();
    }

    void VariantDecl::DoDump() const
    {
	std::cerr << "Variant ";
	for (auto f : fields)
	{
	    f->DoDump();
	    std::cerr << std::endl;
	}
    }

    llvm::Type* VariantDecl::GetLlvmType() const
    {
	const llvm::DataLayout& dl = theModule->getDataLayout();
	size_t                 maxSize = 0;
	size_t                 maxSizeElt = 0;
	size_t                 maxAlign = 0;
	size_t                 maxAlignElt = 0;
	size_t                 maxAlignSize = 0;
	size_t                 elt = 0;
	for (auto f : fields)
	{
	    llvm::Type* ty = f->LlvmType();
	    if (auto pf = llvm::dyn_cast<PointerDecl>(f->SubType()))
	    {
		if (pf->IsIncomplete())
		{
		    if (!opaqueType)
		    {
			opaqueType = llvm::StructType::create(theContext, name);
		    }
		    return opaqueType;
		}
	    }
	    size_t sz = dl.getTypeAllocSize(ty);
	    size_t al = dl.getPrefTypeAlign(ty).value();
	    if (sz > maxSize)
	    {
		maxSize = sz;
		maxSizeElt = elt;
	    }
	    if (al > maxAlign || (al == maxAlign && sz > maxAlignSize))
	    {
		maxAlign = al;
		maxAlignSize = sz;
		maxAlignElt = elt;
	    }
	    elt++;
	}

	llvm::Type*              ty = fields[maxAlignElt]->LlvmType();
	std::vector<llvm::Type*> fv = { ty };
	if (maxAlignElt != maxSizeElt)
	{
	    size_t      nelems = maxSize - maxAlignSize;
	    llvm::Type* ty = llvm::ArrayType::get(Get<CharDecl>()->LlvmType(), nelems);
	    fv.push_back(ty);
	}
	return llvm::StructType::create(fv, name);
    }

    llvm::DIType* VariantDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	// TODO: Fill in.
	return 0;
    }

    void FieldCollection::EnsureSized() const
    {
	if (opaqueType && opaqueType->isOpaque())
	{
	    [[maybe_unused]] llvm::Type* ty = GetLlvmType();
	    ICE_IF(ty != opaqueType, "Expect opaqueType to be returned");
	    ICE_IF(opaqueType->isOpaque(), "Expect opaqueness to have gone away");
	}
    }

    // This is a very basic algorithm, but I think it's good enough for
    // most structures - there's unlikely to be a HUGE number of them.
    int FieldCollection::Element(const std::string& name) const
    {
	int i = 0;
	for (auto f : fields)
	{
	    // Check for special record type
	    if (f->Name() == "")
	    {
		auto rd = llvm::dyn_cast<RecordDecl>(f->SubType());
		ICE_IF(!rd, "Expected record declarataion here!");
		if (rd->Element(name) >= 0)
		{
		    return i;
		}
	    }
	    if (f->Name() == name)
	    {
		return i;
	    }
	    i++;
	}
	return -1;
    }

    bool FieldCollection::SameAs(const TypeDecl* ty) const
    {
	if (Type() != ty->Type())
	{
	    return false;
	}

	if (const auto fty = llvm::dyn_cast<FieldCollection>(ty))
	{
	    if (fields.size() != fty->fields.size())
	    {
		return false;
	    }
	    for (size_t i = 0; i < fields.size(); i++)
	    {
		if (fields[i] != fty->fields[i])
		{
		    return false;
		}
	    }
	    return true;
	}
	return false;
    }

    size_t RecordDecl::Size() const
    {
	EnsureSized();
	return TypeDecl::Size();
    }

    void RecordDecl::DoDump() const
    {
	std::cerr << "Record ";
	for (auto f : fields)
	{
	    f->DoDump();
	    std::cerr << std::endl;
	}
	if (variant)
	{
	    variant->DoDump();
	}
    }

    llvm::Type* RecordDecl::GetLlvmType() const
    {
	if (clonedFrom)
	{
	    return clonedFrom->LlvmType();
	}
	std::vector<llvm::Type*> fv;
	for (auto f : fields)
	{
	    if (auto pf = llvm::dyn_cast_or_null<PointerDecl>(f->SubType()))
	    {
		if (pf->IsIncomplete() || !HasLlvmType(f))
		{
		    if (!opaqueType)
		    {
			opaqueType = llvm::StructType::create(theContext, name);
		    }
		    return opaqueType;
		}
	    }
	    fv.push_back(f->LlvmType());
	}
	if (variant)
	{
	    fv.push_back(variant->LlvmType());
	}
	if (opaqueType)
	{
	    opaqueType->setBody(fv);
	    return opaqueType;
	}
	if (fv.empty())
	{
	    fv.push_back(llvm::Type::getInt8Ty(theContext));
	}
	return llvm::StructType::create(fv, name);
    }

    llvm::DIType* RecordDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	std::vector<llvm::Metadata*> eltTys;

	// TODO: Add unit and name (if available).
	llvm::DIScope* scope = 0;
	llvm::DIFile*  unit = 0;
	int            lineNo = 0;
	int            index = 0;

	llvm::StructType*         st = llvm::cast<llvm::StructType>(LlvmType());
	const llvm::DataLayout&   dl = theModule->getDataLayout();
	const llvm::StructLayout* sl = 0;
	if (!st->isOpaque())
	{
	    sl = dl.getStructLayout(st);
	}

	// TODO: Need to deal with recursive types here...
	for (auto f : fields)
	{
	    llvm::DIType* d = 0;
	    size_t        size = 0;
	    size_t        align = 0;
	    if (auto pf = llvm::dyn_cast<PointerDecl>(f->SubType()))
	    {
		if (pf->IsForward() && !pf->DiType())
		{
		    std::string fullname = "";
		    size = pf->SubType()->Size();
		    align = pf->SubType()->AlignSize();
		    d = builder->createReplaceableCompositeType(llvm::dwarf::DW_TAG_structure_type, "", scope,
		                                                unit, lineNo, 0, size, align,
		                                                llvm::DINode::FlagFwdDecl, fullname);
		    pf->SubType()->DiType(d);
		    fwdMap.push_back(std::make_pair(pf->SubType(), llvm::TrackingMDRef(d)));
		    size = pf->Size();
		    align = pf->AlignSize();
		    d = builder->createPointerType(d, size, align);
		    pf->DiType(d);
		}
		else
		{
		    d = pf->DiType();
		}
	    }
	    if (!d)
	    {
		d = f->DebugType(builder);
		ICE_IF(!d, "Expected debug type here");
	    }
	    size = f->Size() * CHAR_BIT;
	    align = f->AlignSize() * CHAR_BIT;
	    size_t offsetInBits = 0;
	    if (sl)
	    {
		offsetInBits = sl->getElementOffsetInBits(index);
	    }
	    d = builder->createMemberType(scope, f->Name(), unit, lineNo, size, align, offsetInBits,
	                                  llvm::DINode::FlagZero, d);
	    index++;
	    eltTys.push_back(d);
	}

	if (diType)
	{
	    return diType;
	}
	llvm::DINodeArray elements = builder->getOrCreateArray(eltTys);

	std::string   name = "";
	uint64_t      size = Size() * CHAR_BIT;
	uint64_t      align = AlignSize() * CHAR_BIT;
	llvm::DIType* derivedFrom = 0;
	return builder->createStructType(scope, name, unit, lineNo, size, align, llvm::DINode::FlagZero,
	                                 derivedFrom, elements);
    }

    bool RecordDecl::SameAs(const TypeDecl* ty) const
    {
	if (this == ty || this->clonedFrom == ty)
	{
	    return true;
	}
	if (auto rd = llvm::dyn_cast<RecordDecl>(ty))
	{
	    if (rd->clonedFrom == this)
	    {
		return true;
	    }
	}
	return false;
    }

    ClassDecl::ClassDecl(const std::string& nm, const std::vector<FieldDecl*>& flds,
                         const std::vector<MemberFuncDecl*> mf, VariantDecl* v, ClassDecl* base)
        : FieldCollection(TK_Class, flds), baseobj(base), variant(v), vtableType(0)
    {
	Name(nm);
	if (baseobj)
	{
	    membfuncs = baseobj->membfuncs;
	}

	std::vector<VarDef> self = { VarDef("self", this, VarDef::Flags::Reference) };
	for (auto i : mf)
	{
	    if (!i->IsStatic())
	    {
		i->Proto()->SetBaseObj(this);
		i->Proto()->AddExtraArgsFirst(self);
		i->Proto()->SetHasSelf(true);
	    }
	    i->LongName(name + "$" + i->Proto()->Name());
	    bool found = false;
	    for (auto& j : membfuncs)
	    {
		if (j->Proto()->Name() == i->Proto()->Name())
		{
		    j = i;
		    found = true;
		    break;
		}
	    }
	    if (!found)
	    {
		membfuncs.push_back(i);
	    }
	}
    }

    size_t ClassDecl::Size() const
    {
	EnsureSized();
	return TypeDecl::Size();
    }

    void ClassDecl::DoDump() const
    {
	std::cerr << "Object: " << Name() << " ";
	for (auto f : fields)
	{
	    f->DoDump();
	    std::cerr << std::endl;
	}
	if (variant)
	{
	    variant->DoDump();
	}
    }

    size_t ClassDecl::MembFuncCount() const
    {
	return membfuncs.size();
    }

    int ClassDecl::MembFunc(const std::string& nm) const
    {
	for (size_t i = 0; i < membfuncs.size(); i++)
	{
	    if (membfuncs[i]->Proto()->Name() == nm)
	    {
		return i;
	    }
	}
	return -1;
    }

    MemberFuncDecl* ClassDecl::GetMembFunc(size_t index) const
    {
	ICE_IF(index >= membfuncs.size(), "Expected index to be in range");
	return membfuncs[index];
    }

    size_t ClassDecl::NumVirtFuncs() const
    {
	size_t count = 0;
	for (auto mf : membfuncs)
	{
	    count += mf->IsVirtual() || mf->IsOverride();
	}
	return count;
    }

    llvm::Type* ClassDecl::VTableType(bool opaque) const
    {
	if (vtableType && (opaque || !vtableType->isOpaque()))
	{
	    return vtableType;
	}

	if (baseobj)
	{
	    (void)baseobj->VTableType(opaque);
	}

	std::vector<llvm::Type*> vt;
	bool                     needed = false;
	int                      index = 0;
	for (auto m : membfuncs)
	{
	    if (m->IsVirtual())
	    {
		if (m->VirtIndex() == -1)
		{
		    m->VirtIndex(index);
		}
		index++;
		needed = true;
	    }
	    else if (m->IsOverride())
	    {
		int elem = (baseobj) ? baseobj->MembFunc(m->Proto()->Name()) : -1;
		if (elem < 0)
		{
		    std::cerr << "Overriding function " + m->Proto()->Name() +
		                     " that is not a virtual function in the baseclass!";
		    return 0;
		}
		// We need to continue digging here for multi-level functions
		MemberFuncDecl* mf = baseobj->GetMembFunc(elem);
		m->VirtIndex(mf->VirtIndex());
		needed = true;
		index++;
	    }

	    if (!opaque && (m->IsOverride() || m->IsVirtual()))
	    {
		FuncPtrDecl* fp = new FuncPtrDecl(m->Proto());
		vt.push_back(fp->LlvmType());
	    }
	}
	if (!needed)
	{
	    return (baseobj) ? baseobj->VTableType(opaque) : 0;
	}

	if (!vtableType)
	{
	    vtableType = llvm::StructType::create(theContext, "vtable_" + Name());
	}
	if (!opaque)
	{
	    ICE_IF(vt.empty(), "Expected some functions here...");
	    vtableType->setBody(vt);
	}
	return vtableType;
    }

    int ClassDecl::Element(const std::string& name) const
    {
	int b = baseobj ? baseobj->FieldCount() : 0;
	// Shadowing overrides outer elemnts
	int elem = FieldCollection::Element(name);
	if (elem >= 0)
	{
	    elem += b;
	    if (VTableType(true))
	    {
		elem++;
	    }
	    return elem;
	}
	return (baseobj) ? baseobj->Element(name) : -1;
    }

    const FieldDecl* ClassDecl::GetElement(unsigned int n, std::string& objname) const
    {
	unsigned b = baseobj ? baseobj->FieldCount() : 0;
	if (baseobj && n < b + (baseobj->VTableType(true) ? 1 : 0))
	{
	    return baseobj->GetElement(n, objname);
	}
	ICE_IF(n > b + fields.size(), "Out of range field");
	objname = Name();
	return fields[n - (b + (VTableType(true) ? 1 : 0))];
    }

    const FieldDecl* ClassDecl::GetElement(unsigned int n) const
    {
	std::string objname;
	return GetElement(n, objname);
    }

    int ClassDecl::FieldCount() const
    {
	return fields.size() + (baseobj ? baseobj->FieldCount() : 0);
    }

    const TypeDecl* ClassDecl::DerivedFrom(const TypeDecl* ty) const
    {
	ClassDecl* cd = baseobj;
	while (cd)
	{
	    if (ty == cd)
		return cd;
	    cd = cd->baseobj;
	}
	return 0;
    }

    const TypeDecl* ClassDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (*ty == *this)
	{
	    return this;
	}
	if (const auto cd = llvm::dyn_cast<ClassDecl>(ty))
	{
	    return (cd->baseobj) ? CompatibleType(cd->baseobj) : 0;
	}
	return 0;
    }

    llvm::Type* ClassDecl::GetLlvmType() const
    {
	std::vector<llvm::Type*> fv;

	int vtableoffset = 0;
	if (VTableType(true))
	{
	    vtableoffset = 1;
	    fv.push_back(llvm::PointerType::getUnqual(vtableType));
	}

	int fc = FieldCount();
	for (int i = 0; i < fc; i++)
	{
	    const FieldDecl* f = GetElement(i + vtableoffset);

	    ICE_IF(llvm::isa<MemberFuncDecl>(f->SubType()), "Should not have member functions");

	    if (!f->IsStatic())
	    {
		if (auto pd = llvm::dyn_cast<PointerDecl>(f->SubType()))
		{
		    if (pd->IsIncomplete() && !HasLlvmType(pd))
		    {
			if (!opaqueType)
			{
			    opaqueType = llvm::StructType::create(theContext, Name());
			}
			return opaqueType;
		    }
		}
		fv.push_back(f->LlvmType());
	    }
	}
	if (variant)
	{
	    fv.push_back(variant->LlvmType());
	}
	if (opaqueType)
	{
	    opaqueType->setBody(fv);
	    return opaqueType;
	}
	if (!fv.size())
	{
	    llvm::StructType* ty = llvm::StructType::create(theContext, Name());
	    return ty;
	}
	return llvm::StructType::create(fv, Name());
    }

    llvm::DIType* ClassDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	// TODO: Fill in.
	return 0;
    }

    void MemberFuncDecl::DoDump() const
    {
	std::cerr << "Member function ";
	proto->DoDump();
    }

    void FuncPtrDecl::DoDump() const
    {
	std::cerr << "FunctionPtr ";
    }

    llvm::Type* FuncPtrDecl::GetLlvmType() const
    {
	llvm::Type*              resty = proto->Type()->LlvmType();
	std::vector<llvm::Type*> argTys;
	for (auto v : proto->Args())
	{
	    llvm::Type* ty = v.Type()->LlvmType();
	    if (v.IsRef() || IsCompound(v.Type()))
	    {
		ty = llvm::PointerType::getUnqual(ty);
	    }
	    argTys.push_back(ty);
	}
	llvm::Type* ty = llvm::FunctionType::get(resty, argTys, false);
	return llvm::PointerType::getUnqual(ty);
    }

    llvm::DIType* FuncPtrDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	// TODO: Implement this.
	return 0;
    }

    bool FuncPtrDecl::SameAs(const TypeDecl* ty) const
    {
	if (const auto fty = llvm::dyn_cast<FuncPtrDecl>(ty))
	{
	    return *proto == *fty->proto;
	}
	if (const auto fty = llvm::dyn_cast<FunctionDecl>(ty))
	{
	    return *proto == *fty->Proto();
	}
	return false;
    }

    /*
     * A "file" is represented by:
     * struct
     * {
     *    int32     handle;
     *    baseType *ptr;
     *    int32     recordSize;
     *    int32     isText;
     * };
     *
     * The translation from handle to actual file is done inside the C runtime
     * part.
     *
     * Note that this arrangement has to agree with the runtime.c definition.
     */
    llvm::Type* FileDecl::GetLlvmType() const
    {
	llvm::Type*              ty = llvm::PointerType::getUnqual(baseType->LlvmType());
	llvm::Type*              intTy = Get<IntegerDecl>()->LlvmType();
	std::vector<llvm::Type*> fv = { intTy, ty, intTy, intTy };
	return llvm::StructType::create(fv, Type() == TK_Text ? "text" : "file");
    }

    llvm::DIType* FileDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	// TODO: Fill in.
	return 0;
    }

    void FileDecl::DoDump() const
    {
	std::cerr << "File of ";
	baseType->DoDump();
    }

    void TextDecl::DoDump() const
    {
	std::cerr << "Text ";
    }

    SetDecl::SetDecl(TypeKind k, RangeBaseDecl* r, TypeDecl* ty) : CompoundDecl(k, ty), range(r)
    {
	static_assert(sizeof(ElemType) * CHAR_BIT == SetBits && "Set bits mismatch");
	static_assert(1 << SetPow2Bits == SetBits && "Set pow2 mismatch");
	static_assert(SetMask == SetBits - 1 && "Set pow2 mismatch");
	if (r && !llvm::isa<SchemaRange>(r))
	{
	    ICE_IF(r->GetRange()->Size() > MaxSetSize, "Set too large");
	}
    }

    llvm::Type* SetDecl::GetLlvmType() const
    {
	ICE_IF(!range, "Range not set");
	ICE_IF(range->GetRange()->Size() > MaxSetSize, "Set too large");
	auto        ity = llvm::dyn_cast<llvm::IntegerType>(Get<IntegerDecl>()->LlvmType());
	ICE_IF(!ity, "Couldn't make llvm-type");
	llvm::Type* ty = llvm::ArrayType::get(ity, SetWords());
	return ty;
    }

    llvm::DIType* SetDecl::GetDIType(llvm::DIBuilder* builder) const
    {
	std::vector<llvm::Metadata*> subscripts;
	Range*                       rr = range->GetRange();
	subscripts.push_back(builder->getOrCreateSubrange(rr->Start(), rr->End()));
	llvm::DIType*     bd = builder->createBasicType("INTEGER", 32, llvm::dwarf::DW_ATE_unsigned);
	llvm::DINodeArray subsArray = builder->getOrCreateArray(subscripts);
	return builder->createArrayType(baseType->Size() * CHAR_BIT, baseType->AlignSize() * CHAR_BIT, bd,
	                                subsArray);
    }

    void SetDecl::DoDump() const
    {
	std::cerr << "Set of ";
	if (!range)
	{
	    std::cerr << "[Unknown]";
	}
	else
	{
	    range->DoDump();
	}
    }

    void SetDecl::UpdateSubtype(TypeDecl* ty)
    {
	ICE_IF(baseType, "Expected to not have a subtype yet...");
	baseType = ty;
    }

    Range* SetDecl::GetRange() const
    {
	if (range)
	{
	    return range->GetRange();
	}
	return 0;
    }

    const TypeDecl* SetDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (const auto sty = llvm::dyn_cast<SetDecl>(ty))
	{
	    if (*baseType == *sty->baseType)
	    {
		return this;
	    }
	}
	return 0;
    }

    bool SetDecl::SameAs(const TypeDecl* ty) const
    {
	if (CompoundDecl::SameAs(ty))
	{
	    if (const auto sty = llvm::dyn_cast<SetDecl>(ty))
	    {
		return sty->range && *range == *sty->range;
	    }
	}
	return false;
    }

    void StringDecl::DoDump() const
    {
	std::cerr << "String[";
	Ranges()[0]->DoDump();
	std::cerr << "]";
    }

    const TypeDecl* StringDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == TK_Char)
	{
	    return this;
	}
	if (ty->Type() == TK_String)
	{
	    if (llvm::dyn_cast<StringDecl>(ty)->Capacity() > Capacity())
	    {
		return ty;
	    }
	    return this;
	}
	if (ty->Type() == TK_Array)
	{
	    if (const auto aty = llvm::dyn_cast<ArrayDecl>(ty))
	    {
		if (aty->Ranges().size() == 1)
		{
		    return this;
		}
	    }
	}
	return 0;
    }

    // Void pointer is not a "pointer to void", but a "pointer to Int8".
    llvm::Type* GetVoidPtrType()
    {
	llvm::Type* base = llvm::IntegerType::getInt8Ty(theContext);
	return llvm::PointerType::getUnqual(base);
    }

    bool IsNumeric(const TypeDecl* t)
    {
	switch (t->Type())
	{
	case TypeDecl::TK_Integer:
	case TypeDecl::TK_LongInt:
	case TypeDecl::TK_Real:
	case TypeDecl::TK_Complex:
	    return true;
	default:
	    return false;
	}
    }

    bool IsStringLike(const TypeDecl* t)
    {
	switch (t->Type())
	{
	case TypeDecl::TK_Char:
	case TypeDecl::TK_String:
	    return true;
	    break;

	case TypeDecl::TK_Array:
	    return IsCharArray(t);

	default:
	    return false;
	}
    }

    bool IsIntegral(const TypeDecl* t)
    {
	switch (t->Type())
	{
	case TypeDecl::TK_Char:
	case TypeDecl::TK_Integer:
	case TypeDecl::TK_LongInt:
	case TypeDecl::TK_Range:
	case TypeDecl::TK_DynRange:
	case TypeDecl::TK_Enum:
	case TypeDecl::TK_Boolean:
	    return true;

	default:
	    return false;
	}
    }

    bool IsUnsigned(const TypeDecl* t)
    {
	switch (t->Type())
	{
	case TypeDecl::TK_Char:
	case TypeDecl::TK_Enum:
	case TypeDecl::TK_Boolean:
	    return true;
	case TypeDecl::TK_Range:
	{
	    auto r = llvm::cast<RangeDecl>(t);
	    return r->Start() >= 0;
	}
	default:
	    return false;
	}
    }

    bool IsCompound(const TypeDecl* t)
    {
	switch (t->Type())
	{
	case TypeDecl::TK_Array:
	case TypeDecl::TK_String:
	case TypeDecl::TK_DynArray:
	case TypeDecl::TK_Record:
	case TypeDecl::TK_Class:
	    return true;

	default:
	    return false;
	}
    }

    bool HasLlvmType(const TypeDecl* t)
    {
	switch (t->Type())
	{
	case TypeDecl::TK_MemberFunc:
	case TypeDecl::TK_Forward:
	case TypeDecl::TK_Function:
	    return false;

	case TypeDecl::TK_Variant:
	case TypeDecl::TK_Record:
	case TypeDecl::TK_Class:
	    return t->lType;

	case TypeDecl::TK_String:
	case TypeDecl::TK_FuncPtr:
	case TypeDecl::TK_File:
	case TypeDecl::TK_Text:
	case TypeDecl::TK_Set:
	case TypeDecl::TK_Array:
	case TypeDecl::TK_Field:
	case TypeDecl::TK_Pointer:
	{
	    auto p = llvm::cast<CompoundDecl>(t);
	    return HasLlvmType(p->SubType());
	}

	default:
	    return true;
	}
    }

    bool IsCharArray(const TypeDecl* t)
    {
	if (auto at = llvm::dyn_cast<ArrayDecl>(t))
	{
	    return llvm::isa<CharDecl>(at->SubType());
	}
	auto dt = llvm::dyn_cast<DynArrayDecl>(t);
	return dt && llvm::isa<CharDecl>(dt->SubType());
    }

    static RangeDecl* MakeRange(int64_t s, int64_t e)
    {
	return new RangeDecl(new Range(s, e), Get<IntegerDecl>());
    }

    TypeDecl* GetTimeStampType()
    {
	static TypeDecl* timeStampType;
	if (!timeStampType)
	{
	    // DateValid, TimeValid, Year, Month, Day, Hour, Minute, Second
	    std::vector<FieldDecl*> fields = {
		new FieldDecl("DateValid", Get<BoolDecl>(), false),
		new FieldDecl("TimeValid", Get<BoolDecl>(), false),
		new FieldDecl("Year", Get<IntegerDecl>(), false),
		new FieldDecl("Month", MakeRange(1, 12), false),
		new FieldDecl("Day", MakeRange(1, 31), false),
		new FieldDecl("Hour", MakeRange(0, 23), false),
		new FieldDecl("Minute", MakeRange(0, 59), false),
		new FieldDecl("Second", MakeRange(0, 61), false),
		new FieldDecl("MicroSecond", MakeRange(0, 999999), false),
	    };
	    timeStampType = new RecordDecl(fields, nullptr);
	    ICE_IF(sizeof(TimeStamp) != timeStampType->Size(),
	           "Runtime and Pascal TimeStamp type should match in size");
	}
	return timeStampType;
    }

    TypeDecl* GetBindingType()
    {
	static TypeDecl* bindingType;
	if (!bindingType)
	{
	    std::vector<FieldDecl*> fields = {
		new FieldDecl("Bound", Get<BoolDecl>(), false),
		new FieldDecl("Name", Get<StringDecl>(255), false),
	    };
	    bindingType = new RecordDecl(fields, nullptr);
	    ICE_IF(sizeof(BindingType) != bindingType->Size(),
	           "Runtime and Pascal Binding type should match in size");
	}
	return bindingType;
    }

    void Finalize(llvm::DIBuilder* builder)
    {
	for (auto t : fwdMap)
	{
	    llvm::DIType* ty = llvm::cast<llvm::DIType>(t.second);

	    t.first->ResetDebugType();
	    llvm::DIType* newTy = t.first->DebugType(builder);

	    builder->replaceTemporary(llvm::TempDIType(ty), newTy);
	}
    }

    TypeDecl* CloneWithInit(const TypeDecl* ty, ExprAST* init)
    {
	TypeDecl* newTy = ty->Clone();
	newTy->SetInit(init);
	return newTy;
    }
} // namespace Types

bool operator==(const Types::TypeDecl& lty, const Types::TypeDecl& rty)
{
    return lty.SameAs(&rty);
}

bool operator==(const Types::Range& a, const Types::Range& b)
{
    return (a.Start() == b.Start() && a.End() == b.End());
}

bool operator==(const Types::EnumValue& a, const Types::EnumValue& b)
{
    return (a.value == b.value && a.name == b.name);
}
