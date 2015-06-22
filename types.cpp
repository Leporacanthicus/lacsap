#include "types.h"
#include "expr.h"
#include "trace.h"
#include <llvm/IR/LLVMContext.h>
#include <sstream>
#include <climits>

extern llvm::Module* theModule;

namespace Types
{
/* Static variables in Types. */
    static TypeDecl* voidType = 0;
    static TextDecl* textType = 0;
    static StringDecl* strType = 0;

    llvm::Type* ErrorT(const std::string& msg)
    {
	std::cerr << msg << std::endl;
	return 0;
    }

    llvm::Type* GetType(TypeDecl::TypeKind type)
    {
	switch(type)
	{
	case TypeDecl::TK_Enum:
	case TypeDecl::TK_Integer:
	    return llvm::Type::getInt32Ty(llvm::getGlobalContext());

	case TypeDecl::TK_Int64:
	    return llvm::Type::getInt64Ty(llvm::getGlobalContext());

	case TypeDecl::TK_Real:
	    return llvm::Type::getDoubleTy(llvm::getGlobalContext());

	case TypeDecl::TK_Char:
	    return llvm::Type::getInt8Ty(llvm::getGlobalContext());

	case TypeDecl::TK_Boolean:
	    return llvm::Type::getInt1Ty(llvm::getGlobalContext());

	case TypeDecl::TK_Void:
	    return llvm::Type::getVoidTy(llvm::getGlobalContext());

	default:
	    assert(0 && "Not a known type...");
	    break;
	}
	return 0;
    }
    static const char* TypeToStr(TypeDecl::TypeKind t)
    {
	switch(t)
	{
	case TypeDecl::TK_Integer:
	    return "Integer";
	case TypeDecl::TK_Int64:
	    return "Int64";
	case TypeDecl::TK_Real:
	    return "Real";
	case TypeDecl::TK_Char:
	    return "Char";
	case TypeDecl::TK_Boolean:
	    return "Boolean";
	case TypeDecl::TK_Array:
	    return "Array";
	case TypeDecl::TK_Function:
	    return "Function";
	case TypeDecl::TK_Record:
	    return "Record";
	case TypeDecl::TK_Set:
	    return "Set";
	case TypeDecl::TK_Range:
	    return "Range";
	case TypeDecl::TK_Enum:
	    return "Enum";
	case TypeDecl::TK_Pointer:
	    return "Pointer";
	case TypeDecl::TK_Void:
	    return "Void";
	case TypeDecl::TK_Field:
	    return "Field";
	case TypeDecl::TK_File:
	    return "File";
	case TypeDecl::TK_String:
	    return "String";
	case TypeDecl::TK_Variant:
	    return "Variant";
	case TypeDecl::TK_FuncPtr:
	    return "FuncPtr";
	case TypeDecl::TK_Class:
	    return "Class";
	case TypeDecl::TK_MemberFunc:
	    return "MemberFunc";
	default:
	    break;
	}
	return "Unknown";
    }

    size_t TypeDecl::Size() const
    {
	const llvm::DataLayout dl(theModule);
	return dl.getTypeAllocSize(LlvmType());
    }

    size_t TypeDecl::AlignSize() const
    {
	const llvm::DataLayout dl(theModule);
	return dl.getPrefTypeAlignment(LlvmType());
    }

    Range* TypeDecl::GetRange() const
    {
	assert(isIntegral());
	switch(kind)
	{
	case TK_Char:
	    return new Range(0, UCHAR_MAX);
	case TK_Integer:
	    return new Range(INT_MIN, INT_MAX);
	default:
	    return 0;
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
	DoDump(std::cerr);
    }

    void BasicTypeDecl::DoDump(std::ostream& out) const
    {
	out << "Type: " << TypeToStr(kind);
    }

    llvm::Type* TypeDecl::LlvmType() const
    {
	if (!ltype)
	{
	    ltype = GetLlvmType();
	}
	return ltype;
    }

    llvm::Type* CharDecl::GetLlvmType() const
    {
	return llvm::Type::getInt8Ty(llvm::getGlobalContext());
    }

    const TypeDecl* CharDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (*this == *ty)
	{
	    return this;
	}
	if (ty->Type() == TK_String)
	{
	    return ty;
	}
	return 0;
    }

    llvm::Type* IntegerDecl::GetLlvmType() const
    {
	return llvm::Type::getInt32Ty(llvm::getGlobalContext());
    }

    const TypeDecl* IntegerDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (ty->Type() == TK_Integer)
	{
	    return this;
	}
	if (ty->Type() == TK_Int64 || ty->Type() == TK_Real)
	{
	    return ty;
	}
	return 0;
    }

    const TypeDecl* IntegerDecl::AssignableType(const TypeDecl* ty) const
    {
	if (SameAs(ty))
	{
	    return ty;
	}
	return 0;
    }

    llvm::Type* Int64Decl::GetLlvmType() const
    {
	return llvm::Type::getInt64Ty(llvm::getGlobalContext());
    }

    const TypeDecl* Int64Decl::CompatibleType(const TypeDecl* ty) const
    {
	if (ty->Type() == TK_Int64 || ty->Type() == TK_Integer)
	{
	    return this;
	}
	if (ty->Type() == TK_Real)
	{
	    return ty;
	}
	return 0;
    }

    const TypeDecl* Int64Decl::AssignableType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == TK_Integer)
	{
	    return this;
	}
	return 0;
    }

    llvm::Type* RealDecl::GetLlvmType() const
    {
	return llvm::Type::getDoubleTy(llvm::getGlobalContext());
    }

    const TypeDecl* RealDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == TK_Int64 || ty->Type() == TK_Integer)
	{
	    return this;
	}
	return 0;
    }

    const TypeDecl* RealDecl::AssignableType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == TK_Integer || ty->Type() == TK_Int64)
	{
	    return this;
	}
	return 0;
    }

    llvm::Type* VoidDecl::GetLlvmType() const
    {
	return llvm::Type::getVoidTy(llvm::getGlobalContext());
    }

    llvm::Type* BoolDecl::GetLlvmType() const
    {
	return llvm::Type::getInt1Ty(llvm::getGlobalContext());
    }

    void PointerDecl::DoDump(std::ostream& out) const
    {
	out << "Pointer to: " << name << " (" << baseType << ")" << std::endl;
    }

    bool CompoundDecl::classof(const TypeDecl* e)
    {
	switch(e->getKind())
	{
	case TK_Array:
	case TK_String:
	case TK_Pointer:
	case TK_Field:
	case TK_FuncPtr:
	case TK_File:
	case TK_Set:
	    return true;
	default:
	    break;
	}
	return false;
    }

    bool CompoundDecl::SameAs(const TypeDecl* ty) const
    {
        // Both need to be pointers!
	if (Type() != ty->Type())
	{
	    return false;
	}
	const CompoundDecl* cty = llvm::dyn_cast<CompoundDecl>(ty);
        return cty && *cty->SubType() == *baseType;
    }

    llvm::Type* PointerDecl::GetLlvmType() const
    {
	llvm::Type* ty = llvm::PointerType::getUnqual(baseType->LlvmType());
	return ty;
    }

    void ArrayDecl::DoDump(std::ostream& out) const
    {
	out << "Array ";
	for(auto r : ranges)
	{
	    r->DoDump(out);
	}
	out << " of ";
	baseType->DoDump(out);
    }

    llvm::Type* ArrayDecl::GetLlvmType() const
    {
	assert(ranges.size() && "Expect ranges to contain something");
	size_t nelems = 1;
	for(auto r : ranges)
	{
	    assert(r->GetRange()->Size() && "Expectig range to have a non-zero size!");
	    nelems *= r->GetRange()->Size();
	}
	assert(nelems && "Expect number of elements to be non-zero!");
	llvm::Type* ty = baseType->LlvmType();
	assert(ty && "Expected to get a type back!");
	return llvm::ArrayType::get(ty, nelems);
    }

    bool ArrayDecl::SameAs(const TypeDecl* ty) const
    {
	if (!CompoundDecl::SameAs(ty))
	{
	    return false;
	}

	if (const ArrayDecl* aty = llvm::dyn_cast<ArrayDecl>(ty))
	{
	    if (ranges.size() != aty->Ranges().size())
	    {
		return false;
	    }
	    for(size_t i = 0; i < ranges.size(); i++)
	    {
		if (*ranges[i] != *aty->Ranges()[i])
		{
		    return false;
		}
	    }

	    return true;
	}
	return false;
    }

    bool SimpleCompoundDecl::classof(const TypeDecl* e)
    {
	switch(e->getKind())
	{
	case TK_Range:
	case TK_Enum:
	    return true;
	default:
	    break;
	}
	return false;
    }

    llvm::Type* SimpleCompoundDecl::GetLlvmType() const
    {
	return GetType(baseType);
    }

    bool SimpleCompoundDecl::SameAs(const TypeDecl* ty) const
    {
	if (Type() != ty->Type())
	{
	    return false;
	}
	if (const SimpleCompoundDecl* sty = llvm::dyn_cast<SimpleCompoundDecl>(ty))
	{
	    if (sty->baseType != baseType)
	    {
		return false;
	    }
	    return true;
	}
	return false;
    }

    void Range::dump() const
    {
	DoDump(std::cerr);
    }

    void Range::DoDump(std::ostream& out) const
    {
	out << "[" << start << ".." << end << "]";
    }

    void RangeDecl::DoDump(std::ostream& out) const
    {
	out << "RangeDecl: " << TypeToStr(baseType) << " ";
	range->DoDump(out);
	out << std::endl;
    }

    bool RangeDecl::SameAs(const TypeDecl* ty) const
    {
	if (const RangeDecl* rty = llvm::dyn_cast<RangeDecl>(ty))
	{
	    if (rty->Type() != Type() || *range != *rty->range)
	    {
		return false;
	    }
	    return true;
	}
	return false;
    }

    const TypeDecl* RangeDecl::CompatibleType(const TypeDecl* ty) const
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

    const TypeDecl* RangeDecl::AssignableType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == Type())
	{
	    return ty;
	}
	return 0;
    }

    unsigned RangeDecl::Bits() const
    {
	unsigned s = range->Size();
	unsigned b = 1;
	while(s < (1u << b))
	{
	    b++;
	}
	return b;
    }


    void EnumDecl::SetValues(const std::vector<std::string>& nmv)
    {
	unsigned int v = 0;
	for(auto n : nmv)
	{
	    EnumValue e(n, v);
	    values.push_back(e);
	    v++;
	}
    }

    void EnumDecl::DoDump(std::ostream& out) const
    {
	out << "EnumDecl:";
	for(auto v : values)
	{
	    out << "   " << v.name << ": " << v.value;
	}
    }

    bool EnumDecl::SameAs(const TypeDecl* ty) const
    {
	if (const EnumDecl* ety = llvm::dyn_cast<EnumDecl>(ty))
	{
	    if (ety->Type() != Type() || values.size() != ety->values.size())
	    {
		return false;
	    }
	    for(size_t i = 0; i < values.size(); i++)
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

    unsigned EnumDecl::Bits() const
    {
	unsigned s = values.size();
	unsigned b = 1;
	while(s < (1u << b))
	{
	    b++;
	}
	return b;
    }

    void FunctionDecl::DoDump(std::ostream& out) const
    {
	out << "Function " << baseType << std::endl;
    }

    void FieldDecl::DoDump(std::ostream& out) const
    {
	out << "Field " << name << ": ";
	baseType->DoDump(out);
    }

    llvm::Type* FieldDecl::GetLlvmType() const
    {
	return baseType->LlvmType();
    }

    void VariantDecl::DoDump(std::ostream& out) const
    {
	out << "Variant ";
	for(auto f : fields)
	{
	    f->DoDump(out);
	    std::cerr << std::endl;
	}
    }

    llvm::Type* VariantDecl::GetLlvmType() const
    {
	const llvm::DataLayout dl(theModule);
	size_t maxSize = 0;
	size_t maxSizeElt = 0;
	size_t maxAlign = 0;
	size_t maxAlignElt = 0;
	size_t maxAlignSize = 0;
	size_t elt = 0;
	for(auto f : fields)
	{
	    llvm::Type* ty = f->LlvmType();
	    if (llvm::isa<PointerDecl>(f->FieldType()) && !f->hasLlvmType())
	    {
		if (!opaqueType)
		{
		    opaqueType = llvm::StructType::create(llvm::getGlobalContext());
		}
		return opaqueType;
	    }
	    size_t sz = dl.getTypeAllocSize(ty);
	    size_t al = dl.getPrefTypeAlignment(ty);
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

	llvm::Type* ty = fields[maxAlignElt]->LlvmType();
	std::vector<llvm::Type*> fv{ty};
	if (maxAlignElt != maxSizeElt)
	{
	    size_t nelems = maxSize - maxAlignSize;
	    llvm::Type* ty = llvm::ArrayType::get(GetType(TK_Char), nelems);
	    fv.push_back(ty);
	}
	return llvm::StructType::create(fv);
    }

    void FieldCollection::EnsureSized() const
    {
	if (opaqueType && opaqueType->isOpaque())
	{
	    llvm::Type* ty = GetLlvmType();
	    assert(ty == opaqueType && "Expect opaqueType to be returned");
	    assert(!opaqueType->isOpaque() && "Expect opaqueness to have gone away");
	}
    }

// This is a very basic algorithm, but I think it's good enough for
// most structures - there's unlikely to be a HUGE number of them.
    int FieldCollection::Element(const std::string& name) const
    {
	int i = 0;
	for(auto f : fields)
	{
	    // Check for special record type
	    if (f->Name() == "")
	    {
		RecordDecl* rd = llvm::dyn_cast<RecordDecl>(f->FieldType());
		assert(rd && "Expected record declarataion here!");
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

	if (const FieldCollection* fty = llvm::dyn_cast<FieldCollection>(ty))
	{
	    if (fields.size() != fty->fields.size())
	    {
		return false;
	    }
	    for(size_t i = 0; i < fields.size(); i++)
	    {
		if(fields[i] != fty->fields[i])
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

    void RecordDecl::DoDump(std::ostream& out) const
    {
	out << "Record ";
	for(auto f : fields)
	{
	    f->DoDump(out);
	    out << std::endl;
	}
	if (variant)
	{
	    variant->DoDump(out);
	}
    }

    llvm::Type* RecordDecl::GetLlvmType() const
    {
	std::vector<llvm::Type*> fv;
	for(auto f : fields)
	{
	    if (llvm::isa<PointerDecl>(f->FieldType()) && !f->FieldType()->hasLlvmType())
	    {
		if (!opaqueType)
		{
		    opaqueType = llvm::StructType::create(llvm::getGlobalContext());
		}
		return opaqueType;
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
	return llvm::StructType::create(fv);
    }

    bool RecordDecl::SameAs(const TypeDecl* ty) const
    {
	return this == ty;
    }


    ClassDecl::ClassDecl(const std::string& nm, const std::vector<FieldDecl*>& flds, 
			 const std::vector<MemberFuncDecl*> mf, VariantDecl* v, ClassDecl* base)
	: FieldCollection(TK_Class, flds), baseobj(base), name(nm), variant(v), vtableType(0)
    { 
	if (baseobj)
	{
	    membfuncs = baseobj->membfuncs;
	}

	std::vector<VarDef> self = {VarDef("self", this, true)};
	for(auto i : mf)
	{
	    if (!i->IsStatic())
	    {
		i->Proto()->AddExtraArgsFirst(self);
		i->Proto()->SetHasSelf(true);
	    }
	    i->LongName(name + "$" + i->Proto()->Name());
	    bool found = false;
	    for(auto& j : membfuncs)
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

    void ClassDecl::DoDump(std::ostream& out) const
    {
	out << "Object: " << Name();
	for(auto f : fields)
	{
	    f->DoDump(out);
	    out << std::endl;
	}
	if (variant)
	{
	    variant->DoDump(out);
	}
	out << std::endl;
    }

    size_t ClassDecl::MembFuncCount() const
    {
	return membfuncs.size();
    }

    int ClassDecl::MembFunc(const std::string& nm) const
    {
	for(size_t i = 0; i < membfuncs.size();  i++)
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
	assert(index < membfuncs.size() && "Expected index to be in range");
	return membfuncs[index];
    }

    size_t ClassDecl::NumVirtFuncs() const
    {
	size_t count = 0;
	for(auto mf : membfuncs)
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
	    (void) baseobj->VTableType(opaque);
	}

	std::vector<llvm::Type*> vt;
	bool needed = false;
	int index =  0;
	for(auto m : membfuncs)
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
		    return ErrorT("Overriding function " + m->Proto()->Name() +
				  " that is not a virtual function in the baseclass!");
		}
		/* We need to continue digging here for multi-level functions */
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
	    vtableType = llvm::StructType::create(llvm::getGlobalContext(), "vtable_" + Name());
	}
	if (!opaque)
	{
	    assert(vt.size() && "Expected some functions here...");
	    vtableType->setBody(vt);
	}
	return vtableType;
    }

    int ClassDecl::Element(const std::string& name) const
    {
	int b = baseobj ? baseobj->FieldCount() : 0;
	/* Shadowing overrides outer elemnts */
	int elem = FieldCollection::Element(name);
	if (elem >= 0)
	{
	    return elem+b;
	}
	return (baseobj) ? baseobj->Element(name) : -1;
    }

    const FieldDecl* ClassDecl::GetElement(unsigned int n, std::string& objname) const
    {
	int b = baseobj ? baseobj->FieldCount() : 0;
	if (n < (unsigned)b)
	{
	    return baseobj->GetElement(n, objname);
	}
	assert(n < b + fields.size() && "Out of range field");
	objname = Name();
	return fields[n - b];
    }

    const FieldDecl* ClassDecl::GetElement(unsigned int n) const
    {
	std::string objname;
	return GetElement(n, objname);
    }

    int ClassDecl::FieldCount() const
    {
	return fields.size() + (baseobj ? baseobj->FieldCount(): 0);
    }
    
    const TypeDecl* ClassDecl::CompatibleType(const TypeDecl *ty) const
    {
	if (*ty == *this)
	{
	    return this;
	}
	if (const ClassDecl* cd = llvm::dyn_cast<ClassDecl>(ty))
	{
	    return (cd->baseobj) ? CompatibleType(cd->baseobj) : 0;
	}
	return 0;
    }

    llvm::Type* ClassDecl::GetLlvmType() const
    {
	std::vector<llvm::Type*> fv;

	if (VTableType(true))
	{
	    fv.push_back(llvm::PointerType::getUnqual(vtableType));
	}
	
	int fc = FieldCount();
	for(int i = 0; i < fc; i++)
	{
	    const FieldDecl* f = GetElement(i);

	    assert(!llvm::isa<MemberFuncDecl>(f->FieldType()) && "Should not have member functions now");

	    if (!f->IsStatic())
	    {
		if (PointerDecl* pd = llvm::dyn_cast<PointerDecl>(f->FieldType()))
		{
		    if (pd->IsIncomplete() && !pd->hasLlvmType())
		    {
			if (!opaqueType)
			{
			    opaqueType = llvm::StructType::create(llvm::getGlobalContext(), Name());
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
	    llvm::StructType* ty = llvm::StructType::create(llvm::getGlobalContext(), Name());
	    ty->setBody(llvm::None);
	    return ty;
	}
	return llvm::StructType::create(fv, Name());
    }

    bool ClassDecl::SameAs(const TypeDecl* ty) const
    {
	return this == ty;
    }

    void MemberFuncDecl::DoDump(std::ostream& out) const
    {
	out << "Member function "; proto->dump(out);
    }

    bool MemberFuncDecl::SameAs(const TypeDecl* ty) const
    {
	return this == ty;
    }

    void FuncPtrDecl::DoDump(std::ostream& out) const
    {
	out << "FunctionPtr " << std::endl;
    }

    llvm::Type* FuncPtrDecl::GetLlvmType() const
    {
	llvm::Type* resty = proto->Type()->LlvmType();
	std::vector<llvm::Type*> argTys;
	for(auto v : proto->Args())
	{
	    llvm::Type* ty = v.Type()->LlvmType();
	    if (v.IsRef() || v.Type()->isCompound() )
	    {
		ty = llvm::PointerType::getUnqual(ty);
	    }
	    argTys.push_back(ty);
	}
	llvm::Type*  ty = llvm::FunctionType::get(resty, argTys, false);
	return llvm::PointerType::getUnqual(ty);
    }

    FuncPtrDecl::FuncPtrDecl(PrototypeAST* func)
	: CompoundDecl(TK_FuncPtr, 0), proto(func)
    {
	baseType = new FunctionDecl(proto->Type());
    }

    bool FuncPtrDecl::SameAs(const TypeDecl* ty) const
    {
	if (!CompoundDecl::SameAs(ty))
	{
	    return false;
	}
	if (const FuncPtrDecl* fty = llvm::dyn_cast<FuncPtrDecl>(ty))
	{
	    return proto == fty->proto;
	}
	return false;
    }

/*
 * A "file" is represented by:
 * struct
 * {
 *    int32     handle;          // 0: filehandle
 *    baseType *ptr;             // 1: pointer to the record.
 * };
 *
 * The translation from handle to actual file is done inside the C runtime
 * part.
 *
 * Note that this arrangement has to agree with the runtime.c definition.
 *
 * The type name is used to determine if the file is a "text" or "file of TYPE" type.
 */
    llvm::Type* GetFileType(const std::string& name, TypeDecl* baseType)
    {
	llvm::Type* ty = llvm::PointerType::getUnqual(baseType->LlvmType());
	std::vector<llvm::Type*> fv{GetType(TypeDecl::TK_Integer), ty};
	llvm::StructType* st = llvm::StructType::create(fv, name);
	return st;
    }

    llvm::Type* FileDecl::GetLlvmType() const
    {
	return GetFileType("file", baseType);
    }

    void FileDecl::DoDump(std::ostream& out) const
    {
	out << "File of ";
	baseType->DoDump(out);
    }

    llvm::Type* TextDecl::GetLlvmType() const
    {
	return GetFileType("text", baseType);
    }

    void TextDecl::DoDump(std::ostream& out) const
    {
	out << "Text ";
    }

    SetDecl::SetDecl(RangeDecl* r, TypeDecl* ty)
	: CompoundDecl(TK_Set, ty), range(r)
    {
	assert(sizeof(ElemType) * CHAR_BIT == SetBits && "Set bits mismatch");
	assert(1 << SetPow2Bits == SetBits && "Set pow2 mismatch");
	assert(SetMask == SetBits-1 && "Set pow2 mismatch");
	if (r)
	{
	    assert(r->GetRange()->Size() <= MaxSetSize && "Set too large");
	}
    }

    llvm::Type* SetDecl::GetLlvmType() const
    {
	assert(range);
	assert(range->GetRange()->Size() <= MaxSetSize && "Set too large");
	llvm::IntegerType* ity = llvm::dyn_cast<llvm::IntegerType>(GetType(TK_Integer));
	llvm::Type* ty = llvm::ArrayType::get(ity, SetWords());
	return ty;
    }

    void SetDecl::DoDump(std::ostream& out) const
    {
	out << "Set of ";
	if (!range)
	{
	    out << "[Unknown]";
	}
	else
	{
	    range->DoDump(out);
	}
	out << std::endl;
    }

    void SetDecl::UpdateSubtype(TypeDecl* ty)
    {
	assert(!baseType && "Expected to not have a subtype yet...");
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

    void StringDecl::DoDump(std::ostream& out) const
    {
	out << "String[";
	Ranges()[0]->DoDump(out);
	out << "]";
    }

    const TypeDecl* StringDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == TK_Char)
	{
	    return this;
	}
	if (ty->Type() == TK_String)
	{
	    if (llvm::dyn_cast<StringDecl>(ty)->Ranges()[0]->GetEnd() > 
		Ranges()[0]->GetEnd())
	    {
		return ty;
	    }
	    return this;
	}
	if (ty->Type() == TK_Array)
	{
	    if (const ArrayDecl* aty = llvm::dyn_cast<ArrayDecl>(ty))
	    {
		if (aty->Ranges().size() != 1)
		{
		    return 0;
		}
		return this;
	    }
	}
	return 0;
    }

    bool SetDecl::SameAs(const TypeDecl* ty) const
    {
	if (!CompoundDecl::SameAs(ty))
	{
	    return false;
	}

	if (const SetDecl* sty = llvm::dyn_cast<SetDecl>(ty))
	{
	    if (*range != *sty->range)
	    {
		return false;
	    }
	    return true;
	}
	return false;
    }

// Void pointer is not a "pointer to void", but a "pointer to Int8".
    llvm::Type* GetVoidPtrType()
    {
	llvm::Type* base = llvm::IntegerType::getInt8Ty(llvm::getGlobalContext());
	return llvm::PointerType::getUnqual(base);
    }

    TypeDecl* GetVoidType()
    {
	if (!voidType)
	{
	    voidType = new VoidDecl;
	}
	return voidType;
    }

    StringDecl* GetStringType()
    {
	if (!strType)
	{
	    strType = new StringDecl(255);
	}
	return strType;
    }

    TextDecl* GetTextType()
    {
	if (!textType)
	{
	    textType = new TextDecl();
	}
	return textType;
    }
}

bool operator==(const Types::TypeDecl& lty, const Types::TypeDecl& rty)
{
    return lty.SameAs(&rty);
}

bool operator==(const Types::Range& a, const Types::Range& b)
{
    return (a.GetStart() == b.GetStart() && a.GetEnd() == b.GetEnd());
}

bool operator==(const Types::EnumValue& a, const Types::EnumValue& b)
{
    return (a.value == b.value && a.name == b.name);
}
