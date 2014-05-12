#include "types.h"
#include "expr.h"
#include <llvm/IR/LLVMContext.h>
#include <sstream>
#include <climits>

#define TRACE() std::cerr << __FILE__ << ":" << __LINE__ << "::" << __PRETTY_FUNCTION__ << std::endl

extern llvm::Module* theModule;

/* Static variables in Types. */

Types::TypeDecl* Types::voidType = 0;
Types::TypeDecl* Types::setType = 0;
Types::StringDecl* Types::strType = 0;

llvm::Type* ErrorT(const std::string& msg)
{
    std::cerr << msg << std::endl;
    return 0;
}

llvm::Type* Types::GetType(Types::SimpleTypes type)
{
    switch(type)
    {
    case Types::Integer:
	return llvm::Type::getInt32Ty(llvm::getGlobalContext());

    case Types::Int64:
	return llvm::Type::getInt64Ty(llvm::getGlobalContext());

    case Types::Real:
	return llvm::Type::getDoubleTy(llvm::getGlobalContext());

    case Types::Char:
	return llvm::Type::getInt8Ty(llvm::getGlobalContext());

    case Types::Boolean:
	return llvm::Type::getInt1Ty(llvm::getGlobalContext());

    case Types::Void:
	return llvm::Type::getVoidTy(llvm::getGlobalContext());
	
    default:
	break;
    }
    return 0;
}

std::string Types::TypeDecl::to_string() const
{
    std::stringstream ss; 
    ss << "Type: " << (int)type << std::endl;
    return ss.str();
}

bool Types::TypeDecl::isIntegral() const
{
    switch(type)
    {
    case Integer:
    case Char:
	return true;
    default:
	return true;
    }
}

size_t Types::TypeDecl::Size() const
{
    const llvm::DataLayout dl(theModule);
    llvm::Type* ty = LlvmType();
    return dl.getTypeAllocSize(ty);
}

Types::Range* Types::TypeDecl::GetRange() const
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

static const char* TypeToStr(Types::SimpleTypes t)
{
    switch(t)
    {
    case Types::Integer:
	return "Integer";
    case Types::Int64:
	return "Int64";
    case Types::Real:
	return "Real";
    case Types::Char:
	return "Char";
    case Types::Boolean:
	return "Boolean";
    case Types::Array:
	return "Array";
    case Types::Function:
	return "Function";
    case Types::Procedure:
	return "Procedure";
    case Types::Record:
	return "Record";
    case Types::Set:
	return "Set";
    case Types::SubRange:
	return "SubRange";
    case Types::Enum:
	return "Enum";
    case Types::Pointer:
	return "Pointer";
    case Types::PointerIncomplete:
	return "PointerIncomplete";
    case Types::Void:
	return "Void";
    case Types::Field:
	return "Field";
    case Types::File:
	return "File";
    case Types::String:
	return "String";
    case Types::Variant:
	return "Variant";
    }
}

void Types::TypeDecl::dump() const
{
    std::cerr << "Type: " << TypeToStr(type);
}

llvm::Type* Types::TypeDecl::LlvmType() const
{
    if (!ltype)
    {
	ltype = GetLlvmType();
    }
    return ltype;
}

llvm::Type* Types::TypeDecl::GetLlvmType() const
{
    return GetType(type);
}

void Types::PointerDecl::dump() const
{
    std::cerr << "Pointer to: ";
    baseType->dump();
}

llvm::Type* Types::PointerDecl::GetLlvmType() const
{
    llvm::Type* ty = llvm::PointerType::getUnqual(baseType->LlvmType());
    return ty;
}

void Types::ArrayDecl::dump() const
{
    std::cerr << "Array ";
    for(auto r : ranges)
    {
	r->dump();
    }
    std::cerr << " of "; 
    baseType->dump();
}

llvm::Type* Types::ArrayDecl::GetLlvmType() const
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

void Types::Range::dump() const
{
    std::cerr << "[" << start << ".." << end << "]";
}

void Types::RangeDecl::dump() const
{
    std::cerr << "RangeDecl: " << TypeToStr(baseType) << " " << range << std::endl;
}

llvm::Type* Types::RangeDecl::GetLlvmType() const
{
    return GetType(baseType);
}

void Types::EnumDecl::SetValues(const std::vector<std::string>& nmv)
{
    unsigned int v = 0;
    for(auto n : nmv)
    {
	EnumValue e(n, v);
	values.push_back(e);
	v++;
    }
}

void Types::EnumDecl::dump() const
{
    std::cerr << "EnumDecl:";
    for(auto v : values)
    {
	std::cerr << "   " << v.name << ": " << v.value;
    }
}

llvm::Type* Types::EnumDecl::GetLlvmType() const
{
    return GetType(subType);
}

void Types::FieldDecl::dump() const
{
    std::cerr << "Field " << name << ": ";
    baseType->dump();
}

llvm::Type* Types::FieldDecl::GetLlvmType() const
{
    return baseType->LlvmType();
}

void Types::VariantDecl::dump() const
{
    std::cerr << "Variant ";
    for(auto f : fields)
    {
	f.dump();
	std::cerr << std::endl;
    }
}

llvm::Type* Types::VariantDecl::GetLlvmType() const
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

    std::vector<llvm::Type*> fv;
    llvm::Type* ty = fields[maxAlignElt].LlvmType();
    fv.push_back(ty);
    if (maxAlignElt != maxSizeElt)
    {
	size_t nelems = maxSize - maxAlignSize;
	llvm::Type* ty = llvm::ArrayType::get(Types::GetType(Types::Char), nelems);
	fv.push_back(ty);
    }
    return llvm::StructType::create(fv);
}

void Types::FieldCollection::EnsureSized() const
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
int Types::FieldCollection::Element(const std::string& name) const
{
    int i = 0;
    for(auto f : fields)
    {
	// Check for special record type 
	if (f.Name() == "")
	{
	    Types::RecordDecl* rd = llvm::dyn_cast<Types::RecordDecl>(f.FieldType());
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

size_t Types::RecordDecl::Size() const
{
    EnsureSized();
    return TypeDecl::Size();
}

void Types::RecordDecl::dump() const
{
    std::cerr << "Record ";
    for(auto f : fields)
    {
	f.dump();
	std::cerr << std::endl;
    }
    if (variant)
    {
	variant->dump();
    }
}

llvm::Type* Types::RecordDecl::GetLlvmType() const
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

void Types::FuncPtrDecl::dump() const
{
    std::cerr << "FunctionPtr " << std::endl;
}

llvm::Type* Types::FuncPtrDecl::GetLlvmType() const
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

Types::FuncPtrDecl::FuncPtrDecl(PrototypeAST* func)
    : TypeDecl(Pointer), proto(func) 
{
    Types::SimpleTypes t = Types::Function;
    if (proto->Type()->Type() == Types::Void)
    {
	t = Types::Procedure;
    }
    baseType = new TypeDecl(t);
}

/*
 * A "file" is represented by:
 * struct 
 * { 
 *    int32     handle;          // 0: filehandle
 *    baseType *ptr;             // 1: pointer to the record.
 * }; 
 * 
 * The translation from handle to actual file is 
 * done inside the C runtime part. 
 * 
 * Note that this arrangement has to agree with the runtime.c definition.
 *
 * The type name is used to determine if the file is a "text" or "file of TYPE" type.
 */
llvm::Type* Types::GetFileType(const std::string& name, Types::TypeDecl* baseType)
{
    std::vector<llvm::Type*> fv;
    fv.push_back(Types::GetType(Types::Integer));
    llvm::Type* ty = llvm::PointerType::getUnqual(baseType->LlvmType());
    fv.push_back(ty);
    llvm::StructType* st = llvm::StructType::create(fv);
    st->setName(name);
    return st;
}

llvm::Type* Types::FileDecl::GetLlvmType() const
{
    return GetFileType("file", baseType);
}

void Types::FileDecl::dump() const
{
    std::cerr << "File of ";
    baseType->dump();
}

llvm::Type* Types::TextDecl::GetLlvmType() const
{
    return GetFileType("text", baseType);
}

void Types::TextDecl::dump() const
{
    std::cerr << "Text ";
}

void Types::SetDecl::dump() const
{
    std::cerr << "Set of ";
    range->dump();
}

Types::TypeDecl* Types::TypeForSet()
{
    if (!setType)
    {
	Types::Range *r = new Types::Range(0,32 * Types::SetDecl::MaxSetWords);
	setType = new Types::SetDecl(r);
    }
    return setType;
}

llvm::Type* Types::SetDecl::GetLlvmType() const
{
    llvm::IntegerType* ity = llvm::dyn_cast<llvm::IntegerType>(GetType(Types::Integer));
    size_t nelems = MaxSetWords;
    llvm::Type* ty = llvm::ArrayType::get(ity, nelems);
    return ty;
}

// Void pointer is not a "pointer to void", but a "pointer to Int8". 
llvm::Type* Types::GetVoidPtrType()
{
    llvm::Type* base = llvm::IntegerType::getInt8Ty(llvm::getGlobalContext());
    return llvm::PointerType::getUnqual(base);
}

Types::TypeDecl* Types::GetVoidType()
{
    if (!voidType)
    {
	voidType = new TypeDecl(Types::Void);
    }
    return voidType;
}

Types::StringDecl* Types::GetStringType()
{
    if (!strType)
    {
	strType = new Types::StringDecl(255);
    }
    return strType;
}
