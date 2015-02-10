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

    llvm::Type* GetType(SimpleTypes type)
    {
	switch(type)
	{
	case Integer:
	    return llvm::Type::getInt32Ty(llvm::getGlobalContext());

	case Int64:
	    return llvm::Type::getInt64Ty(llvm::getGlobalContext());

	case Real:
	    return llvm::Type::getDoubleTy(llvm::getGlobalContext());

	case Char:
	    return llvm::Type::getInt8Ty(llvm::getGlobalContext());

	case Boolean:
	    return llvm::Type::getInt1Ty(llvm::getGlobalContext());

	case Void:
	    return llvm::Type::getVoidTy(llvm::getGlobalContext());

	default:
	    assert(0 && "Not a known type...");
	    break;
	}
	return 0;
    }

    std::string TypeDecl::to_string() const
    {
	std::stringstream ss;
	ss << "Type: " << (int)type << std::endl;
	return ss.str();
    }

    bool TypeDecl::isIntegral() const
    {
	switch(type)
	{
	case Integer:
	case Int64:
	case Char:
	    return true;
	default:
	    return false;
	}
    }

    size_t TypeDecl::Size() const
    {
	const llvm::DataLayout dl(theModule);
	llvm::Type* ty = LlvmType();
	return dl.getTypeAllocSize(ty);
    }

    Range* TypeDecl::GetRange() const
    {
	assert(isIntegral());
	switch(type)
	{
	case Char:
	    return new Range(0, UCHAR_MAX);
	case Integer:
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

    static const char* TypeToStr(SimpleTypes t)
    {
	switch(t)
	{
	case Integer:
	    return "Integer";
	case Int64:
	    return "Int64";
	case Real:
	    return "Real";
	case Char:
	    return "Char";
	case Boolean:
	    return "Boolean";
	case Array:
	    return "Array";
	case Function:
	    return "Function";
	case Procedure:
	    return "Procedure";
	case Record:
	    return "Record";
	case Set:
	    return "Set";
	case SubRange:
	    return "SubRange";
	case Enum:
	    return "Enum";
	case Pointer:
	    return "Pointer";
	case PointerIncomplete:
	    return "PointerIncomplete";
	case Void:
	    return "Void";
	case Field:
	    return "Field";
	case File:
	    return "File";
	case String:
	    return "String";
	case Variant:
	    return "Variant";
	case FuncPtr:
	    return "FuncPtr";
	}
    }

    void TypeDecl::dump() const
    {
	DoDump(std::cerr);
    }

    void BasicTypeDecl::DoDump(std::ostream& out) const
    {
	out << "Type: " << TypeToStr(type);
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
	if (ty->Type() == String)
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
	if (ty->Type() == Integer)
	{
	    return this;
	}
	if (ty->Type() == Int64 || ty->Type() == Real)
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
	if (ty->Type() == Int64 || ty->Type() == Integer)
	{
	    return this;
	}
	if (ty->Type() == Real)
	{
	    return ty;
	}
	return 0;
    }

    const TypeDecl* Int64Decl::AssignableType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == Integer)
	{
	    return ty;
	}
	return 0;
    }

    llvm::Type* RealDecl::GetLlvmType() const
    {
	return llvm::Type::getDoubleTy(llvm::getGlobalContext());
    }

    const TypeDecl* RealDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == Int64 || ty->Type() == Integer)
	{
	    return this;
	}
	return 0;
    }

    const TypeDecl* RealDecl::AssignableType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == Integer || ty->Type() == Int64)
	{
	    return ty;
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
	if (type != ty->Type())
	{
	    return false;
	}
	const CompoundDecl* cty = llvm::dyn_cast<CompoundDecl>(ty);
	if (*cty->SubType() != *baseType)
	{
	    return false;
	}
	return true;
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
	size_t nelems = 0;
	for(auto r : ranges)
	{
	    assert(r->Size() && "Expectig range to have a non-zero size!");
	    if (!nelems)
	    {
		nelems = r->Size();
	    }
	    else
	    {
		nelems *= r->Size();
	    }
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

	const ArrayDecl* aty = llvm::dyn_cast<ArrayDecl>(ty);
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
	if (type != ty->Type())
	{
	    return false;
	}
	const SimpleCompoundDecl* sty = llvm::dyn_cast<SimpleCompoundDecl>(ty);

	if (sty->baseType != baseType)
	{
	    return false;
	}
	return true;
    }

    void Range::DoDump(std::ostream& out) const
    {
	out << "[" << start << ".." << end << "]";
    }

    void RangeDecl::DoDump(std::ostream& out) const
    {
	out << "RangeDecl: " << TypeToStr(baseType) << " " << range << std::endl;
    }

    bool RangeDecl::SameAs(const TypeDecl* ty) const
    {
	if (const RangeDecl* rty = llvm::dyn_cast<RangeDecl>(ty))
	{
	    if (rty->type != type)
	    {
		return false;
	    }
	    if (*range != *rty->range)
	    {
		return false;
	    }
	}
	else
	{
	    return false;
	}
	return true;
    }

    const TypeDecl* RangeDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (*this == *ty)
	{
	    return this;
	}
	if (ty->Type() == Integer || ty->Type() == Int64|| ty->Type() == Real)
	{
	    return ty;
	}
	return 0;
    }

    const TypeDecl* RangeDecl::AssignableType(const TypeDecl* ty) const
    {
	if (SameAs(ty))
	{
	    return ty;
	}
	return 0;
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
	    if (ety->type != type)
	    {
		return false;
	    }
	    if (values.size() != ety->values.size())
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
	    f.DoDump(out);
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
	    llvm::Type* ty = f.LlvmType();
	    if (llvm::isa<PointerDecl>(f.FieldType()) && !f.hasLlvmType())
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

	llvm::Type* ty = fields[maxAlignElt].LlvmType();
	std::vector<llvm::Type*> fv{ty};
	if (maxAlignElt != maxSizeElt)
	{
	    size_t nelems = maxSize - maxAlignSize;
	    llvm::Type* ty = llvm::ArrayType::get(GetType(Char), nelems);
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
	    if (f.Name() == "")
	    {
		RecordDecl* rd = llvm::dyn_cast<RecordDecl>(f.FieldType());
		assert(rd && "Expected record declarataion here!");
		if (rd->Element(name) >= 0)
		{
		    return i;
		}
	    }
	    if (f.Name() == name)
	    {
		return i;
	    }
	    i++;
	}
	return -1;
    }

    bool FieldCollection::SameAs(const TypeDecl* ty) const
    {
	if (type != ty->Type())
	{
	    return false;
	}

	const FieldCollection* fty = llvm::dyn_cast<FieldCollection>(ty);
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
	    f.DoDump(out);
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
	    if (llvm::isa<PointerDecl>(f.FieldType()) && !f.FieldType()->hasLlvmType())
	    {
		if (!opaqueType)
		{
		    opaqueType = llvm::StructType::create(llvm::getGlobalContext());
		}
		return opaqueType;
	    }
	    fv.push_back(f.LlvmType());
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
	    argTys.push_back(v.Type()->LlvmType());
	}
	llvm::Type*  ty = llvm::FunctionType::get(resty, argTys, false);
	return llvm::PointerType::getUnqual(ty);
    }

    FuncPtrDecl::FuncPtrDecl(PrototypeAST* func)
	: CompoundDecl(TK_FuncPtr, FuncPtr, 0), proto(func)
    {
	SimpleTypes t = Function;
	if (proto->Type()->Type() == Void)
	{
	    t = Procedure;
	}
	baseType = new FunctionDecl(t, proto->Type());
    }

    bool FuncPtrDecl::SameAs(const TypeDecl* ty) const
    {
	if (!CompoundDecl::SameAs(ty))
	{
	    return false;
	}
	const FuncPtrDecl* fty = llvm::dyn_cast<FuncPtrDecl>(ty);
	return proto == fty->proto;
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
	std::vector<llvm::Type*> fv{GetType(Integer), ty};
	llvm::StructType* st = llvm::StructType::create(fv);
	st->setName(name);
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

    SetDecl::SetDecl(Range* r, TypeDecl* ty)
	: CompoundDecl(TK_Set, Set, ty), range(r)
    {
	assert(sizeof(ElemType) * CHAR_BIT == SetBits && "Set bits mismatch");
	assert(1 << SetPow2Bits == SetBits && "Set pow2 mismatch");
	assert(SetMask == SetBits-1 && "Set pow2 mismatch");
	if (r)
	{
	    assert(r->Size() <= MaxSetSize && "Set too large");
	}
    }

    llvm::Type* SetDecl::GetLlvmType() const
    {
	assert(range);
	assert(range->Size() <= MaxSetSize && "Set too large");
	llvm::IntegerType* ity = llvm::dyn_cast<llvm::IntegerType>(GetType(Integer));
	llvm::Type* ty = llvm::ArrayType::get(ity, SetWords());
	return ty;
    }

    void SetDecl::UpdateSubtype(TypeDecl* ty)
    {
	assert(!baseType && "Expected to not have a subtype yet...");
	baseType = ty;
    }

    void StringDecl::DoDump(std::ostream& out) const
    {
	out << "String[";
	Ranges()[0]->DoDump(out);
	out << "]";
    }

    const TypeDecl* StringDecl::CompatibleType(const TypeDecl* ty) const
    {
	if (SameAs(ty) || ty->Type() == Char)
	{
	    return this;
	}
	if (ty->Type() == Array)
	{
	    const ArrayDecl* aty = llvm::dyn_cast<ArrayDecl>(ty);
	    if (aty->Ranges().size() != 1)
	    {
		return 0;
	    }
	    return this;
	}
	return 0;
    }

    bool SetDecl::SameAs(const TypeDecl* ty) const
    {
	if (!CompoundDecl::SameAs(ty))
	{
	    return false;
	}

	const SetDecl* sty = llvm::dyn_cast<SetDecl>(ty);
	if (*range != *sty->range)
	{
	    return false;
	}
	return true;
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
};

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

